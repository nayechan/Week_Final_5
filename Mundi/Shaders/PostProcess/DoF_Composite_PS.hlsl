#include "../Common/PostProcessCommon.hlsl"

Texture2D<float4> g_SceneColorSource : register(t0); // 원본 씬 컬러
Texture2D<float4> g_DofBlurMap : register(t1);     // 최종 블러 결과
Texture2D<float4> g_DofCocMap : register(t2);      // CoC 맵 (R=Far, G=Near)

SamplerState g_LinearSampler : register(s0);

float4 mainPS(float4 position : SV_Position, float2 texcoord : TEXCOORD0) : SV_Target
{
    float4 originalColor = g_SceneColorSource.Sample(g_LinearSampler, texcoord);
    float4 blurredColor = g_DofBlurMap.Sample(g_LinearSampler, texcoord);
    float4 cocSample = g_DofCocMap.Sample(g_LinearSampler, texcoord);

    float farCoC = cocSample.r;
    float nearCoC = cocSample.g;

    // Near/Far CoC 중 더 큰 값을 최종 블러 강도(혼합 비율)로 사용
    float blendFactor = max(nearCoC, farCoC);

    // FDepthOfFieldBuffer.Weight를 사용하여 전체 DoF 효과의 강도를 조절
    blendFactor *= FDepthOfFieldBuffer.Weight;

    // 블러 색상 유효성 확인 (알파 채널 기반)
    float blurValid = saturate(blurredColor.a * 10.0);  // 부드러운 페이드인

    // ===== 부드러운 전환 영역 처리 (윤곽선 아티팩트 제거) =====

    // 1. CoC를 부드럽게 램핑 (0.05 임계점 주변을 부드럽게)
    // smoothstep으로 0.02~0.08 사이를 부드럽게 전환
    float cocRamp = smoothstep(0.02, 0.08, blendFactor);

    // 2. 블러 유효성도 부드럽게 적용
    float finalBlendFactor = cocRamp * blurValid;

    // 3. 원본과 블러를 부드럽게 블렌딩 (if문 제거하여 하드 컷 방지)
    float4 finalColor = lerp(originalColor, blurredColor, saturate(finalBlendFactor));

    // 알파 채널은 원본의 값을 유지
    finalColor.a = originalColor.a;

    return finalColor;
}
