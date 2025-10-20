//================================================================================================
// Filename:      LightingCommon.hlsl
// Description:   Common lighting calculation functions
//                Supports GOURAUD, LAMBERT, PHONG lighting models via macros
//================================================================================================

// This file must be included AFTER:
// - LightStructures.hlsl
// - LightingBuffers.hlsl

//================================================================================================
// 유틸리티 함수
//================================================================================================

// 타일 인덱스 계산 (픽셀 위치로부터)
// SV_POSITION은 픽셀 중심 좌표 (0.5, 0.5 offset)
uint CalculateTileIndex(float4 screenPos)
{
    uint tileX = uint(screenPos.x) / TileSize;
    uint tileY = uint(screenPos.y) / TileSize;
    return tileY * TileCountX + tileX;
}

// 타일별 라이트 인덱스 데이터의 시작 오프셋 계산
uint GetTileDataOffset(uint tileIndex)
{
    // MaxLightsPerTile = 256 (TileLightCuller.h와 일치)
    const uint MaxLightsPerTile = 256;
    return tileIndex * MaxLightsPerTile;
}

//================================================================================================
// 기본 조명 계산 함수
//================================================================================================

// Ambient Light Calculation
// Uses the provided materialColor (which includes texture if available)
// Note: light.Color already includes Intensity and Temperature
float3 CalculateAmbientLight(FAmbientLightInfo light, float4 materialColor)
{
    // Use materialColor directly (already contains texture if available)
    return light.Color.rgb * materialColor.rgb;
}

// Diffuse Light Calculation (Lambert)
// Uses the provided materialColor (which includes texture if available)
// Note: lightColor already includes Intensity (calculated in C++)
float3 CalculateDiffuse(float3 lightDir, float3 normal, float4 lightColor, float4 materialColor)
{
    float NdotL = max(dot(normal, lightDir), 0.0f);
    // Use materialColor directly (already contains texture if available)
    return lightColor.rgb * materialColor.rgb * NdotL;
}

// Specular Light Calculation (Blinn-Phong)
// Note: lightColor already includes Intensity (calculated in C++)
// Note: This function uses white (1,1,1) for specular color
//       UberLit.hlsl can override this to use Material.SpecularColor
float3 CalculateSpecular(float3 lightDir, float3 normal, float3 viewDir, float4 lightColor, float specularPower)
{
    float3 halfVec = normalize(lightDir + viewDir);
    float NdotH = max(dot(normal, halfVec), 0.0f);
    float specular = pow(NdotH, specularPower);

    // Default: white specular (1, 1, 1)
    // Individual shaders can override this for material-specific specular
    return lightColor.rgb * specular;
}

//================================================================================================
// Attenuation Functions
//================================================================================================

// Unreal Engine: Inverse Square Falloff (Physically Accurate)
// https://docs.unrealengine.com/en-US/BuildingWorlds/LightingAndShadows/PhysicalLightUnits/
float CalculateInverseSquareFalloff(float distance, float attenuationRadius)
{
    // Unreal Engine's inverse square falloff with smooth window function
    float distanceSq = distance * distance;
    float radiusSq = attenuationRadius * attenuationRadius;

    // Basic inverse square law: I = 1 / (distance^2)
    float basicFalloff = 1.0f / max(distanceSq, 0.01f * 0.01f); // Prevent division by zero

    // Apply smooth window function that reaches 0 at attenuationRadius
    // This prevents hard cutoff at radius boundary
    float distanceRatio = saturate(distance / attenuationRadius);
    float windowAttenuation = pow(1.0f - pow(distanceRatio, 4.0f), 2.0f);

    return basicFalloff * windowAttenuation;
}

// Unreal Engine: Light Falloff Exponent (Artistic Control)
// Non-physically accurate but provides artistic control
float CalculateExponentFalloff(float distance, float attenuationRadius, float falloffExponent)
{
    // Normalized distance (0 at light center, 1 at radius boundary)
    float distanceRatio = saturate(distance / attenuationRadius);

    // Simple exponent-based falloff: attenuation = (1 - ratio)^exponent
    // falloffExponent controls the curve:
    // - exponent = 1: linear falloff
    // - exponent > 1: faster falloff (sharper)
    // - exponent < 1: slower falloff (gentler)
    float attenuation = pow(1.0f - pow(distanceRatio, 2.0f), max(falloffExponent, 0.1f));

    return attenuation;
}

