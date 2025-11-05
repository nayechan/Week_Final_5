#include "pch.h"
#include "CamMod_Shake.h"
#include "SceneView.h"
#include <cmath>
#include <algorithm>

UCamMod_Shake::UCamMod_Shake()
{
    Priority = 0;
    bEnabled = true;
    Weight   = 1.0f;
    Duration = -1.f;
}

void UCamMod_Shake::Initialize(float InDuration, float InAmplitudeLocation, float InAmplitudeRotationDegrees, float InFrequency)
{
    Duration                 = InDuration;
    AmplitudeLocation        = InAmplitudeLocation;
    AmplitudeRotationDegrees = InAmplitudeRotationDegrees;
    Frequency                = InFrequency;

    Time          = 0.f;
    EnvelopeAlpha = 0.f;
}

static inline float Sin2Pi(float X) { return std::sin(X * 6.28318530718f); }

float UCamMod_Shake::Cubic(float T, float P0, float P1, float P2, float P3)
{
    float U = 1.f - T;
    return U*U*U*P0 + 3*U*U*T*P1 + 3*U*T*T*P2 + T*T*T*P3;
}

float UCamMod_Shake::BezierEase(float T, ImVec2 P1, ImVec2 P2)
{
    // P0=(0,0), P3=(1,1). X축은 시간 재매핑, Y축은 값.
    // X(S) ~= T 인 S를 이진탐색으로 찾아서 Y(S) 반환.
    float S0 = 0.f, S1 = 1.f, S = T;
    for (int I = 0; I < 10; ++I)
    {
        S = 0.5f * (S0 + S1);
        float X = Cubic(S, 0.f, P1.x, P2.x, 1.f);
        if (X < T) S0 = S; else S1 = S;
    }
    return Cubic(S, 0.f, P1.y, P2.y, 1.f);
}

float UCamMod_Shake::EvalEnvelope01_NoBezier(float T, float InDuration, float InBlendIn, float InBlendOut)
{
    if (InDuration <= 0.f) return 1.f;
    float X = FMath::Clamp(T / InDuration, 0.f, 1.f);

    float BlendInAlpha  = (InBlendIn  > 0.f) ? FMath::Clamp(X / InBlendIn, 0.f, 1.f) : 1.f;
    float BlendOutAlpha = (InBlendOut > 0.f) ? FMath::Clamp((1.f - X) / InBlendOut, 0.f, 1.f) : 1.f;
    float Alpha = FMath::Min(BlendInAlpha, BlendOutAlpha);

    // 스무스스텝
    return Alpha*Alpha*(3.f - 2.f*Alpha);
}

float UCamMod_Shake::EvalEnvelope01(float T) const
{
    // 0..Duration → 0..1
    if (Duration <= 0.f) return 1.f;
    float X = std::clamp(T / Duration, 0.f, 1.f);

    // BlendIn/Out
    float BlendInAlpha  = (BlendIn  > 0.f) ? std::clamp(X / BlendIn, 0.f, 1.f) : 1.f;
    float BlendOutAlpha = (BlendOut > 0.f) ? std::clamp((1.f - X) / BlendOut, 0.f, 1.f) : 1.f;
    float Alpha = std::min(BlendInAlpha, BlendOutAlpha);

    // 곡선 적용
    if (bUseBezier) return std::clamp(BezierEase(Alpha, BezierP1, BezierP2), 0.f, 1.f);

    // 기본 스무스스텝
    return Alpha*Alpha*(3.f - 2.f*Alpha);
}

void UCamMod_Shake::ApplyToView(float DeltaTime, FSceneView& InOutSceneView)
{
    if (!bEnabled || Weight <= 0.f) return;
    
    // TODO : Dilation 넣기! Slomo, Hit Stop 병합 후 개선
    // const float ScaledDeltaTime = DeltaTime * GWorld->GetGlobalTimeDilation();
    const float ScaledDeltaTime = DeltaTime;
    Time += ScaledDeltaTime;

    EnvelopeAlpha = EvalEnvelope01(Time) * Weight;
    if (EnvelopeAlpha <= 1e-4f) return;

    // 간단한 사인 기반 3축 + 회전(균등)
    const float ClampedFrequency = std::max(0.01f, Frequency);
    float SineX        = Sin2Pi(Time * (ClampedFrequency * 1.00f) + PhaseX);
    float SineY        = Sin2Pi(Time * (ClampedFrequency * 1.18f) + PhaseY);
    float SineZ        = Sin2Pi(Time * (ClampedFrequency * 1.37f) + PhaseZ);
    float SineRotation = Sin2Pi(Time * (ClampedFrequency * 0.77f) + PhaseR);

    // 로컬 공간 오프셋
    FVector LocalLocation     = FVector(SineX, SineY, SineZ) * (AmplitudeLocation * EnvelopeAlpha);
    FVector LocalEulerDegrees = FVector(SineRotation, SineRotation, SineRotation) * (AmplitudeRotationDegrees * EnvelopeAlpha);

    // 카메라 로컬 → 월드
    const FQuat CameraQuaternion = InOutSceneView.ViewRotation;
    FVector WorldOffset    = CameraQuaternion.RotateVector(LocalLocation);
    FQuat   RotationOffset = FQuat::MakeFromEulerZYX(LocalEulerDegrees);

    InOutSceneView.ViewLocation += WorldOffset;
    InOutSceneView.ViewRotation  = (RotationOffset * InOutSceneView.ViewRotation).GetNormalized();
}

void UCamMod_Shake::TickLifetime(float DeltaTime)
{
    if (Duration > 0.f && Time >= Duration && EnvelopeAlpha <= 1e-3f)
        bEnabled = false;
}

// TODO :
void UCamMod_Shake::DrawImGui()
{
    // if (ImGui::CollapsingHeader("CameraShake", ImGuiTreeNodeFlags_DefaultOpen))
    // {
    //     ImGui::DragFloat("Duration",  &Duration,  0.01f, 0.0f,  10.0f);
    //     ImGui::DragFloat("BlendIn",   &BlendIn,   0.005f,0.0f,  1.0f);
    //     ImGui::DragFloat("BlendOut",  &BlendOut,  0.005f,0.0f,  1.0f);
    //     ImGui::DragFloat("Frequency", &Frequency, 0.1f,  0.1f,  60.0f);
    //     ImGui::DragFloat("AmplitudeLocation",        &AmplitudeLocation,        0.1f,  0.0f,  20.0f);
    //     ImGui::DragFloat("AmplitudeRotationDegrees", &AmplitudeRotationDegrees, 0.1f,  0.0f,  30.0f);
    //
    //     ImGui::Checkbox("Use Bezier", &bUseBezier);
    //     if (bUseBezier)
    //     {
    //         ImGui::DragFloat2("P1 (x,y)", (float*)&BezierP1, 0.01f, 0.0f, 1.0f);
    //         ImGui::DragFloat2("P2 (x,y)", (float*)&BezierP2, 0.01f, 0.0f, 1.0f);
    //
    //         // 미니 프리뷰
    //         static float EnvelopeYValues[64];
    //         for (int I = 0; I < 64; ++I) {
    //             float T = (I) / (63.f);
    //             EnvelopeYValues[I] = BezierEase(T, BezierP1, BezierP2);
    //         }
    //         ImGui::PlotLines("Envelope", EnvelopeYValues, 64, 0, nullptr, 0.0f, 1.0f, ImVec2(220, 60));
    //     }
    // }
}