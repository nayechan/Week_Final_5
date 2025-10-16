Texture2D g_DepthTex : register(t0);

SamplerState g_PointClampSample : register(s1);

cbuffer PostProcessCB : register(b0)
{
    float Near;
    float Far;
    int IsOrthographic; // 0: Perspective, 1: Orthographic
}

struct VS_INPUT
{
    float3 position : POSITION; // Input position from vertex buffer
    float2 texCoord : TEXCOORD0;
};

struct PS_INPUT
{
    float4 position : SV_POSITION; // Transformed position to pass to the pixel shader
    float2 texCoord : TEXCOORD0;
};

PS_INPUT mainVS(VS_INPUT input)
{
    PS_INPUT output;
    
    output.position = float4(input.position, 1.0f);
    output.texCoord = input.texCoord;
    
    return output;
}

float4 mainPS(PS_INPUT input) : SV_TARGET
{
    float depth = g_DepthTex.Sample(g_PointClampSample, input.texCoord).r;

    float zView;
    if (IsOrthographic == 1)
    {
        // Orthographic
        zView = Near + depth * (Far - Near);
    }
    else
    {
        // Perspective
        zView = Near * Far / (Far - depth * (Far - Near));
    }
    //float zView = (2.0 * Near * Far) / (Far + Near - depth * (Far - Near));
    float NormalizedDepth = saturate((zView - Near) / (Far - Near));
    float FinalColor = float4(NormalizedDepth, NormalizedDepth, NormalizedDepth, 1.0f);

    return FinalColor;
}
