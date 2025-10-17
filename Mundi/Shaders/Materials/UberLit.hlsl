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
struct FAmbientLightInfo
{
    float4 Color;       // FLinearColor
    float Intensity;
    float3 Padding;     // FVector Padding
};

struct FDirectionalLightInfo
{
    float4 Color;       // FLinearColor
    float Intensity;
    float3 Direction;   // FVector
};

struct FPointLightInfo
{
    float4 Color;           // FLinearColor
    float3 Position;        // FVector
    float FalloffExponent;  // float
    float3 Attenuation;     // FVector (constant, linear, quadratic)
    float AttenuationRadius; // float
    float Intensity;
    uint bUseAttenuationCoefficients; // uint32
    float2 Padding;         // FVector2D Padding
};

struct FSpotLightInfo
{
    float4 Color;           // FLinearColor
    float3 Position;        // FVector
    float InnerConeAngle;   // float
    float3 Direction;       // FVector
    float OuterConeAngle;   // float
    float3 Attenuation;     // FVector
    float AttenuationRadius; // float
    float FalloffExponent;  // float
    float Intensity;
    uint bUseAttenuationCoefficients; // uint32
    float Padding;          // float Padding
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
    float MaterialDummy;        // Padding
};

// --- 상수 버퍼 (Constant Buffers) ---
// Extended to support both lighting and StaticMeshShader features

// b0: ModelBuffer (VS) - Matches ModelBufferType exactly (64 bytes)
cbuffer ModelBuffer : register(b0)
{
    row_major float4x4 WorldMatrix;  // 64 bytes only
    // Note: C++ ModelBufferType doesn't include WorldInverseTranspose
    // Must use WorldMatrix for normal transformation (works for uniform scale)
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
};

// b4: PixelConstBuffer (PS) - Material information from OBJ files
// Must match FPixelConstBufferType exactly!
// HLSL bool = 4 bytes, C++ bool = 1 byte -> need careful padding
cbuffer PixelConstBuffer : register(b4)
{
    FMaterial Material;         // 64 bytes
    bool HasMaterial;           // 4 bytes (HLSL)
    bool HasTexture;            // 4 bytes (HLSL)
};

// b7: CameraBuffer (VS+PS) - Camera properties (moved from b2)
cbuffer CameraBuffer : register(b7)
{
    float3 CameraPosition;
    float CameraPadding;
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
    float2 LightPadding;
};

// --- 텍스처 및 샘플러 리소스 ---
Texture2D g_DiffuseTexColor : register(t0);
SamplerState g_Sample : register(s0);

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

// --- 유틸리티 함수 ---

// Ambient Light Calculation
// Uses the provided materialColor (which includes texture if available)
float3 CalculateAmbientLight(FAmbientLightInfo light, float4 materialColor)
{
    // Use materialColor directly (already contains texture if available)
    return light.Color.rgb * light.Intensity * materialColor.rgb;
}

// Diffuse Light Calculation (Lambert)
// Uses the provided materialColor (which includes texture if available)
float3 CalculateDiffuse(float3 lightDir, float3 normal, float4 lightColor, float intensity, float4 materialColor)
{
    float NdotL = max(dot(normal, lightDir), 0.0f);
    // Use materialColor directly (already contains texture if available)
    return lightColor.rgb * intensity * materialColor.rgb * NdotL;
}

// Specular Light Calculation (Blinn-Phong)
// Uses material's SpecularColor (Ks) if available - this is important!
float3 CalculateSpecular(float3 lightDir, float3 normal, float3 viewDir, float4 lightColor, float intensity, float specularPower)
{
    float3 halfVec = normalize(lightDir + viewDir);
    float NdotH = max(dot(normal, halfVec), 0.0f);
    float specular = pow(NdotH, specularPower);

    // Apply material's specular color (Ks) - metallic materials have colored specular!
    float3 specularMaterial = HasMaterial ? Material.SpecularColor : float3(1.0f, 1.0f, 1.0f);
    return lightColor.rgb * intensity * specularMaterial * specular;
}

// Attenuation Calculation for Point/Spot Lights
float CalculateAttenuation(float3 attenuation, float distance)
{
    return 1.0f / (attenuation.x + attenuation.y * distance + attenuation.z * distance * distance);
}

// Attenuation with Falloff Exponent (for Point Lights)
float CalculateAttenuationWithFalloff(float3 attenuation, float distance, float falloffExponent)
{
    float baseAttenuation = CalculateAttenuation(attenuation, distance);
    // Apply falloff exponent to create steeper or gentler falloff curves
    // falloffExponent = 1.0 means linear, > 1.0 means sharper falloff, < 1.0 means gentler
    return pow(baseAttenuation, falloffExponent);
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

    // Diffuse
    float3 diffuse = CalculateDiffuse(lightDir, normal, light.Color, light.Intensity, materialColor);

    // Specular (optional)
    float3 specular = float3(0.0f, 0.0f, 0.0f);
    if (includeSpecular)
    {
        specular = CalculateSpecular(lightDir, normal, viewDir, light.Color, light.Intensity, specularPower);
    }

    return diffuse + specular;
}

