#ifndef POST_PROCESS_COMMON_HLSL
#define POST_PROCESS_COMMON_HLSL

// b0
struct FPostProcessData
{
    float Near;
    float Far;
    int IsOrthographic;
    float padding;
};
cbuffer cbPostProcess : register(b0)
{
    FPostProcessData PostProcessBuffer;
};

// b2
struct FDepthOfFieldData
{
    float FocalDistance;
    float FocalLength;
    float FNumber;
    float MaxCoc;
    float NearTransitionRange;
    float FarTransitionRange;
    float NearBlurScale;
    float FarBlurScale;
    int BlurSampleCount;
    float BokehRotation;
    float Weight;
    float _Pad;
};
cbuffer cbDepthOfField : register(b2)
{
    FDepthOfFieldData FDepthOfFieldBuffer;
};

// b10
cbuffer FViewportConstants : register(b10)
{
    float4 ViewportRect;
    float4 ScreenSize; // 얘는 전역 변수처럼 쓰도록 기존 유지 (코드 수정 최소화)
};

#endif