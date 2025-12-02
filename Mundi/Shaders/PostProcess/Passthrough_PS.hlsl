#include "../Common/PostProcessCommon.hlsl"

Texture2D<float4> g_InputTexture : register(t0);
SamplerState g_PointSampler : register(s0);

// Simple 1:1 passthrough shader - outputs input as-is
float4 mainPS(float4 position : SV_Position, float2 texcoord : TEXCOORD0) : SV_Target
{
    return g_InputTexture.Sample(g_PointSampler, texcoord);
}
