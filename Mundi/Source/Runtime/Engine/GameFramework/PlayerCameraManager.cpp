#include "pch.h"
#include "PlayerCameraManager.h"
#include "Camera/CameraModifierBase.h"
#include "Camera/CamMod_Fade.h"
#include "Camera/CamMod_Shake.h"
#include "Camera/CamMod_LetterBox.h"
#include "Camera/CamMod_Vignette.h"
#include "SceneView.h"
#include "CameraActor.h"
#include "World.h"
#include "FViewport.h"
#include "RenderSettings.h"

IMPLEMENT_CLASS(APlayerCameraManager)

BEGIN_PROPERTIES(APlayerCameraManager)
MARK_AS_SPAWNABLE("플레이어 카메라 매니저", "최종적으로 카메라 화면을 관리하는 액터입니다. (씬에 1개만 존재 필요)")
ADD_PROPERTY_CURVE(MyCurve, "Light", true, "원뿔 내부 각도입니다. 이 각도 안에서는 빛이 최대 밝기로 표시됩니다.")
END_PROPERTIES()


APlayerCameraManager::~APlayerCameraManager()
{
	CurrentViewTarget = nullptr;
	PendingViewTarget = nullptr;

	CachedViewport = nullptr;
}

void APlayerCameraManager::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	
	// 매 틱 모든 카메라 로직을 계산하여 SceneView를 업데이트합니다.
	BuildForFrame(DeltaTime);
}

void APlayerCameraManager::BuildForFrame(float DeltaTime)
{
	// 뷰 타겟 결정 (트렌지션 또는 현재 타겟)
	UpdateViewTarget(DeltaTime);

	// 모든 Modifier tick Update
	for (UCameraModifierBase* M : ActiveModifiers)
	{
		if (!M || !M->bEnabled || M->Weight <= 0.f) continue;
		M->ApplyToView(DeltaTime, &SceneView);
	}

	// 2) 후처리: PP 모디파이어 초기화 + 수집
	Modifiers.clear();
	for (UCameraModifierBase* M : ActiveModifiers)
	{
		if (!M || !M->bEnabled || M->Weight <= 0.f) continue;
		M->CollectPostProcess(Modifiers);
	}

	// 3) 수명 정리
	for (int32 i= ActiveModifiers.Num()-1; i>=0; i--)
	{
		UCameraModifierBase* M = ActiveModifiers[i];
		if (!M) { ActiveModifiers.RemoveAtSwap(i); continue; }
		
		ActiveModifiers[i]->TickLifetime(DeltaTime);

		if (M->Duration >= 0.f && !M->bEnabled)
		{ ActiveModifiers.RemoveAtSwap(i); continue; }
	}

	if (CachedViewport)
	{
		// 최종 종횡비 (AspectRatio)
		if (SceneView.ViewRect.Height() > 0)
		{
			SceneView.AspectRatio = SceneView.ViewRect.Width() / SceneView.ViewRect.Height();
		}
		else
		{
			SceneView.AspectRatio = 1.7777f; // 16:9 폴백
		}
	}
	else
	{
		// 폴백 (뷰포트가 아직 캐시 안됨)
		SceneView.AspectRatio = 1.7777f;
		SceneView.ViewRect = { 0, 0, 1920, 1080 };
	}
}

void APlayerCameraManager::Destroy()
{
	// 교체할 수 있으면 해당 매니저로 교체 후 삭제 허용
	TArray<APlayerCameraManager*> PlayerCameraManagers = GetWorld()->FindActors<APlayerCameraManager>();
	if (1 < PlayerCameraManagers.Num())
	{
		for (APlayerCameraManager* PlayerCameraManager : PlayerCameraManagers)
		{
			if (this != PlayerCameraManager)
			{
				GetWorld()->SetFirstPlayerCameraManager(PlayerCameraManager);
				Super::Destroy();
				return;
			}
		}
	}

	UE_LOG("[warning] PlayerCameraManager는 삭제할 수 없습니다. (새로운 매니저를 만들고 삭제하면 가능)");
}

