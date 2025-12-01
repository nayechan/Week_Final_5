Texture2D g_COCTex : register(t0);

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
    g_COCTex.GetDimensions(TexWidth, TexHeight);
    float2 uv = float2(input.position.x / TexWidth, input.position.y / TexHeight);
    float2 InvTexSize = float2(1.0f / TexWidth, 1.0f / TexHeight);
    float2 TotalCOCR = float2(0, 0);
    float TotalGaussian = 0;
    
    float2 UVDir = float2(InvTexSize.x, 0);
    if(bHorizontal == 0)
    {
        UVDir = float2(0, InvTexSize.y);
    }
    
    int halfRange = Range / 2;

    float3 FinalColor = float3(0, 0, 0);
    for (int i = -halfRange; i <= halfRange; ++i)
    {
        float CurGaussian = GetGaussian(i);
        float2 CurUV = uv + UVDir * i;
        float3 CurCOC = g_COCTex.Sample(g_PointClampSample, CurUV).rgb;
        TotalGaussian += CurGaussian;
        TotalCOCR += CurCOC.rg * CurGaussian;
    }
    
    FinalColor.rg = TotalCOCR / TotalGaussian;
    return float4(FinalColor.rgb, 1);
}
