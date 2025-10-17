#pragma once
constexpr uint32 NUM_POINT_LIGHT_MAX = 16;
constexpr uint32 NUM_SPOT_LIGHT_MAX = 16;

struct FAmbientLightInfo
{
    FLinearColor Color;
    float Intensity;
    float Padding[3];
};

struct FDirectionalLightInfo
{
    FLinearColor Color;
    FVector Direction;
    float Intensity;
};

struct FPointLightInfo
{
    FLinearColor Color;
    FVector Position;
    float Radius;
    FVector Attenuation;    //상수, 일차항, 이차항
    float Intensity;
};

struct FSpotLightInfo
{
    FLinearColor Color;
    FVector Position;
    float InnerConeAngle;
    FVector Direction;
    float OuterConeAngle;
    float Intensity;
    float Radius;
    float Padding[2];
};