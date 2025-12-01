#include "../Common/PostProcessCommon.hlsl"

Texture2D<float> g_SceneDepthTexture : register(t0); // SceneDepth
SamplerState g_PointSampler : register(s0);

float4 mainPS(float4 position : SV_Position, float2 texcoord : TEXCOORD0) : SV_Target
{
    float rawDepth = g_SceneDepthTexture.Sample(g_PointSampler, texcoord);

    float viewZ;
    if (PostProcessBuffer.IsOrthographic == 1)
    {
        viewZ = PostProcessBuffer.Near + rawDepth * (PostProcessBuffer.Far - PostProcessBuffer.Near);
    }
    else
    {
        // 원근 투영: 클립 공간 깊이(0-1)에서 뷰 공간 깊이로 변환
        // viewZ가 양수가 되도록 처리
        viewZ = PostProcessBuffer.Far * PostProcessBuffer.Near / (PostProcessBuffer.Far - rawDepth * (PostProcessBuffer.Far - PostProcessBuffer.Near));
    }
    
    // CoC(Circle of Confusion) 계산
    // Aperture = FocalLength / FNumber
    float aperture = FDepthOfFieldBuffer.FocalLength / FDepthOfFieldBuffer.FNumber;
    
    // 초점면보다 뒤(Far) or 앞(Near)에 있는지에 따라 CoC 계산
    // coc = | (viewZ - FocalDistance) / viewZ | * ( (FocalLength * aperture) / (FocalDistance - FocalLength) )
    // 위 수식은 viewZ가 FocalDistance와 같을 때 0이 되며, 멀거나 가까울수록 절대값이 커짐.
    // (FocalDistance - FocalLength)가 0이 되는 경우 방지
    float coc = 0.0f;
    if (abs(FDepthOfFieldBuffer.FocalDistance - FDepthOfFieldBuffer.FocalLength) > 1e-5f)
    {
        coc = (aperture * FDepthOfFieldBuffer.FocalLength) / (FDepthOfFieldBuffer.FocalDistance 
        - FDepthOfFieldBuffer.FocalLength) 
        * abs(viewZ - FDepthOfFieldBuffer.FocalDistance) / viewZ;
    }

    // 최대 CoC 값(MaxCoc)으로 정규화 및 클램프
    coc = saturate(coc / FDepthOfFieldBuffer.MaxCoc);

    // ===== 부드러운 전환 영역 처리 (윤곽선 아티팩트 제거) =====

    // Near/Far 필드 구분 + Transition Range 적용
    float farCoC = 0.0f;
    float nearCoC = 0.0f;

    if (viewZ > FDepthOfFieldBuffer.FocalDistance)
    {
        // Far 영역: FocalDistance부터 멀어질수록 CoC 증가
        // Transition Range를 사용하여 부드럽게 블렌딩
        float distanceFromFocus = viewZ - FDepthOfFieldBuffer.FocalDistance;
        float transitionFactor = smoothstep(0.0, FDepthOfFieldBuffer.FarTransitionRange, distanceFromFocus);
        farCoC = coc * transitionFactor;
    }
    else if (viewZ < FDepthOfFieldBuffer.FocalDistance)
    {
        // Near 영역: FocalDistance부터 가까워질수록 CoC 증가
        // Transition Range를 사용하여 부드럽게 블렌딩
        float distanceFromFocus = FDepthOfFieldBuffer.FocalDistance - viewZ;
        float transitionFactor = smoothstep(0.0, FDepthOfFieldBuffer.NearTransitionRange, distanceFromFocus);
        nearCoC = coc * transitionFactor;
    }

    // R: Far CoC, G: Near CoC, B: 0, A: RawDepth
    return float4(farCoC, nearCoC, 0, rawDepth);
}