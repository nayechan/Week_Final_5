#include "../Common/PostProcessCommon.hlsl"

Texture2D<float4> g_InputTexture : register(t0);  // Near 또는 Far 필드 (RGB=Color, A=CoC)
SamplerState g_LinearSampler : register(s0);

// Prefilter: 블러 전에 밝은 영역을 부스트하여 보케 하이라이트 개선
// 동시에 CoC 기반 가중 평균으로 품질 향상
float4 mainPS(float4 position : SV_Position, float2 texcoord : TEXCOORD0) : SV_Target
{
    float4 centerSample = g_InputTexture.Sample(g_LinearSampler, texcoord);
    float centerCoC = centerSample.a;

    // CoC 임계점 미만이면 원본 그대로 반환 (검은색 라인 방지)
    if (centerCoC < 0.05)
    {
        return centerSample;
    }

    // 3x3 다운샘플링 + 하이라이트 부스트
    float2 pixelSize = 1.0 / ScreenSize.xy;

    float4 colorSum = float4(0, 0, 0, 0);
    float weightSum = 0.0;

    // 3x3 커널로 다운샘플링
    [unroll]
    for (int y = -1; y <= 1; ++y)
    {
        [unroll]
        for (int x = -1; x <= 1; ++x)
        {
            float2 offset = float2(x, y) * pixelSize;
            float4 sample = g_InputTexture.Sample(g_LinearSampler, texcoord + offset);

            // 밝기 계산
            float luminance = dot(sample.rgb, float3(0.2126, 0.7152, 0.0722));

            // 밝은 영역 5배 증폭 (하이라이트 강조, 아티팩트 완화)
            float boost = 1.0 + saturate((luminance - 0.5) * 2.0) * 5.0;
            sample.rgb *= boost;

            // CoC 기반 부드러운 가중치 (하드 임계점 제거)
            float cocWeight = smoothstep(0.0, 0.05, sample.a);
            float weight = cocWeight;

            colorSum += sample * weight;
            weightSum += weight;
        }
    }

    // 가중 평균 (최소 가중치 보장으로 검은색 방지)
    if (weightSum > 0.001)
    {
        colorSum /= weightSum;
        colorSum.a = centerCoC;  // 중심 픽셀의 CoC 유지
        return colorSum;
    }

    return centerSample;
}