UCameraComponent* APlayerCameraManager::GetMainCamera()
{
	if (CurrentViewTarget)
	{
		return CurrentViewTarget;
	}

	// 추후 컴포넌트로 교체
	ACameraActor* CameraActor = GetWorld()->FindActor<ACameraActor>();

	if (CameraActor)
	{
		// Todo: 추후 world의 CameraActor 변경
		GetWorld()->SetCameraActor(CameraActor);

		CurrentViewTarget = CameraActor->GetCameraComponent();
	}

	return CurrentViewTarget;
}

FMinimalViewInfo* APlayerCameraManager::GetSceneView()
{
	UCameraComponent* ViewTarget = CurrentViewTarget;
	if (!ViewTarget)
	{
		ViewTarget = GetMainCamera();
	}

	if (!ViewTarget)
	{
		return nullptr;
	}

	return &SceneView; 
}

void APlayerCameraManager::SetViewTarget(UCameraComponent* NewViewTarget)
{
	CurrentViewTarget = NewViewTarget;
	PendingViewTarget = nullptr; // 블렌딩 취소
	BlendTimeRemaining = 0.0f;
}

void APlayerCameraManager::SetViewTargetWithBlend(UCameraComponent* NewViewTarget, float InBlendTime)
{
	// "현재 뷰 타겟"의 뷰 정보를 스냅샷으로 저장해야 합니다.
	BlendStartView = SceneView;

	// 현재 뷰를 블렌딩 시작 뷰로 저장
	PendingViewTarget = NewViewTarget;
	BlendTimeTotal = InBlendTime;
	BlendTimeRemaining = InBlendTime;
}

void APlayerCameraManager::StartCameraShake(float InDuration, float AmpLoc, float AmpRotDeg, float Frequency,
	int32 InPriority)
{
	UCamMod_Shake* ShakeModifier = new UCamMod_Shake();
	ShakeModifier->Priority = InPriority;
	ShakeModifier->Initialize(InDuration, AmpLoc, AmpRotDeg, Frequency);
	ActiveModifiers.Add(ShakeModifier);
}

void APlayerCameraManager::StartFade(float InDuration, float FromAlpha, float ToAlpha, const FLinearColor& InColor,
	int32 InPriority)
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

void APlayerCameraManager::StartLetterBox(float InDuration, float Aspect, float BarHeight, const FLinearColor& InColor, int32 InPriority)
{
	UCamMod_LetterBox* LetterBoxModifier = new UCamMod_LetterBox();
	LetterBoxModifier->Duration = InDuration;
	LetterBoxModifier->Priority = InPriority;
	LetterBoxModifier->AspectRatio = Aspect;
	LetterBoxModifier->HeightBarSize = BarHeight;
	LetterBoxModifier->BoxColor = InColor;

	ActiveModifiers.Add(LetterBoxModifier);
}

int APlayerCameraManager::StartVignette(float InDuration, float Radius, float Softness, float Intensity, float Roundness, const FLinearColor& InColor, int32 InPriority)
{
	UCamMod_Vignette* VignetteModifier = new UCamMod_Vignette();
	VignetteModifier->Duration = InDuration;
	VignetteModifier->Priority = InPriority;
	VignetteModifier->Radius = Radius;
	VignetteModifier->Softness = Softness;
	VignetteModifier->Intensity = Intensity;
	VignetteModifier->Roundness = Roundness;
	VignetteModifier->Color = InColor;

	ActiveModifiers.Add(VignetteModifier);
	return (ActiveModifiers.Num() - 1);
}

int APlayerCameraManager::UpdateVignette(int Idx, float InDuration, float Radius, float Softness, float Intensity, float Roundness, const FLinearColor& InColor, int32 InPriority)
{
	if (ActiveModifiers.Num() <= Idx || !ActiveModifiers[Idx] || !Cast<UCamMod_Vignette>(ActiveModifiers[Idx]))
	{
		return -1;
	}

	UCamMod_Vignette* VignetteModifier = Cast<UCamMod_Vignette>(ActiveModifiers[Idx]);
	VignetteModifier->Duration = InDuration;
	VignetteModifier->Priority = InPriority;
	VignetteModifier->Radius = Radius;
	VignetteModifier->Softness = Softness;
	VignetteModifier->Intensity = Intensity;
	VignetteModifier->Roundness = Roundness;
	VignetteModifier->Color = InColor;

	ActiveModifiers[Idx] = VignetteModifier;

	return Idx;
}

