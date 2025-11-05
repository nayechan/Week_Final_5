#pragma once
#include "CameraModifierBase.h"

class UCamMod_Fade : public UCameraModifierBase
{
public:
    DECLARE_CLASS(UCamMod_Fade, UCameraModifierBase)

    UCamMod_Fade();
    virtual ~UCamMod_Fade() = default;
    FLinearColor FadeColor = FLinearColor::Zero();
    float StartAlpha = 0.f;   // 시작 불투명도 [0..1]
    float EndAlpha   = 1.f;   // 종료 불투명도 [0..1]
    float Duration   = 1.f;   // 총 소요 시간(초)
    float Elapsed    = 0.f;   // 진행 시간(초)

    float CurrentAlpha = 0.f; // 이번 프레임 출력 알파
    
    virtual void ApplyToView(float DeltaTime, FSceneView& InOutView) override
    {
        if (!bEnabled) return;

        Elapsed += DeltaTime;
        const float T = (Duration <= 0.f) ? 1.f : FMath::Clamp(Elapsed / Duration, 0.f, 1.f);

        CurrentAlpha = FMath::Lerp(StartAlpha, EndAlpha, T);

        // 완료 처리
        if (T >= 1.f) bEnabled = false;
    }

    virtual void CollectPostProcess(TArray<FPostProcessModifier>& Out, const FSceneView&) override
    {
        if (!bEnabled && CurrentAlpha <= 0.f) return;
        if (CurrentAlpha <= 0.f) return; 

        FPostProcessModifier M;
        M.Type = EPostProcessEffectType::Fade;
        M.Priority = Priority;
        M.bEnabled = true;
        M.Weight = Weight;
        M.SourceObject = this;

        M.Payload.Color = FadeColor;
        M.Payload.Params0 = FVector4(CurrentAlpha, 0, 0, 0);

        Out.Add(M);
    }
};


