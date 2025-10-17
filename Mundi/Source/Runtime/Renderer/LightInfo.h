#pragma once
constexpr uint32 NUM_POINT_LIGHT_MAX = 16;
constexpr uint32 NUM_SPOT_LIGHT_MAX = 16;

struct FAmbientLightInfo
{
    FLinearColor Color;

    float Intensity;
    FVector Padding;
};

struct FDirectionalLightInfo
{
    FLinearColor Color;

    float Intensity;
    FVector Direction;
};

struct FPointLightInfo
{
    FLinearColor Color;

    FVector Position;
    float FalloffExponent;

    FVector Attenuation;    // 상수, 일차항, 이차항
    float AttenuationRadius;

    float Intensity;
    uint32 bUseAttenuationCoefficients;
    FVector2D Padding;
};

struct FSpotLightInfo
{
    FLinearColor Color;

    FVector Position;
    float InnerConeAngle;

    FVector Direction;
    float OuterConeAngle;

    FVector Attenuation;
    float AttenuationRadius;

    float FalloffExponent;
    float Intensity;
    uint32 bUseAttenuationCoefficients;
    float Padding;
};