void APlayerCameraManager::AdjustVignette(float InDuration, float Radius, float Softness, float Intensity, float Roundness, const FLinearColor& InColor, int32 InPriority)
{
	if (LastVignetteIdx == -1)
	{
		LastVignetteIdx = StartVignette(InDuration, Radius, Softness, Intensity, Roundness, InColor, InPriority);
	}
	else
	{
		LastVignetteIdx = UpdateVignette(LastVignetteIdx, InDuration, Radius, Softness, Intensity, Roundness, InColor, InPriority);
	}
}

void APlayerCameraManager::DeleteVignette()
{
	if (ActiveModifiers.Num() <= LastVignetteIdx || !ActiveModifiers[LastVignetteIdx] || !Cast<UCamMod_Vignette>(ActiveModifiers[LastVignetteIdx]))
		return;
	ActiveModifiers[LastVignetteIdx]->bEnabled = false;
}

void APlayerCameraManager::UpdateViewTarget(float DeltaTime)
{
	UCameraComponent* TargetCam = CurrentViewTarget ? CurrentViewTarget : GetMainCamera();

	if (CachedViewport)
	{
		// 최종 뷰 영역 (ViewRect)
		SceneView.ViewRect.MinX = CachedViewport->GetStartX();
		SceneView.ViewRect.MinY = CachedViewport->GetStartY();
		SceneView.ViewRect.MaxX = SceneView.ViewRect.MinX + CachedViewport->GetSizeX();
		SceneView.ViewRect.MaxY = SceneView.ViewRect.MinY + CachedViewport->GetSizeY();
	}

	if (PendingViewTarget) // 1. 블렌딩 중일 때
	{
		float V = 1.0f;
		if (BlendTimeTotal > KINDA_SMALL_NUMBER)
		{
			V = 1.0f - (BlendTimeRemaining / BlendTimeTotal);
		}
		V = FMath::Clamp(V, 0.0f, 1.0f);

		// --- 시작 값 ---
		FVector StartLocation = BlendStartView.ViewLocation;
		FQuat StartRotation = BlendStartView.ViewRotation;
		float StartFOV = BlendStartView.FieldOfView;

		// --- 목표 값 ---
		FVector TargetLocation = PendingViewTarget->GetWorldLocation();
		FQuat TargetRotation = PendingViewTarget->GetWorldRotation();
		float TargetFOV = PendingViewTarget->GetFOV();

		// --- SceneView 보간 ---
		SceneView.ViewLocation = FVector::Lerp(StartLocation, TargetLocation, V);
		SceneView.ViewRotation = FQuat::Slerp(StartRotation, TargetRotation, V);
		SceneView.FieldOfView = FMath::Lerp(StartFOV, TargetFOV, V); // FOV 보간

		// Near/Far 등은 최종 타겟의 것을 즉시 따름
		SceneView.NearClip = PendingViewTarget->GetNearClip();
		SceneView.FarClip = PendingViewTarget->GetFarClip();
		SceneView.ZoomFactor = PendingViewTarget->GetZoomFactor();
		SceneView.ProjectionMode = PendingViewTarget->GetProjectionMode();

		// 남은 시간 계산
		BlendTimeRemaining -= DeltaTime;
		if (BlendTimeRemaining <= 0.0f)
		{
			CurrentViewTarget = PendingViewTarget;
			PendingViewTarget = nullptr;
			TargetCam = CurrentViewTarget; // TargetCam 변수 업데이트

			// 최종 값으로 고정
			SceneView.ViewLocation = TargetLocation;
			SceneView.ViewRotation = TargetRotation;
			SceneView.FieldOfView = TargetFOV; // FOV 고정
		}
	}
	else if (TargetCam) // 2. 블렌딩 중이 아닐 때
	{
		// SceneView의 모든 기본 값을 현재 타겟으로 설정
		SceneView.ViewLocation = TargetCam->GetWorldLocation();
		SceneView.ViewRotation = TargetCam->GetWorldRotation();
		SceneView.NearClip = TargetCam->GetNearClip();
		SceneView.FarClip = TargetCam->GetFarClip();
		SceneView.FieldOfView = TargetCam->GetFOV();
		SceneView.ZoomFactor = TargetCam->GetZoomFactor();
		SceneView.ProjectionMode = TargetCam->GetProjectionMode();
	}
}