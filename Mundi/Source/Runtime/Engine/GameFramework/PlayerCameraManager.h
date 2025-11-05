#pragma once
#include "Actor.h"

class UCameraComponent;
class UCameraModifierBase;
class FSceneView;
class FViewport;
class URenderSettings;
class UCamMod_Fade;

class APlayerCameraManager : public AActor
{
	DECLARE_CLASS(APlayerCameraManager, AActor)
	GENERATED_REFLECTION_BODY()

public:
	APlayerCameraManager();

	TArray<UCameraModifierBase*> ActiveModifiers;

	void StartCameraShake(float InDuration, float AmpLoc, float AmpRotDeg, float Frequency, int32 InPriority = 0);
	void StartFade(float InDuration, float FromAlpha, float ToAlpha, const FLinearColor& InColor, int32 InPriority=0);
	
	inline void FadeIn(float Duration, const FLinearColor& Color=FLinearColor::Zero(), int32 Priority=0)
	{   // 검은 화면(1) → 씬(0)
		StartFade(Duration, 1.f, 0.f, Color, Priority);
	}
	inline void FadeOut(float Duration, const FLinearColor& Color=FLinearColor::Zero(), int32 Priority=0)
	{   // 씬(0) → 검은 화면(1)
		StartFade(Duration, 0.f, 1.f, Color, Priority);
	}
	
	void AddModifier(UCameraModifierBase* Modifier)
	{
		ActiveModifiers.Add(Modifier);	
	}
	
	void BuildForFrame(float DeltaTime);
	
protected:
	~APlayerCameraManager() override;

public:
	void Destroy() override;
	// Actor의 메인 틱 함수
	void Tick(float DeltaTime) override;
	void UpdateCamera(float DeltaTime);

	void SetMainCamera(UCameraComponent* InCamera) { CurrentViewTarget = InCamera; };
	UCameraComponent* GetMainCamera();

	FSceneView* GetSceneView(FViewport* InViewport, URenderSettings* InRenderSettings);
	
	FSceneView* GetBaseViewInfo(UCameraComponent* ViewTarget);
	void SetViewTarget(UCameraComponent* NewViewTarget);
	void SetViewTargetWithBlend(UCameraComponent* NewViewTarget, float InBlendTime);

	DECLARE_DUPLICATE(APlayerCameraManager)

private:
	UCameraComponent* CurrentViewTarget{};
	UCameraComponent* PendingViewTarget{};
	
	float LastDeltaSeconds = 0.f;

	FSceneView* SceneView{};
	FSceneView* BlendStartView{};

	float BlendTimeTotal;
	float BlendTimeRemaining;
};


