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

cbuffer DepthOfFieldCB : register(b2)
{
    float FocusDistance;
    float FocusRange;
    float COCSize;
    float padding;
}
cbuffer DOFGaussianCB : register(b3)
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

float GetGaussian(float dis)
{
    return exp(-dis * dis / (2 * Weight * Weight));
}

float4 mainPS(PS_INPUT input) : SV_Target
{
    uint TexWidth, TexHeight;
    g_SceneColorTex.GetDimensions(TexWidth, TexHeight);
    float2 uv = float2(input.position.x / TexWidth, input.position.y / TexHeight);
    float2 InvTexSize = float2(1.0f / TexWidth, 1.0f / TexHeight);
    float3 TotalColor = float3(0, 0, 0);
    float TotalGaussian = 0;
    
    float2 UVDir = float2(0, 0);
    if (bHorizontal)
    {
        UVDir = float2(InvTexSize.x, 0);
    }
    else
    {
        UVDir = float2(0, InvTexSize.y);
    }
    int halfRange = Range / 2;
    float2 COC = g_COCTex.Sample(g_PointClampSample, uv).rg * COCSize;

    if(bNear)
    {
        for (int i = -halfRange; i <= halfRange; ++i)
        {
            float CurGaussian = GetGaussian(i);
            float2 CurUV = uv + UVDir * i * COC.r;
            float2 CurCOC = g_COCTex.Sample(g_PointClampSample, CurUV).rg;
            if (CurCOC.r > 0)
            {
                float3 SampleColor = g_SceneColorTex.Sample(g_LinearClampSample, CurUV).rgb;
                TotalGaussian += CurGaussian;
                TotalColor += SampleColor * CurGaussian;
            }
        }
    }
    else
    {
        for (int i = -halfRange; i <= halfRange; ++i)
        {
            float CurGaussian = GetGaussian(i);
            float2 CurUV = uv + UVDir * i * COC.g;
            float2 CurCOC = g_COCTex.Sample(g_PointClampSample, CurUV).rg;
            if (CurCOC.g > 0)
            {
                float3 SampleColor = g_SceneColorTex.Sample(g_LinearClampSample, CurUV).rgb;
                TotalGaussian += CurGaussian;
                TotalColor += SampleColor * CurGaussian;
            }
        }
    }
    
    float3 FinalColor = TotalColor / TotalGaussian;
    return float4(FinalColor.rgb, 1);
}