// Point Light Calculation (Diffuse + Specular with Attenuation and Falloff)
float3 CalculatePointLight(FPointLightInfo light, float3 worldPos, float3 normal, float3 viewDir, float4 materialColor, bool includeSpecular, float specularPower)
{
    float3 lightVec = light.Position - worldPos;
    float distance = length(lightVec);

    // Early out if beyond radius
    if (distance > light.AttenuationRadius)
        return float3(0.0f, 0.0f, 0.0f);

    // Protect against division by zero with epsilon
    distance = max(distance, 0.0001f);
    float3 lightDir = lightVec / distance;

    // Calculate attenuation based on bUseAttenuationCoefficients flag
    float attenuation;
    if (light.bUseAttenuationCoefficients)
    {
        // Use FalloffExponent to control attenuation curve
        attenuation = CalculateAttenuationWithFalloff(light.Attenuation, distance, light.FalloffExponent);
    }
    else
    {
        // Inverse square falloff (physically accurate)
        // Scale by radius squared to normalize brightness across different radius values
        float radiusSq = light.AttenuationRadius * light.AttenuationRadius;
        attenuation = radiusSq / (distance * distance);
        // Clamp to avoid over-bright values at very close distances
        attenuation = min(attenuation, 1.0f);
    }

    // Diffuse
    float3 diffuse = CalculateDiffuse(lightDir, normal, light.Color, light.Intensity, materialColor) * attenuation;

    // Specular (optional)
    float3 specular = float3(0.0f, 0.0f, 0.0f);
    if (includeSpecular)
    {
        specular = CalculateSpecular(lightDir, normal, viewDir, light.Color, light.Intensity, specularPower) * attenuation;
    }

    return diffuse + specular;
}

