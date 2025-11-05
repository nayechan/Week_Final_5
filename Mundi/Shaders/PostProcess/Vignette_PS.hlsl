Texture2D    g_SceneColorTex   : register(t0);

SamplerState g_LinearClampSample  : register(s0);

struct PS_INPUT
{
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD0;
};

cbuffer PostProcessCB : register(b0)
{
    float Near;
    float Far;
}

cbuffer ViewProjBuffer : register(b1)
{
    row_major float4x4 ViewMatrix;
    row_major float4x4 ProjectionMatrix;
    row_major float4x4 InverseViewMatrix;
    row_major float4x4 InverseProjectionMatrix;
}

cbuffer VignetteCB : register(b2)
{
    float4 VignetteColor;    // target color
    
    float  Radius;
    float  Softness;
    float  Intensity;
    float  Roundness;
    
    float  Weight;
    float  _Pad0[3];
}

cbuffer ViewportConstants : register(b10)
{
    // x: Viewport TopLeftX, y: Viewport TopLeftY
    // z: Viewport Width,   w: Viewport Height
    float4 ViewportRect;
    
    // x: Screen Width      (전체 렌더 타겟 너비)
    // y: Screen Height     (전체 렌더 타겟 높이)
    // z: 1.0f / Screen W,  w: 1.0f / Screen H
    float4 ScreenSize;
}

float CalculateDistanceFromVignetteCenter(float2 textureCoordinates)
{
    // 뷰포트의 시작 UV 좌표 (전체 화면 기준)
    float2 ViewportStartUV = ViewportRect.xy * ScreenSize.zw;
    // 뷰포트의 UV 공간 크기 (전체 화면 기준)
    float2 ViewportUVSpan = ViewportRect.zw * ScreenSize.zw;
    
    // Center (픽셀 단위, 뷰포트 좌상단 기준) -> UV(0..1)로 정규화
    float2 CenterUV;
    CenterUV.x = (ViewportUVSpan.x * 0.5 + ViewportStartUV.x);
    CenterUV.y = (ViewportUVSpan.y * 0.5 + ViewportStartUV.y);
    
    // 뷰포트 로컬 UV에서의 오프셋
    float2 CenterOffset = textureCoordinates - CenterUV;
    
    // 화면비 보정: 원이 찌그러지지 않도록 x를 H/W로 스케일
    float AspectRatio = (ViewportUVSpan.y <= 0.0f) ? 1.0f : (ViewportUVSpan.x / ViewportUVSpan.y);
    CenterOffset.x *= (1.0f / max(AspectRatio, 1e-6));
    
    // Roundness: n>=1, n 커질수록 모서리가 각지게
    float NormExponent = max(1.0f, Roundness);
    
    float2 AbsolutePosition = abs(CenterOffset) + 1e-8; // zero-div 방지
    float Distance = pow(pow(AbsolutePosition.x, NormExponent) + pow(AbsolutePosition.y, NormExponent), 1.0f / NormExponent);
    
    return Distance; // 중심=0, 바깥쪽으로 증가
}

float4 mainPS(PS_INPUT input) : SV_Target
{
    float4 SceneColor = g_SceneColorTex.Sample(g_LinearClampSample, input.texCoord);
    
    // 0이 중앙, 1이 가장자리
    float Distance = CalculateDistanceFromVignetteCenter(input.texCoord);
    
    float MaxColor = Radius + max(Softness, 1e-5);
    float K = saturate(smoothstep(Radius, MaxColor, Distance) * Weight);
    
    // 가장자리 색 계산
    float3 EdgeColor = lerp(SceneColor.rgb, VignetteColor.rgb, saturate(Intensity));
    
    float3 OutRgb = lerp(SceneColor.rgb, EdgeColor.rgb, K);
    
    return float4(OutRgb, SceneColor.a);
}
