//================================================================================================
// Filename:      UberLit.hlsl
// Description:   오브젝트 표면 렌더링을 위한 기본 Uber 셰이더.
//                Extends StaticMeshShader with full lighting support (Gouraud, Lambert, Phong)
//================================================================================================

// --- 조명 모델 선택 ---
// #define LIGHTING_MODEL_GOURAUD 1
// #define LIGHTING_MODEL_LAMBERT 1
// #define LIGHTING_MODEL_PHONG 1

// --- 감마 보정 옵션 ---
// StaticMeshShader 호환을 위해 기본적으로 꺼져있음
// 조명 사용 시 활성화 권장
// #define USE_GAMMA_CORRECTION 1

// --- 전역 상수 정의 ---
#define NUM_POINT_LIGHT_MAX 16
#define NUM_SPOT_LIGHT_MAX 16

// --- 조명 정보 구조체 (LightInfo.h와 완전히 일치) ---
// Note: Color already includes Intensity and Temperature (calculated in C++)
// Optimized padding for minimal memory usage

struct FAmbientLightInfo
{
    float4 Color;       // 16 bytes - FLinearColor (includes Intensity + Temperature)
};

struct FDirectionalLightInfo
{
    float4 Color;       // 16 bytes - FLinearColor (includes Intensity + Temperature)
    float3 Direction;   // 12 bytes - FVector
    float Padding;      // 4 bytes - Padding for alignment
};

struct FPointLightInfo
{
    float4 Color;           // 16 bytes - FLinearColor (includes Intensity + Temperature)
    float3 Position;        // 12 bytes - FVector
    float AttenuationRadius; // 4 bytes (moved up to fill slot)
    float FalloffExponent;  // 4 bytes - Falloff exponent for artistic control
    uint bUseInverseSquareFalloff; // 4 bytes - uint32 (true = physically accurate, false = exponent-based)
    float2 Padding;         // 8 bytes - Padding for alignment (Attenuation removed)
};

struct FSpotLightInfo
{
    float4 Color;           // 16 bytes - FLinearColor (includes Intensity + Temperature)
    float3 Position;        // 12 bytes - FVector
    float InnerConeAngle;   // 4 bytes
    float3 Direction;       // 12 bytes - FVector
    float OuterConeAngle;   // 4 bytes
    float AttenuationRadius; // 4 bytes
    float FalloffExponent;  // 4 bytes - Falloff exponent for artistic control
    uint bUseInverseSquareFalloff; // 4 bytes - uint32 (true = physically accurate, false = exponent-based)
    float Padding;         // 4 bytes - Padding for alignment (Attenuation removed)
};

// --- Material 구조체 (OBJ 머티리얼 정보) ---
struct FMaterial
{
    float3 DiffuseColor;        // Kd - Diffuse color
    float OpticalDensity;       // Ni - Optical density (index of refraction)
    float3 AmbientColor;        // Ka - Ambient color
    float Transparency;         // Tr or d - Transparency (0=opaque, 1=transparent)
    float3 SpecularColor;       // Ks - Specular color
    float SpecularExponent;     // Ns - Specular exponent (shininess)
    float3 EmissiveColor;       // Ke - Emissive color (self-illumination)
    uint IlluminationModel;     // illum - Illumination model
    float3 TransmissionFilter;  // Tf - Transmission filter color
    float Padding;              // Padding for alignment
};

// --- 상수 버퍼 (Constant Buffers) ---
// Extended to support both lighting and StaticMeshShader features

// b0: ModelBuffer (VS) - Matches ModelBufferType exactly (128 bytes)
cbuffer ModelBuffer : register(b0)
{
    row_major float4x4 WorldMatrix;              // 64 bytes
    row_major float4x4 WorldInverseTranspose;    // 64 bytes - For correct normal transformation
};

// b1: ViewProjBuffer (VS) - Matches ViewProjBufferType
cbuffer ViewProjBuffer : register(b1)
{
    row_major float4x4 ViewMatrix;
    row_major float4x4 ProjectionMatrix;
};