// Spot Light Calculation (Diffuse + Specular with Attenuation and Cone)
float3 CalculateSpotLight(FSpotLightInfo light, float3 worldPos, float3 normal, float3 viewDir, float4 materialColor, bool includeSpecular, float specularPower)
{
    float3 lightVec = light.Position - worldPos;
    float distance = length(lightVec);

    // Early out if beyond radius
    if (distance > light.AttenuationRadius)
        return float3(0.0f, 0.0f, 0.0f);

    // Protect against division by zero with epsilon
    distance = max(distance, 0.0001f);
    float3 lightDir = lightVec / distance;
    float3 spotDir = normalize(light.Direction);

    // Spot cone attenuation
    float cosAngle = dot(-lightDir, spotDir);
    float innerCos = cos(light.InnerConeAngle);
    float outerCos = cos(light.OuterConeAngle);

    // Early out if outside cone
    if (cosAngle < outerCos)
        return float3(0.0f, 0.0f, 0.0f);

    // Calculate distance attenuation based on bUseAttenuationCoefficients flag
    float distanceAttenuation;
    if (light.bUseAttenuationCoefficients)
    {
        // Use FalloffExponent to control attenuation curve
        distanceAttenuation = CalculateAttenuationWithFalloff(light.Attenuation, distance, light.FalloffExponent);
    }
    else
    {
        // Inverse square falloff (physically accurate)
        // Scale by radius squared to normalize brightness across different radius values
        float radiusSq = light.AttenuationRadius * light.AttenuationRadius;
        distanceAttenuation = radiusSq / (distance * distance);
        // Clamp to avoid over-bright values at very close distances
        distanceAttenuation = min(distanceAttenuation, 1.0f);
    }

    // Smooth falloff between inner and outer cone
    float spotAttenuation = smoothstep(outerCos, innerCos, cosAngle);

    // Combine both attenuations
    float attenuation = distanceAttenuation * spotAttenuation;

    // Diffuse
    float3 diffuse = CalculateDiffuse(lightDir, normal, light.Color, light.Intensity, materialColor) * attenuation;

    // Specular (optional)
    float3 specular = float3(0.0f, 0.0f, 0.0f);
    if (includeSpecular)
    {
        specular = CalculateSpecular(lightDir, normal, viewDir, light.Color, light.Intensity, specularPower) * attenuation;
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
    // Using WorldMatrix (works correctly for uniform scale only)
    // For non-uniform scale, would need transpose(inverse(WorldMatrix))
    float3 worldNormal = normalize(mul(Input.Normal, (float3x3) WorldMatrix));
    Out.Normal = worldNormal;

    Out.TexCoord = Input.TexCoord;

    // Use SpecularExponent from material, or default value if no material
    float specPower = HasMaterial ? Material.SpecularExponent : 32.0f;

#if LIGHTING_MODEL_GOURAUD
    // Gouraud Shading: Calculate lighting per-vertex (diffuse + specular)
    float3 finalColor = float3(0.0f, 0.0f, 0.0f);

    // Calculate view direction for specular
    float3 viewDir = normalize(CameraPosition - Out.WorldPos);

    // Ambient light
    finalColor += CalculateAmbientLight(AmbientLight, Input.Color);

    // Directional light (diffuse + specular)
    finalColor += CalculateDirectionalLight(DirectionalLight, worldNormal, viewDir, Input.Color, true, specPower);

    // Point lights (diffuse + specular)
    for (int i = 0; i < PointLightCount; i++)
    {
        finalColor += CalculatePointLight(PointLights[i], Out.WorldPos, worldNormal, viewDir, Input.Color, true, specPower);
    }

    // Spot lights (diffuse + specular)
    for (int j = 0; j < SpotLightCount; j++)
    {
        finalColor += CalculateSpotLight(SpotLights[j], Out.WorldPos, worldNormal, viewDir, Input.Color, true, specPower);
    }

    Out.Color = float4(finalColor, Input.Color.a);

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
float4 mainPS(PS_INPUT Input) : SV_TARGET
{
    // Apply UV scrolling if enabled
    float2 uv = Input.TexCoord;
    //if (HasMaterial && HasTexture)
    //{
    //    uv += UVScrollSpeed * UVScrollTime;
    //}

    // Sample texture
    float4 texColor = g_DiffuseTexColor.Sample(g_Sample, uv);

    // Use SpecularExponent from material, or default value if no material
    float specPower = HasMaterial ? Material.SpecularExponent : 32.0f;

#if LIGHTING_MODEL_GOURAUD
    // Gouraud Shading: Lighting already calculated in vertex shader
    float4 finalPixel = Input.Color;

    // Apply texture modulation if available
    if (HasTexture)
    {
        finalPixel.rgb *= texColor.rgb;
    }

    // Add emissive (self-illumination) - not affected by lighting
    if (HasMaterial)
    {
        finalPixel.rgb += Material.EmissiveColor;
    }

    // Apply material/color blending for non-material objects
    if (!HasMaterial)
    {
        finalPixel.rgb = lerp(finalPixel.rgb, LerpColor.rgb, LerpColor.a);
    }
    
#ifdef USE_GAMMA_CORRECTION
    // Apply gamma correction (Linear to sRGB)
    finalPixel.rgb = LinearToSRGB(finalPixel.rgb);
#endif

    return finalPixel;

#elif LIGHTING_MODEL_LAMBERT
    // Lambert Shading: Calculate diffuse lighting per-pixel (no specular)
    float3 normal = normalize(Input.Normal);
    float4 baseColor = Input.Color;

    // Start with texture if available
    if (HasTexture)
    {
        baseColor.rgb = texColor.rgb;
    }
    else if (HasMaterial)
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

    // Point lights (diffuse only)
    for (int i = 0; i < PointLightCount; i++)
    {
        litColor += CalculatePointLight(PointLights[i], Input.WorldPos, normal, float3(0, 0, 0), baseColor, false, 0.0f);
    }

    // Spot lights (diffuse only)
    for (int j = 0; j < SpotLightCount; j++)
    {
        litColor += CalculateSpotLight(SpotLights[j], Input.WorldPos, normal, float3(0, 0, 0), baseColor, false, 0.0f);
    }

    // Add emissive (self-illumination) after lighting calculation
    if (HasMaterial)
    {
        litColor += Material.EmissiveColor;
    }

#ifdef USE_GAMMA_CORRECTION
    // Apply gamma correction (Linear to sRGB)
    litColor = LinearToSRGB(litColor);
#endif

    // Preserve original alpha (lighting doesn't affect transparency)
    return float4(litColor, baseColor.a);

#elif LIGHTING_MODEL_PHONG
    // Phong Shading: Calculate diffuse and specular lighting per-pixel (Blinn-Phong)
    float3 normal = normalize(Input.Normal);
    float3 viewDir = normalize(CameraPosition - Input.WorldPos);
    float4 baseColor = Input.Color;

    // Start with texture if available
    if (HasTexture)
    {
        baseColor.rgb = texColor.rgb;
    }
    else if (HasMaterial)
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

    // Point lights (diffuse + specular)
    for (int i = 0; i < PointLightCount; i++)
    {
        litColor += CalculatePointLight(PointLights[i], Input.WorldPos, normal, viewDir, baseColor, true, specPower);
    }

    // Spot lights (diffuse + specular)
    for (int j = 0; j < SpotLightCount; j++)
    {
        litColor += CalculateSpotLight(SpotLights[j], Input.WorldPos, normal, viewDir, baseColor, true, specPower);
    }

    // Add emissive (self-illumination) after lighting calculation
    if (HasMaterial)
    {
        litColor += Material.EmissiveColor;
    }

#ifdef USE_GAMMA_CORRECTION
    // Apply gamma correction (Linear to sRGB)
    litColor = LinearToSRGB(litColor);
#endif

    // Preserve original alpha (lighting doesn't affect transparency)
    return float4(litColor, baseColor.a);

#else
    // No lighting model defined - use StaticMeshShader behavior
    float4 finalPixel = Input.Color;

    // Apply material/texture blending
    if (HasMaterial)
    {
        finalPixel.rgb = Material.DiffuseColor;
        if (HasTexture)
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
    
    return finalPixel;
#endif
}