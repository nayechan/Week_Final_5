Texture2D g_SceneColorTex : register(t0);
Texture2D g_COCTex : register(t1);
Texture2D g_BluredCOCTex : register(t2);
Texture2D g_NearBlurTex : register(t3);
Texture2D g_FarBlurTex : register(t4);

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
    
    float2 BluredCOC = g_BluredCOCTex.Sample(g_LinearClampSample, uv).rg;
    float2 COC = g_COCTex.Sample(g_LinearClampSample, uv).rg;
    
    float w0 = 0, w1 = 0, w2 = 0;
    int kind = 2;
    if(kind == 0)
    {
        w0 = max(1 - COC.r - COC.g, 0);
        w1 = BluredCOC.r;
        w2 = 1 - w1 - w0;
    }
    else if(kind == 1)
    {
        w0 = max(1 - BluredCOC.r - BluredCOC.g, 0);
        w1 = BluredCOC.r;
        w2 = BluredCOC.g;
    }
    else
    {
        w0 = max(1 - COC.r - COC.g, 0);
        w1 = BluredCOC.r;
        w2 = BluredCOC.g;

        float sum = w0 + w1 + w2;
        w0 /= sum;
        w1 /= sum;
        w2 /= sum;
    }
    
    float3 SceneColor = g_SceneColorTex.Sample(g_LinearClampSample, uv);
    float3 NearColor = g_NearBlurTex.Sample(g_LinearClampSample, uv);
    float3 FarColor = g_FarBlurTex.Sample(g_LinearClampSample, uv);

    float3 FinalColor = SceneColor * w0 + NearColor * w1 + FarColor * w2;

    //return float4(SceneColor, 1);
    //return float4(FarColor, 1);
    //return float4(NearColor, 1);
    //return float4(COC.rg,0, 1);
    return float4(FinalColor, 1);
}
