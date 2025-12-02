#include "../Common/PostProcessCommon.hlsl"

Texture2D<float4> g_NearBlurTexture : register(t0);  // Near 필드 블러 결과 (RGB=Color, A=CoC)
Texture2D<float4> g_FarBlurTexture : register(t1);   // Far 필드 블러 결과 (RGB=Color, A=CoC)
Texture2D<float4> g_CoCTexture : register(t2);       // 원본 CoC Map (R=Far, G=Near, A=Depth)
SamplerState g_LinearSampler : register(s0);

float4 mainPS(float4 position : SV_Position, float2 texcoord : TEXCOORD0) : SV_Target
{
    // Near와 Far 블러 결과 읽기 (알파 채널에 CoC 포함)
    float4 nearBlur = g_NearBlurTexture.Sample(g_LinearSampler, texcoord);
    float4 farBlur = g_FarBlurTexture.Sample(g_LinearSampler, texcoord);
    float4 cocSample = g_CoCTexture.Sample(g_LinearSampler, texcoord);

    float nearCoC = cocSample.g;  // 원본 CoC 맵에서 Near CoC
    float farCoC = cocSample.r;   // 원본 CoC 맵에서 Far CoC

    // ===== 부드러운 Near/Far 블렌딩 (윤곽선 아티팩트 제거) =====

    // Near와 Far의 가중치를 부드럽게 계산 (동일한 범위)
    float nearWeight = smoothstep(0.02, 0.08, nearCoC);
    float farWeight = smoothstep(0.02, 0.08, farCoC);

    // Near가 Far보다 우선순위가 높음 (전경이 배경을 가림)
    farWeight *= (1.0 - nearWeight);

    // 가중치 합 계산
    float totalWeight = nearWeight + farWeight;

    // Near와 Far를 부드럽게 블렌딩
    float4 finalBlur;
    if (totalWeight > 0.001)
    {
        // 정규화된 가중치로 블렌딩
        finalBlur = (nearBlur * nearWeight + farBlur * farWeight) / totalWeight;
    }
    else
    {
        // 가중치가 너무 작으면 Near와 Far의 평균 (검은색 방지)
        finalBlur = (nearBlur + farBlur) * 0.5;
    }

    // 최종 CoC 값 설정 (Near가 우선, 없으면 Far)
    float finalCoC = max(nearCoC, farCoC);
    finalBlur.a = finalCoC;

    return finalBlur;
}