//================================================================================================
// Gamma Correction Functions
//================================================================================================

// Linear to sRGB conversion (Gamma Correction)
// Converts linear RGB values to sRGB color space for display
float3 LinearToSRGB(float3 linearColor)
{
    // sRGB standard: exact formula with piecewise function
    float3 sRGBLo = linearColor * 12.92f;
    float3 sRGBHi = pow(max(linearColor, 0.0f), 1.0f / 2.4f) * 1.055f - 0.055f;
    float3 sRGB = (linearColor <= 0.0031308f) ? sRGBLo : sRGBHi;
    return sRGB;
}

// Simple gamma correction (approximation, faster but less accurate)
float3 LinearToGamma(float3 linearColor)
{
    return pow(max(linearColor, 0.0f), 1.0f / 2.2f);
}

//================================================================================================
// 통합 조명 계산 함수
//================================================================================================

// Directional Light Calculation (Diffuse + Specular)
float3 CalculateDirectionalLight(FDirectionalLightInfo light, float3 normal, float3 viewDir, float4 materialColor, bool includeSpecular, float specularPower)
{
    // if Light.Direction is zero vector, avoid normalization issues
    if (all(light.Direction == float3(0.0f, 0.0f, 0.0f)))
    {
        return float3(0.0f, 0.0f, 0.0f);
    }

    float3 lightDir = normalize(-light.Direction);

    // Diffuse (light.Color already includes Intensity)
    float3 diffuse = CalculateDiffuse(lightDir, normal, light.Color, materialColor);

    // Specular (optional)
    float3 specular = float3(0.0f, 0.0f, 0.0f);
    if (includeSpecular)
    {
        specular = CalculateSpecular(lightDir, normal, viewDir, light.Color, specularPower);
    }

    return diffuse + specular;
}

// Point Light Calculation (Diffuse + Specular with Attenuation and Falloff)
float3 CalculatePointLight(FPointLightInfo light, float3 worldPos, float3 normal, float3 viewDir, float4 materialColor, bool includeSpecular, float specularPower)
{
    float3 lightVec = light.Position - worldPos;
    float distance = length(lightVec);

    // Protect against division by zero with epsilon
    distance = max(distance, 0.0001f);
    float3 lightDir = lightVec / distance;

    // Calculate attenuation based on falloff mode
    float attenuation;
    if (light.bUseInverseSquareFalloff)
    {
        // Physically accurate inverse square falloff (Unreal Engine style)
        attenuation = CalculateInverseSquareFalloff(distance, light.AttenuationRadius);
    }
    else
    {
        // Artistic exponent-based falloff for greater control
        attenuation = CalculateExponentFalloff(distance, light.AttenuationRadius, light.FalloffExponent);
    }

    // Diffuse (light.Color already includes Intensity)
    float3 diffuse = CalculateDiffuse(lightDir, normal, light.Color, materialColor) * attenuation;

    // Specular (optional)
    float3 specular = float3(0.0f, 0.0f, 0.0f);
    if (includeSpecular)
    {
        specular = CalculateSpecular(lightDir, normal, viewDir, light.Color, specularPower) * attenuation;
    }

    return diffuse + specular;
}

// Spot Light Calculation (Diffuse + Specular with Attenuation and Cone)
float3 CalculateSpotLight(FSpotLightInfo light, float3 worldPos, float3 normal, float3 viewDir, float4 materialColor, bool includeSpecular, float specularPower)
{
    float3 lightVec = light.Position - worldPos;
    float distance = length(lightVec);

    // Protect against division by zero with epsilon
    distance = max(distance, 0.0001f);
    float3 lightDir = lightVec / distance;
    float3 spotDir = normalize(light.Direction);

    // Spot cone attenuation
    float cosAngle = dot(-lightDir, spotDir);
    float innerCos = cos(radians(light.InnerConeAngle));
    float outerCos = cos(radians(light.OuterConeAngle));

    // Smooth falloff between inner and outer cone (returns 0 if outside cone)
    float spotAttenuation = smoothstep(outerCos, innerCos, cosAngle);

    // Calculate distance attenuation based on falloff mode
    float distanceAttenuation;
    if (light.bUseInverseSquareFalloff)
    {
        // Physically accurate inverse square falloff (Unreal Engine style)
        distanceAttenuation = CalculateInverseSquareFalloff(distance, light.AttenuationRadius);
    }
    else
    {
        // Artistic exponent-based falloff for greater control
        distanceAttenuation = CalculateExponentFalloff(distance, light.AttenuationRadius, light.FalloffExponent);
    }

    // Combine both attenuations
    float attenuation = distanceAttenuation * spotAttenuation;

    // Diffuse (light.Color already includes Intensity)
    float3 diffuse = CalculateDiffuse(lightDir, normal, light.Color, materialColor) * attenuation;

    // Specular (optional)
    float3 specular = float3(0.0f, 0.0f, 0.0f);
    if (includeSpecular)
    {
        specular = CalculateSpecular(lightDir, normal, viewDir, light.Color, specularPower) * attenuation;
    }

    return diffuse + specular;
}

