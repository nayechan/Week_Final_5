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

cbuffer DOFMcIntoshCB : register(b4)
{
    uint bHorizontal;
    uint Range;
    float2 McIntoshpadding;
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

float GetLuminance(float3 InColor)
{
    // ITU-R BT.709 표준에 따른 휘도 공식 (가장 일반적)
    return dot(InColor, float3(0.2126, 0.7152, 0.0722));
}


float4 mainPS(PS_INPUT input) : SV_Target
{
    uint TexWidth, TexHeight;
    g_SceneColorTex.GetDimensions(TexWidth, TexHeight);
    float2 uv = float2(input.position.x / TexWidth, input.position.y / TexHeight);
    float2 InvTexSize = float2(1.0f / TexWidth, 1.0f / TexHeight);
    float2 COC = g_COCTex.Sample(g_PointClampSample, uv).rg;
    
    float absCOC = max(COC.r, COC.g);
    float2 UVDir = float2(0, InvTexSize.y);
    
    if(bHorizontal)
    {
        UVDir = float2(InvTexSize.x, 0);
    }
    
    float MaxLuminance = 0;;
    float3 MaxColor = float3(0, 0, 0);
    float3 SceneColor;
    int halfRange = Range / 2;
    for (int i = -halfRange; i <= halfRange; ++i)
    {
        float2 CurUV = uv + UVDir * i;
        float3 CurColor = g_SceneColorTex.Sample(g_PointClampSample, CurUV).rgb;
        if(i == 0)
        {
            SceneColor = CurColor;
        }
        float Luminance = GetLuminance(CurColor) * absCOC;
        if(Luminance > MaxLuminance)
        {
            MaxLuminance = Luminance;
            MaxColor = CurColor;
        }
    }
    //return float4(SceneColor, 1);
    if (absCOC < 0.1f)
    {
        return float4(SceneColor, 1);
    }
    
    return float4(MaxColor, 1);
}
