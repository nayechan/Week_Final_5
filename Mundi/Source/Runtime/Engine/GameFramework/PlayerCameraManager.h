#pragma once
#include "Actor.h"
#include "Camera/UCamMod_Fade.h"

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

	inline void FadeIn(float Duration, const FLinearColor& Color = FLinearColor::Zero(), int32 Priority = 0)
	{   // 검은 화면(1) → 씬(0)
		StartFade(Duration, 1.f, 0.f, Color, Priority);
	}
	inline void FadeOut(float Duration, const FLinearColor& Color = FLinearColor::Zero(), int32 Priority = 0)
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

	void StartFade(float InDuration, float FromAlpha, float ToAlpha, const FLinearColor& InColor, int32 InPriority = 0)
	{
		UCamMod_Fade* FadeModifier = new UCamMod_Fade();
		FadeModifier->Priority = InPriority;
		FadeModifier->bEnabled = true;

		FadeModifier->FadeColor = InColor;
		FadeModifier->StartAlpha = FMath::Clamp(FromAlpha, 0.f, 1.f);
		FadeModifier->EndAlpha = FMath::Clamp(ToAlpha, 0.f, 1.f);
		FadeModifier->Duration = FMath::Max(0.f, InDuration);
		FadeModifier->Elapsed = 0.f;
		FadeModifier->CurrentAlpha = FadeModifier->StartAlpha;

		ActiveModifiers.Add(FadeModifier);
		// ActiveModifiers.Sort([](UCameraModifierBase* A, UCameraModifierBase* B){ return *A < *B; });
	}

public:
	void Destroy() override;
	// Actor의 메인 틱 함수
	void Tick(float DeltaTime) override;
	void UpdateCamera(float DeltaTime);

	void SetMainCamera(UCameraComponent* InCamera)
	{
		CurrentViewTarget = InCamera;
	};
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