//================================================================================================
// 매크로 기반 조명 모델 헬퍼
//================================================================================================

// 조명 모델에 따라 적절한 파라미터 자동 설정
#ifdef LIGHTING_MODEL_GOURAUD
    #define LIGHTING_INCLUDE_SPECULAR true
    #define LIGHTING_NEED_VIEWDIR true
#elif defined(LIGHTING_MODEL_LAMBERT)
    #define LIGHTING_INCLUDE_SPECULAR false
    #define LIGHTING_NEED_VIEWDIR false
#elif defined(LIGHTING_MODEL_PHONG)
    #define LIGHTING_INCLUDE_SPECULAR true
    #define LIGHTING_NEED_VIEWDIR true
#else
    // No lighting model defined - still provide defaults
    #define LIGHTING_INCLUDE_SPECULAR false
    #define LIGHTING_NEED_VIEWDIR false
#endif

//================================================================================================
// 타일 컬링 기반 전체 조명 계산 (매크로 자동 대응)
//================================================================================================

// 모든 라이트(Point + Spot) 계산 - 타일 컬링 지원
// 매크로에 따라 자동으로 specular on/off
float3 CalculateAllLights(
    float3 worldPos,
    float3 normal,
    float3 viewDir,        // LAMBERT에서는 사용 안 함
    float4 baseColor,
    float specularPower,
    float4 screenPos)      // For tile culling
{
    float3 litColor = float3(0, 0, 0);

    // Ambient
    litColor += CalculateAmbientLight(AmbientLight, baseColor);

    // Directional
    litColor += CalculateDirectionalLight(
        DirectionalLight,
        normal,
        viewDir,  // LAMBERT에서는 무시됨
        baseColor,
        LIGHTING_INCLUDE_SPECULAR,  // 매크로에 따라 자동 설정
        specularPower
    );

    // Point + Spot with tile culling
    if (bUseTileCulling)
    {
        uint tileIndex = CalculateTileIndex(screenPos);
        uint tileDataOffset = GetTileDataOffset(tileIndex);
        uint lightCount = g_TileLightIndices[tileDataOffset];

        for (uint i = 0; i < lightCount; i++)
        {
            uint packedIndex = g_TileLightIndices[tileDataOffset + 1 + i];
            uint lightType = (packedIndex >> 16) & 0xFFFF;
            uint lightIdx = packedIndex & 0xFFFF;

            if (lightType == 0)  // Point Light
            {
                litColor += CalculatePointLight(
                    g_PointLightList[lightIdx],
                    worldPos,
                    normal,
                    viewDir,
                    baseColor,
                    LIGHTING_INCLUDE_SPECULAR,  // 매크로 자동 대응
                    specularPower
                );
            }
            else if (lightType == 1)  // Spot Light
            {
                litColor += CalculateSpotLight(
                    g_SpotLightList[lightIdx],
                    worldPos,
                    normal,
                    viewDir,
                    baseColor,
                    LIGHTING_INCLUDE_SPECULAR,  // 매크로 자동 대응
                    specularPower
                );
            }
        }
    }
    else
    {
        // 모든 라이트 순회 (타일 컬링 비활성화)
        for (int i = 0; i < PointLightCount; i++)
        {
            litColor += CalculatePointLight(
                g_PointLightList[i],
                worldPos,
                normal,
                viewDir,
                baseColor,
                LIGHTING_INCLUDE_SPECULAR,
                specularPower
            );
        }

        for (int j = 0; j < SpotLightCount; j++)
        {
            litColor += CalculateSpotLight(
                g_SpotLightList[j],
                worldPos,
                normal,
                viewDir,
                baseColor,
                LIGHTING_INCLUDE_SPECULAR,
                specularPower
            );
        }
    }

    return litColor;
}
