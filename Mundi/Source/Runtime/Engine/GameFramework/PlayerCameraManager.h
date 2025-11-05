#pragma once
#include "Actor.h"
#include "SceneView.h"

class UCameraComponent;
class UCameraModifierBase;
class FViewport;
class URenderSettings;
class UCamMod_Fade;

class APlayerCameraManager : public AActor
{
	DECLARE_CLASS(APlayerCameraManager, AActor)
	GENERATED_REFLECTION_BODY()

public:
	APlayerCameraManager() { Name = "Player Camera Manager";  };

	void StartCameraShake(float InDuration, float AmpLoc, float AmpRotDeg, float Frequency, int32 InPriority = 0);
	
	void StartFade(float InDuration, float FromAlpha, float ToAlpha, const FLinearColor& InColor= FLinearColor::Zero(), int32 InPriority = 0);
	
	void StartLetterBox(float InDuration, float Aspect, float BarHeight, const FLinearColor& InColor = FLinearColor::Zero(), int32 InPriority = 0);
	
	int StartVignette(float InDuration, float Radius, float Softness, float Intensity, float Roundness, const FLinearColor& InColor = FLinearColor::Zero(), int32 InPriority = 0);
	int UpdateVignette(int Idx, float InDuration, float Radius, float Softness, float Intensity, float Roundness, const FLinearColor& InColor = FLinearColor::Zero(), int32 InPriority = 0);
	void AdjustVignette(float InDuration, float Radius, float Softness, float Intensity, float Roundness, const FLinearColor& InColor = FLinearColor::Zero(), int32 InPriority = 0);
	void DeleteVignette();

protected:
	~APlayerCameraManager() override;

	void AddModifier(UCameraModifierBase* Modifier)
	{
		ActiveModifiers.Add(Modifier);
	}

	void BuildForFrame(float DeltaTime);

public:
	TArray<UCameraModifierBase*> ActiveModifiers;

	inline void FadeIn(float Duration, const FLinearColor& Color=FLinearColor::Zero(), int32 Priority=0)
	{   // 검은 화면(1) → 씬(0)
		StartFade(Duration, 1.f, 0.f, Color, Priority);
	}
	inline void FadeOut(float Duration, const FLinearColor& Color = FLinearColor::Zero(), int32 Priority = 0)
	{   // 씬(0) → 검은 화면(1)
		StartFade(Duration, 0.f, 1.f, Color, Priority);
	}

public:
	void Destroy() override;
	// Actor의 메인 틱 함수
	void Tick(float DeltaTime) override;

	// TODO: SetViewTarget 로 통합
	void SetMainCamera(UCameraComponent* InCamera)
	{
		CurrentViewTarget = InCamera;
	};
	UCameraComponent* GetMainCamera();

	void CacheViewport(FViewport* InViewport) { CachedViewport = InViewport; }
	FMinimalViewInfo* GetSceneView();

	void SetViewTarget(UCameraComponent* NewViewTarget);
	void SetViewTargetWithBlend(UCameraComponent* NewViewTarget, float InBlendTime);

	DECLARE_DUPLICATE(APlayerCameraManager)

	TArray<FPostProcessModifier> GetModifiers() { return Modifiers; };

	void UpdateViewTarget(float DeltaTime);

private:
	UCameraComponent* CurrentViewTarget{};
	UCameraComponent* PendingViewTarget{};

	// TODO: 추후 CurrentMinimalViewInfo 와 비슷하게 이름 변경 필요

	// TODO : 감싸기 or 배열로 관리, 현재 vignette 1개만 Update 가능
	// Vignette 연속 효과를 위한 IDX
	int LastVignetteIdx = 0;

	FMinimalViewInfo SceneView{};
	FMinimalViewInfo BlendStartView{};

	TArray<FPostProcessModifier> Modifiers;

	FViewport* CachedViewport = nullptr;

	float BlendTimeTotal;
	float BlendTimeRemaining;

	float MyCurve[5] = { 0.950f, 0.050f, 0.795f, 0.035f, 6.0f };
};
