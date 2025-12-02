Texture2D g_SceneColorTex : register(t0);

SamplerState g_LinearClampSample : register(s0);
SamplerState g_PointClampSample : register(s1);

struct PS_INPUT
{
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD0;
};
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
static uint KernelCount = 13;
static float2 UVOffset[13] =
{
    float2(-2, -2), float2(0, -2), float2(2, -2),
    float2(-2, 0), float2(0, 0), float2(2, 0),
    float2(-2, 2), float2(0, 2), float2(2, 2),
    float2(-1, -1), float2(1, -1),
    float2(-1,1), float2(1,1)
};
static float UVWeight[13] =
{
    0.03125f, 0.0625f, 0.03125f,
    0.0625f, 0.125f, 0.0625f,
    0.03125f, 0.0625f, 0.03125f,
    0.125f, 0.125f, 0.125f, 0.125f
};

float4 mainPS(PS_INPUT input) : SV_Target
{
    uint TexWidth, TexHeight;
    g_SceneColorTex.GetDimensions(TexWidth, TexHeight);
    float2 uv = float2(input.position.x / TexWidth, input.position.y / TexHeight) * 2;
    float2 InvTexSize = float2(1.0f / TexWidth, 1.0f / TexHeight);
    
    float3 Color = g_SceneColorTex.Sample(g_LinearClampSample, uv).rgb;

    float3 TotalColor = float3(0, 0, 0);
    for (int i = 0; i < KernelCount;i++)
    {
        float2 CurUV = uv + InvTexSize * UVOffset[i];
        float3 CurColor = g_SceneColorTex.Sample(g_LinearClampSample, CurUV).rgb;
        TotalColor += CurColor * UVWeight[i];
    }
    return float4(TotalColor, 1);
}
