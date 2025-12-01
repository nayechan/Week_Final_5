Texture2D g_SceneColorTex : register(t0);
Texture2D g_COCTex : register(t1);

SamplerState g_LinearClampSample : register(s0);
SamplerState g_PointClampSample : register(s1);

struct PS_INPUT
{
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD0;
};

cbuffer PostProcessCB : register(b0)
{
    float Near;
    float Far;
}

cbuffer ViewProjBuffer : register(b1)
{
    row_major float4x4 ViewMatrix;
    row_major float4x4 ProjectionMatrix;
    row_major float4x4 InverseViewMatrix;
    row_major float4x4 InverseProjectionMatrix;
}

cbuffer GaussianCB : register(b2)
{
    float Weight;
    uint Range;
    uint bHorizontal;
    uint bNear;
}

cbuffer ViewportConstants : register(b10)
{
    // x: Viewport TopLeftX, y: Viewport TopLeftY
    // z: Viewport Width,   w: Viewport Height
    float4 ViewportRect;
    
    // x: Screen Width      (전체 렌더 타겟 너비)
    // y: Screen Height     (전체 렌더 타겟 높이)
    // z: 1.0f / Screen W,  w: 1.0f / Screen H
    float4 ScreenSize;
}
#define PI 3.141592f
#define PI_OVER_2 1.5707963f
#define PI_OVER_4 0.785398f
#define EPSILON 0.000001f

// Maps a unit square in [-1, 1] to a unit disk in [-1, 1]. Shirley 97 "A Low Distortion Map Between Disk and Square"
// Inputs: cartesian coordinates
// Return: new circle-mapped polar coordinates (radius, angle)
float2 UnitSquareToUnitDiskPolar(float a, float b)
{
    float radius, angle;
    if (abs(a) > abs(b))
    { // First region (left and right quadrants of the disk)
        radius = a;
        angle = b / (a + EPSILON) * PI_OVER_4;
    }
    else
    { // Second region (top and botom quadrants of the disk)
        radius = b;
        angle = PI_OVER_2 - (a / (b + EPSILON) * PI_OVER_4);
    }
    if (radius < 0)
    { // Always keep radius positive
        radius *= -1.0f;
        angle += PI;
    }
    return float2(radius, angle);
}

// Maps a unit square in [-1, 1] to a unit disk in [-1, 1]
// Inputs: cartesian coordinates
// Return: new circle-mapped cartesian coordinates
float2 SquareToDiskMapping(float a, float b)
{
    float2 PolarCoord = UnitSquareToUnitDiskPolar(a, b);
    return float2(PolarCoord.x * cos(PolarCoord.y), PolarCoord.x * sin(PolarCoord.y));
}

float4 mainPS(PS_INPUT input) : SV_Target
{
    
}
