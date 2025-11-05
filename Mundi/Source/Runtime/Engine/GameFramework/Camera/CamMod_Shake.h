#pragma once
#include "CameraModifierBase.h"

class UCamMod_Shake : public UCameraModifierBase
{
public:
    DECLARE_CLASS(UCamMod_Shake, UCameraModifierBase)

    UCamMod_Shake();
    virtual ~UCamMod_Shake() = default;
    
    float Duration   = 0.20f;   // 총 재생 시간(초)
    float BlendIn    = 0.02f;   // 짧게 들어오기
    float BlendOut   = 0.10f;   // 서서히 사라지기

    float AmplitudeLocation     = 1.5f;    // 위치 진폭(cm) - 카메라 로컬
    float AmplitudeRotationDegrees  = 2.0f;    // 회전 진폭(deg)  - pitch/yaw/roll 균등
    float Frequency  = 8.0f;    // 기본 주파수(Hz)

    // TODO 
    // 2D Bezier 엔벌로프: p0=(0,0), p3=(1,1) 고정. p1/p2만 수정.
    bool  bUseBezier = false;
    ImVec2 BezierP1  = ImVec2(0.25f, 0.0f);
    ImVec2 BezierP2  = ImVec2(0.0f,  1.0f);

    // 엔진 루프
    void Initialize(float InDuration, float InAmpLoc, float InAmpRotDeg, float InFrequency);
    virtual void ApplyToView(float DeltaTime, FSceneView& InOutView) override;
    virtual void TickLifetime(float DeltaTime) override;

    // TODO 
    // 디버그 UI 
    void DrawImGui();

private:
    float Time = 0.0f;
    float EnvelopeAlpha = 0.0f;

    // 간단한 결정적 위상값
    float PhaseX = 1.1f, PhaseY = 2.3f, PhaseZ = 3.7f, PhaseR = 5.5f;

    // 보조
    float EvalEnvelope01_NoBezier(float t, float Duration, float BlendIn, float BlendOut);
    float EvalEnvelope01(float t) const;                      // 0..1 -> 0..1
    static float BezierEase(float t, ImVec2 p1, ImVec2 p2);   // x=시간, y=값 (이진탐색 역함수)
    static float Cubic(float t, float p0, float p1, float p2, float p3);
};