// b3: ColorBuffer (PS) - For color blending/lerping
cbuffer ColorBuffer : register(b3)
{
    float4 LerpColor;   // Color to blend with (alpha controls blend amount)
    uint UUID;
};

// b4: PixelConstBuffer (VS+PS) - Material information from OBJ files
// Must match FPixelConstBufferType exactly!
// Note: Used in Vertex Shader for GOURAUD lighting model
cbuffer PixelConstBuffer : register(b4)
{
    FMaterial Material;         // 64 bytes
    uint bHasMaterial;          // 4 bytes (HLSL)
    uint bHasTexture;           // 4 bytes (HLSL)
};

// b7: CameraBuffer (VS+PS) - Camera properties (moved from b2)
cbuffer CameraBuffer : register(b7)
{
    float3 CameraPosition;
};

// b8: LightBuffer (VS+PS) - Matches FLightBufferType from ConstantBufferType.h
cbuffer LightBuffer : register(b8)
{
    FAmbientLightInfo AmbientLight;
    FDirectionalLightInfo DirectionalLight;
    FPointLightInfo PointLights[NUM_POINT_LIGHT_MAX];
    FSpotLightInfo SpotLights[NUM_SPOT_LIGHT_MAX];
    uint PointLightCount;
    uint SpotLightCount;
};

// --- 텍스처 및 샘플러 리소스 ---
Texture2D g_DiffuseTexColor : register(t0);
Texture2D g_NormalTexColor : register(t1);
SamplerState g_Sample : register(s0);
SamplerState g_Sample2 : register(s1);

// --- 타일 기반 라이트 컬링 리소스 ---
// t2: 타일별 라이트 인덱스 Structured Buffer
// 구조:  [TileIndex * MaxLightsPerTile] = LightCount
//        [TileIndex * MaxLightsPerTile + 1 ~ ...] = LightIndices (상위 16비트: 타입, 하위 16비트: 인덱스)
StructuredBuffer<uint> g_TileLightIndices : register(t2);

// b11: 타일 컬링 설정 상수 버퍼
cbuffer TileCullingBuffer : register(b11)
{
    uint TileSize;          // 타일 크기 (픽셀, 기본 16)
    uint TileCountX;        // 가로 타일 개수
    uint TileCountY;        // 세로 타일 개수
    uint bUseTileCulling;   // 타일 컬링 활성화 여부 (0=비활성화, 1=활성화)
};

// --- 셰이더 입출력 구조체 ---
struct VS_INPUT
{
    float3 Position : POSITION;
    float3 Normal : NORMAL0;
    float4 Color : COLOR;
    float2 TexCoord : TEXCOORD0;
};

struct PS_INPUT
{
    float4 Position : SV_POSITION;
    float3 WorldPos : POSITION;     // World position for per-pixel lighting
    float3 Normal : NORMAL0;
    float4 Color : COLOR;
    float2 TexCoord : TEXCOORD0;
};

struct PS_OUTPUT
{
    float4 Color : SV_Target0;
    uint UUID : SV_Target1;
};

// --- 유틸리티 함수 ---

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
// Uses material's SpecularColor (Ks) if available - this is important!
// Note: lightColor already includes Intensity (calculated in C++)
float3 CalculateSpecular(float3 lightDir, float3 normal, float3 viewDir, float4 lightColor, float specularPower)
{
    float3 halfVec = normalize(lightDir + viewDir);
    float NdotH = max(dot(normal, halfVec), 0.0f);
    float specular = pow(NdotH, specularPower);

    // Apply material's specular color (Ks) - metallic materials have colored specular!
    float3 specularMaterial = bHasMaterial ? Material.SpecularColor : float3(1.0f, 1.0f, 1.0f);
    return lightColor.rgb * specularMaterial * specular;
}

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

