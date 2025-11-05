#pragma once
#include "SceneView.h"

class UCameraModifierBase : public UObject
{
public:
    DECLARE_CLASS(UCameraModifierBase, UObject)

public:
    UCameraModifierBase();
    virtual ~UCameraModifierBase() =default;
    
    int32 Priority = 0;     // 낮을수록 먼저
    bool  bEnabled = true;
    float Weight  = 1.0f;  // 0~1, 모디파이어 내부 블렌딩 스케일
    float Duration = -1.f; // <0: 무한 / >=0: 수명
    float Elapsed = 0.f;
    
    // 전처리: 카메라 트랜스폼에 작용 (Shake, SpringArm 보정, FOV/Aspect 조정 등)
    virtual void ApplyToView(float DeltaTime, FSceneView& InOutView);

    // 후처리: 이번 프레임에 필요한 PP 모디파이어 수집 (Fade/Vignette/Letterbox 등)
    virtual void CollectPostProcess(TArray<FPostProcessModifier>& OutModifiers, const FSceneView&  View);

    // 수명/활성 관리
    virtual void TickLifetime(float DeltaTime)
    {
        if (Duration >= 0.f) { Elapsed += DeltaTime; if (Elapsed >= Duration) bEnabled = false; }
    }
    // 정렬 키: Priority -> 타입 우선순위(선택) -> 생성순
    bool operator<(const UCameraModifierBase& Other) const { return Priority < Other.Priority; }
};
