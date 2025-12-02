#include "../Common/PostProcessCommon.hlsl"

Texture2D<float4> g_InputTexture : register(t0);    // Near 또는 Far 필드 (RGB=Color, A=CoC)
Texture2D<float4> g_CoCTexture : register(t1);      // 원본 CoC Map (R=Far, G=Near, A=Depth)
SamplerState g_LinearSampler : register(s0);

// 2차 패스: 60도 방향 블러
#define BLUR_DIRECTION float2(0.5, 0.8660254)

// 회전 행렬 헬퍼 함수
float2 RotateVector(float2 v, float angle)
{
    float cosAngle = cos(angle);
    float sinAngle = sin(angle);
    return float2(
        v.x * cosAngle - v.y * sinAngle,
        v.x * sinAngle + v.y * cosAngle
    );
}

float4 mainPS(float4 position : SV_Position, float2 texcoord : TEXCOORD0) : SV_Target
{
    // 입력 텍스처에서 현재 픽셀의 색상과 CoC 읽기
    float4 centerSample = g_InputTexture.Sample(g_LinearSampler, texcoord);
    float centerCoC = centerSample.a;  // 알파 채널에 CoC가 저장되어 있음

    // CoC 임계점 미만이면 원본 그대로 반환 (검은색 라인 방지)
    if (centerCoC < 0.05)
    {
        return centerSample;
    }

    // Hexagonal blur 적용
    float4 colorSum = float4(0.0, 0.0, 0.0, 0.0);
    float weightSum = 0.0;

    // cbuffer에서 샘플 개수와 블러 스케일 가져오기
    int sampleCount = max(1, FDepthOfFieldBuffer.BlurSampleCount);
    int kernelRadius = max(1, sampleCount / 2);

    // 텍스처 해상도 (ScreenSize.xy에서 가져오기)
    float2 pixelSize = 1.0 / ScreenSize.xy;

    // CoC 텍스처에서 Near/Far 구분하여 적절한 블러 스케일 적용
    float4 cocSample = g_CoCTexture.Sample(g_LinearSampler, texcoord);
    float farCoC = cocSample.r;
    float nearCoC = cocSample.g;

    // Near와 Far 중 어느 것이 더 큰지 확인하여 적절한 스케일 사용
    float blurScaleMultiplier = (nearCoC > farCoC) ? FDepthOfFieldBuffer.NearBlurScale : FDepthOfFieldBuffer.FarBlurScale;

    // 블러 범위 계산: Dilated CoC 사용 (전경 블러가 배경으로 번지도록)
    // Near 블러는 주변의 최대 CoC를 사용해야 영역이 축소되지 않음
    float blurRadius = (nearCoC > farCoC) ? max(nearCoC, centerCoC) : centerCoC;
    float blurScale = blurRadius * blurScaleMultiplier * 8.0;

    // BokehRotation 적용
    float2 rotatedDirection = RotateVector(BLUR_DIRECTION, FDepthOfFieldBuffer.BokehRotation);

    // 육각형 보케를 위한 방향성 블러 적용
    // 육각형의 테두리에 더 많은 샘플을 배치하여 보케 모양 생성
    [loop]
    for (int i = -kernelRadius; i <= kernelRadius; i++)
    {
        float t = float(i) / float(kernelRadius);  // -1 ~ 1

        // 육각형 모양을 위한 오프셋 계산
        float2 offset = rotatedDirection * t * pixelSize * blurScale;
        float2 sampleUV = texcoord + offset;

        float4 sampleColor = g_InputTexture.Sample(g_LinearSampler, sampleUV);
        float sampleCoC = sampleColor.a;

        // 육각형 보케 가중치 계산
        float edgeDistance = abs(t);  // 0 (중심) ~ 1 (가장자리)

        // 밝은 영역 강조 (보케 하이라이트)
        float luminance = dot(sampleColor.rgb, float3(0.2126, 0.7152, 0.0722));
        float brightnessBoost = 1.0 + saturate((luminance - 0.5) * 2.0) * 2.5;

        // 육각형 테두리 강조 - 더 부드러운 곡선으로 라인 아티팩트 완화
        // smoothstep 사용으로 매우 부드러운 전환
        float hexWeight = lerp(0.6, 1.0, smoothstep(0.0, 1.0, edgeDistance));

        // CoC 기반 샘플 가중치
        // Near 블러: 전경(높은 CoC)에서 배경(낮은 CoC)으로 번져야 하므로 CoC가 작은 샘플도 포함
        // Far 블러: 배경(높은 CoC)이 전경(낮은 CoC)으로 번지면 안 되므로 CoC 차이 체크 필요
        float cocDiff = abs(sampleCoC - centerCoC);
        float cocWeight;
        if (nearCoC > farCoC)
        {
            // Near 블러: sampleCoC가 centerCoC보다 작아도 허용 (전경->배경 번짐)
            cocWeight = (sampleCoC <= centerCoC) ? 1.0 : saturate(1.0 - cocDiff * 3.0);
        }
        else
        {
            // Far 블러: 비슷한 CoC만 허용 (bleeding 방지)
            cocWeight = saturate(1.0 - cocDiff * 3.0);
        }

        // 가우시안 유사 감쇠 (라인 아티팩트 완화)
        // 중심에서 멀어질수록 부드럽게 감소
        float gaussianFalloff = exp(-1.5 * edgeDistance * edgeDistance);

        // 최종 가중치
        float weight = hexWeight * brightnessBoost * cocWeight * gaussianFalloff;

        // 임계점 대신 부드러운 페이드 적용 (검은색 제거)
        weight *= smoothstep(0.0, 0.05, sampleCoC);

        colorSum += sampleColor * weight;
        weightSum += weight;
    }

    // 가중 평균 계산 (최소 가중치 보장으로 검은색 방지)
    if (weightSum > 0.001)
    {
        colorSum /= weightSum;
        colorSum.a = centerCoC;  // CoC 값 유지
        return colorSum;
    }

    // 가중치 합이 너무 작으면 원본 반환
    return centerSample;
}