// Directional Light Calculation (Diffuse + Specular)
float3 CalculateDirectionalLight(FDirectionalLightInfo light, float3 normal, float3 viewDir, float4 materialColor, bool includeSpecular, float specularPower)
{
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
// 버텍스 셰이더 (Vertex Shader)
//================================================================================================
PS_INPUT mainVS(VS_INPUT Input)
{
    PS_INPUT Out;

    // Transform position to world space first
    float4 worldPos = mul(float4(Input.Position, 1.0f), WorldMatrix);
    Out.WorldPos = worldPos.xyz;

    // Then to view space
    float4 viewPos = mul(worldPos, ViewMatrix);

    // Finally to clip space
    Out.Position = mul(viewPos, ProjectionMatrix);

    // Transform normal to world space
    // Using WorldInverseTranspose for correct normal transformation with non-uniform scale
    // Normal vectors transform by transpose(inverse(WorldMatrix))
    float3 worldNormal = normalize(mul(Input.Normal, (float3x3) WorldInverseTranspose));
    Out.Normal = worldNormal;

    Out.TexCoord = Input.TexCoord;

    // Use SpecularExponent from material, or default value if no material
    float specPower = bHasMaterial ? Material.SpecularExponent : 32.0f;

#if LIGHTING_MODEL_GOURAUD
    // Gouraud Shading: Calculate lighting per-vertex (diffuse + specular)
    float3 finalColor = float3(0.0f, 0.0f, 0.0f);

    // Calculate view direction for specular
    float3 viewDir = normalize(CameraPosition - Out.WorldPos);

    // Determine base color (same logic as Lambert/Phong for consistency)
    float4 baseColor = Input.Color;
    if (bHasMaterial)
    {
        // Use material diffuse color
        // Note: Textures will be multiplied in pixel shader
        baseColor.rgb = Material.DiffuseColor;
        baseColor.a = 1.0f;  // Ensure alpha is set properly
    }

    // Ambient light
    finalColor += CalculateAmbientLight(AmbientLight, baseColor);

    // Directional light (diffuse + specular)
    finalColor += CalculateDirectionalLight(DirectionalLight, worldNormal, viewDir, baseColor, true, specPower);

    // Point lights (diffuse + specular)
    for (int i = 0; i < PointLightCount; i++)
    {
        finalColor += CalculatePointLight(PointLights[i], Out.WorldPos, worldNormal, viewDir, baseColor, true, specPower);
    }

    // Spot lights (diffuse + specular)
    for (int j = 0; j < SpotLightCount; j++)
    {
        finalColor += CalculateSpotLight(SpotLights[j], Out.WorldPos, worldNormal, viewDir, baseColor, true, specPower);
    }

    Out.Color = float4(finalColor, baseColor.a);

#elif LIGHTING_MODEL_LAMBERT
    // Lambert Shading: Pass data to pixel shader for per-pixel calculation
    Out.Color = Input.Color;

#elif LIGHTING_MODEL_PHONG
    // Phong Shading: Pass data to pixel shader for per-pixel calculation
    Out.Color = Input.Color;

#else
    // No lighting model defined - pass vertex color as-is
    Out.Color = Input.Color;

#endif

    return Out;
}

//================================================================================================
// 픽셀 셰이더 (Pixel Shader)
//================================================================================================
PS_OUTPUT mainPS(PS_INPUT Input)
{
    PS_OUTPUT Output;
    Output.UUID = UUID;

#ifdef VIEWMODE_WORLD_NORMAL
    // World Normal 시각화: Normal 벡터를 색상으로 변환
    // Normal 범위: -1~1 → 색상 범위: 0~1
    float3 normalColor = Input.Normal * 0.5 + 0.5;
    Output.Color = float4(normalColor, 1.0);
    return Output;
#endif

    // Apply UV scrolling if enabled
    float2 uv = Input.TexCoord;
    //if (bHasMaterial && bHasTexture)
    //{
    //    uv += UVScrollSpeed * UVScrollTime;
    //}

    // Sample texture
    float4 texColor = g_DiffuseTexColor.Sample(g_Sample, uv);

    // Use SpecularExponent from material, or default value if no material
    float specPower = bHasMaterial ? Material.SpecularExponent : 32.0f;

#if LIGHTING_MODEL_GOURAUD
    // Gouraud Shading: Lighting already calculated in vertex shader
    float4 finalPixel = Input.Color;

    // Apply texture or material color
    if (bHasTexture)
    {
        // Texture modulation: multiply lighting result by texture
        finalPixel.rgb *= texColor.rgb;
    }
    // Note: Material.DiffuseColor is already applied in Vertex Shader
    // No additional color application needed here

    // Add emissive (self-illumination) - not affected by lighting
    if (bHasMaterial)
    {
        finalPixel.rgb += Material.EmissiveColor;
    }

    // Apply material/color blending for non-material objects
    if (!bHasMaterial)
    {
        finalPixel.rgb = lerp(finalPixel.rgb, LerpColor.rgb, LerpColor.a);
    }
    
#ifdef USE_GAMMA_CORRECTION
    // Apply gamma correction (Linear to sRGB)
    finalPixel.rgb = LinearToSRGB(finalPixel.rgb);
#endif
    Output.Color = finalPixel;
    return Output;

#elif LIGHTING_MODEL_LAMBERT
    // Lambert Shading: Calculate diffuse lighting per-pixel (no specular)
    float3 normal = normalize(Input.Normal);
    float4 baseColor = Input.Color;

    // Start with texture if available
    if (bHasTexture)
    {
        baseColor.rgb = texColor.rgb;
    }
    else if (bHasMaterial)
    {
        // No texture, use material diffuse color
        baseColor.rgb = Material.DiffuseColor;
    }
    else
    {
        // No texture and no material, blend with LerpColor
        baseColor.rgb = lerp(baseColor.rgb, LerpColor.rgb, LerpColor.a);
    }

    float3 litColor = float3(0.0f, 0.0f, 0.0f);

    // Ambient light
    litColor += CalculateAmbientLight(AmbientLight, baseColor);

    // Directional light (diffuse only)
    litColor += CalculateDirectionalLight(DirectionalLight, normal, float3(0, 0, 0), baseColor, false, 0.0f);

    // 타일 기반 라이트 컬링 적용 (활성화된 경우)
    if (bUseTileCulling)
    {
        // 현재 픽셀이 속한 타일 계산
        uint tileIndex = CalculateTileIndex(Input.Position);
        uint tileDataOffset = GetTileDataOffset(tileIndex);

        // 타일에 영향을 주는 라이트 개수
        uint lightCount = g_TileLightIndices[tileDataOffset];

        // 타일 내 라이트만 순회
        for (uint i = 0; i < lightCount; i++)
        {
            uint packedIndex = g_TileLightIndices[tileDataOffset + 1 + i];
            uint lightType = (packedIndex >> 16) & 0xFFFF;  // 상위 16비트: 타입
            uint lightIdx = packedIndex & 0xFFFF;           // 하위 16비트: 인덱스

            if (lightType == 0)  // Point Light
            {
                litColor += CalculatePointLight(PointLights[lightIdx], Input.WorldPos, normal, float3(0, 0, 0), baseColor, false, 0.0f);
            }
            else if (lightType == 1)  // Spot Light
            {
                litColor += CalculateSpotLight(SpotLights[lightIdx], Input.WorldPos, normal, float3(0, 0, 0), baseColor, false, 0.0f);
            }
        }
    }
    else
    {
        // 타일 컬링 비활성화: 모든 라이트 순회 (기존 방식)
        for (int i = 0; i < PointLightCount; i++)
        {
            litColor += CalculatePointLight(PointLights[i], Input.WorldPos, normal, float3(0, 0, 0), baseColor, false, 0.0f);
        }

        for (int j = 0; j < SpotLightCount; j++)
        {
            litColor += CalculateSpotLight(SpotLights[j], Input.WorldPos, normal, float3(0, 0, 0), baseColor, false, 0.0f);
        }
    }

    // Add emissive (self-illumination) after lighting calculation
    if (bHasMaterial)
    {
        litColor += Material.EmissiveColor;
    }

#ifdef USE_GAMMA_CORRECTION
    // Apply gamma correction (Linear to sRGB)
    litColor = LinearToSRGB(litColor);
#endif
    Output.Color = float4(litColor, baseColor.a);
    // Preserve original alpha (lighting doesn't affect transparency)
    return Output;

#elif LIGHTING_MODEL_PHONG
    // Phong Shading: Calculate diffuse and specular lighting per-pixel (Blinn-Phong)
    float3 normal = normalize(Input.Normal);
    float3 viewDir = normalize(CameraPosition - Input.WorldPos);
    float4 baseColor = Input.Color;

    // Start with texture if available
    if (bHasTexture)
    {
        baseColor.rgb = texColor.rgb;
    }
    else if (bHasMaterial)
    {
        // No texture, use material diffuse color
        baseColor.rgb = Material.DiffuseColor;
    }
    else
    {
        // No texture and no material, blend with LerpColor
        baseColor.rgb = lerp(baseColor.rgb, LerpColor.rgb, LerpColor.a);
    }

    float3 litColor = float3(0.0f, 0.0f, 0.0f);

    // Ambient light
    litColor += CalculateAmbientLight(AmbientLight, baseColor);

    // Directional light (diffuse + specular)
    litColor += CalculateDirectionalLight(DirectionalLight, normal, viewDir, baseColor, true, specPower);

    // 타일 기반 라이트 컬링 적용 (활성화된 경우)
    if (bUseTileCulling)
    {
        // 현재 픽셀이 속한 타일 계산
        uint tileIndex = CalculateTileIndex(Input.Position);
        uint tileDataOffset = GetTileDataOffset(tileIndex);

        // 타일에 영향을 주는 라이트 개수
        uint lightCount = g_TileLightIndices[tileDataOffset];

        // 타일 내 라이트만 순회
        for (uint i = 0; i < lightCount; i++)
        {
            uint packedIndex = g_TileLightIndices[tileDataOffset + 1 + i];
            uint lightType = (packedIndex >> 16) & 0xFFFF;  // 상위 16비트: 타입
            uint lightIdx = packedIndex & 0xFFFF;           // 하위 16비트: 인덱스

            if (lightType == 0)  // Point Light
            {
                litColor += CalculatePointLight(PointLights[lightIdx], Input.WorldPos, normal, viewDir, baseColor, true, specPower);
            }
            else if (lightType == 1)  // Spot Light
            {
                litColor += CalculateSpotLight(SpotLights[lightIdx], Input.WorldPos, normal, viewDir, baseColor, true, specPower);
            }
        }
    }
    else
    {
        // 타일 컬링 비활성화: 모든 라이트 순회 (기존 방식)
        for (int i = 0; i < PointLightCount; i++)
        {
            litColor += CalculatePointLight(PointLights[i], Input.WorldPos, normal, viewDir, baseColor, true, specPower);
        }

        for (int j = 0; j < SpotLightCount; j++)
        {
            litColor += CalculateSpotLight(SpotLights[j], Input.WorldPos, normal, viewDir, baseColor, true, specPower);
        }
    }

    // Add emissive (self-illumination) after lighting calculation
    if (bHasMaterial)
    {
        litColor += Material.EmissiveColor;
    }

#ifdef USE_GAMMA_CORRECTION
    // Apply gamma correction (Linear to sRGB)
    litColor = LinearToSRGB(litColor);
#endif

    // Preserve original alpha (lighting doesn't affect transparency)
    Output.Color = float4(litColor, baseColor.a);
    return Output;

#else
    // No lighting model defined - use StaticMeshShader behavior
    float4 finalPixel = Input.Color;

    // Apply material/texture blending
    if (bHasMaterial)
    {
        finalPixel.rgb = Material.DiffuseColor;
        if (bHasTexture)
        {
            finalPixel.rgb = texColor.rgb;
        }
        // Add emissive
        finalPixel.rgb += Material.EmissiveColor;
    }
    else
    {
        // Blend with LerpColor
        finalPixel.rgb = lerp(finalPixel.rgb, LerpColor.rgb, LerpColor.a);
        finalPixel.rgb *= texColor.rgb;
    }
    
#ifdef USE_GAMMA_CORRECTION
    // Apply gamma correction (Linear to sRGB)
    finalPixel.rgb = LinearToSRGB(finalPixel.rgb);
#endif
    
    Output.Color = finalPixel;
    return Output;
#endif
}