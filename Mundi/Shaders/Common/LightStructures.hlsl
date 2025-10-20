//================================================================================================
// Filename:      LightStructures.hlsl
// Description:   Common light structure definitions
//                Shared across all shaders that use lighting (UberLit, Decal, etc.)
//================================================================================================

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
