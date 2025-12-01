Texture2D g_SceneColorTex : register(t0);
Texture2D g_COCTex : register(t1);
Texture2D g_NearBlurTex : register(t2);
Texture2D g_FarBlurTex : register(t3);

SamplerState g_LinearClampSample : register(s0);
SamplerState g_PointClampSample : register(s1);

struct PS_INPUT
{
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD0;
};

float4 mainPS(PS_INPUT input) : SV_Target
{
    uint TexWidth, TexHeight;
    g_SceneColorTex.GetDimensions(TexWidth, TexHeight);
    float2 uv = float2(input.position.x / TexWidth, input.position.y / TexHeight);
    
    float2 COC = g_COCTex.Sample(g_PointClampSample, uv).rg;

    float Origin = 1 - COC.r - COC.g;
    float3 SceneColor = g_SceneColorTex.Sample(g_LinearClampSample, uv);
    float3 NearColor = g_NearBlurTex.Sample(g_LinearClampSample, uv);
    float3 FarColor = g_FarBlurTex.Sample(g_LinearClampSample, uv);
    
    float3 FinalColor = SceneColor * Origin + NearColor * COC.r + FarColor * COC.g;
    //return float4(SceneColor, 1);
    //return float4(FarColor, 1);
    //return float4(NearColor, 1);
    //return float4(COC.rg,0, 1);
    return float4(FinalColor, 1);
}
