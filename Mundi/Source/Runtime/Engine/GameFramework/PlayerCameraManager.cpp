#include "pch.h"
#include "PlayerCameraManager.h"
#include "Camera/CameraModifierBase.h"
#include "Camera/CamMod_Fade.h"
#include "Camera/CamMod_Shake.h"
#include "SceneView.h"
#include "CameraActor.h"
#include "World.h"
#include "FViewport.h"
#include "RenderSettings.h"

IMPLEMENT_CLASS(APlayerCameraManager)

BEGIN_PROPERTIES(APlayerCameraManager)
MARK_AS_SPAWNABLE("플레이어 카메라 매니저", "최종적으로 카메라 화면을 관리하는 액터입니다. (씬에 1개만 존재 필요)")
END_PROPERTIES()

APlayerCameraManager::APlayerCameraManager()
{
	Name = "Player Camera Manager";
	
	SceneView = new FSceneView();
}

APlayerCameraManager::~APlayerCameraManager()
{
	if (SceneView != nullptr)
	{
		delete SceneView;
		SceneView = nullptr;
	}
	
	if(BlendStartView)
	{
		delete BlendStartView;
		BlendStartView = nullptr;
	}
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
	FadeModifier->Priority   = InPriority;
	FadeModifier->bEnabled   = true;

	FadeModifier->FadeColor      = InColor;
	FadeModifier->StartAlpha = FMath::Clamp(FromAlpha, 0.f, 1.f);
	FadeModifier->EndAlpha   = FMath::Clamp(ToAlpha,   0.f, 1.f);
	FadeModifier->Duration   = FMath::Max(0.f, InDuration);
	FadeModifier->Elapsed    = 0.f;
	FadeModifier->CurrentAlpha = FadeModifier->StartAlpha;

	ActiveModifiers.Add(FadeModifier);
	// ActiveModifiers.Sort([](UCameraModifierBase* A, UCameraModifierBase* B){ return *A < *B; });
}

void APlayerCameraManager::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	

	// 매 틱 모든 카메라 로직을 계산하여 CachedViewInfo를 업데이트합니다.
	LastDeltaSeconds = DeltaTime;
	BuildForFrame(DeltaTime);
}


void APlayerCameraManager::BuildForFrame(float DeltaTime)
{
	// 모든 Modifier tick Update
	for (UCameraModifierBase* M : ActiveModifiers)
	{
		if (!M || !M->bEnabled || M->Weight <= 0.f) continue;
		M->ApplyToView(DeltaTime, *SceneView);
	}

	// 2) 후처리: PP 모디파이어 초기화 + 수집
	SceneView->PostProcessInput.Modifiers.clear();
	for (UCameraModifierBase* M : ActiveModifiers)
	{
		if (!M || !M->bEnabled || M->Weight <= 0.f) continue;
		M->CollectPostProcess(SceneView->PostProcessInput.Modifiers, *SceneView);
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

	if (PendingViewTarget)
	{
		float V = 1.0f - (BlendTimeRemaining / BlendTimeTotal);
		FVector StartLocation = BlendStartView->ViewLocation;
		FQuat StartRotation = BlendStartView->ViewRotation;

		FVector TargetLocation = PendingViewTarget->GetWorldLocation();
		FQuat TargetRotation = PendingViewTarget->GetWorldRotation();

		FVector CurLocation = FVector::Lerp(StartLocation, TargetLocation, V);

		BlendTimeRemaining -= DeltaTime;
		if (BlendTimeRemaining <= 0)
		{
			PendingViewTarget = nullptr;
		}
	}
	else
	{
		SceneView->Camera = CurrentViewTarget;
		SceneView->ViewMatrix = CurrentViewTarget->GetViewMatrix();
		SceneView->ViewLocation = CurrentViewTarget->GetWorldLocation();
		SceneView->ViewRotation = CurrentViewTarget->GetWorldRotation();
		SceneView->ZNear = CurrentViewTarget->GetNearClip();
		SceneView->ZFar = CurrentViewTarget->GetFarClip();
		SceneView->ProjectionMode = CurrentViewTarget->GetProjectionMode();
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

FSceneView* APlayerCameraManager::GetSceneView(FViewport* InViewport, URenderSettings* InRenderSettings)
{
	if (!InViewport || !InRenderSettings)
	{
		return nullptr;
	}

	UCameraComponent* ViewTarget = CurrentViewTarget;
	if (!ViewTarget)
	{
		ViewTarget = GetMainCamera();
	}

	if (!ViewTarget)
	{
		return nullptr;
	}

	if (SceneView->Camera)
	{
		SceneView->InitRenderSetting(InViewport, InRenderSettings);
		return SceneView; 
	}

	return nullptr;
}

void APlayerCameraManager::SetViewTarget(UCameraComponent* NewViewTarget)
{
	CurrentViewTarget = NewViewTarget;
	PendingViewTarget = nullptr; // 블렌딩 취소
	BlendTimeRemaining = 0.0f;
}

void APlayerCameraManager::SetViewTargetWithBlend(UCameraComponent* NewViewTarget, float InBlendTime)
{
	if(BlendStartView)
	{
		delete BlendStartView;
		BlendStartView = nullptr;
	}

	// "현재 뷰 타겟"의 뷰 정보를 스냅샷으로 저장해야 합니다.
	*BlendStartView = *SceneView;

	// 현재 뷰를 블렌딩 시작 뷰로 저장
	PendingViewTarget = NewViewTarget;
	BlendTimeTotal = InBlendTime;
	BlendTimeRemaining = InBlendTime;
}


//
// void APlayerCameraManager::UpdateCamera(float DeltaTime)
// {
// 	FMinimalViewInfo ResultView;
//
// 	// --- 1. 뷰 타겟 블렌딩(Blending) 처리 ---
// 	if (BlendTimeRemaining > 0.0f)
// 	{
// 	    // 블렌딩 진행도 (Alpha) 계산
// 	    float BlendAlpha = 1.0f - (BlendTimeRemaining / BlendTimeTotal);
// 	    BlendAlpha = std.clamp(BlendAlpha, 0.0f, 1.0f);
//
// 	    // 새 타겟의 현재 뷰 정보 가져오기
// 	    FMinimalViewInfo NewViewInfo = GetBaseViewInfo(PendingViewTarget);
//
// 	    // 이전 뷰(BlendStartViewInfo)와 새 뷰(NewViewInfo)를 보간
// 	    ResultView = BlendViews(BlendStartViewInfo, NewViewInfo, BlendAlpha);
//
// 	    // 시간 업데이트
// 	    BlendTimeRemaining -= DeltaTime;
// 	    if (BlendTimeRemaining <= 0.0f)
// 	    {
// 	        // 블렌딩 종료
// 	        CurrentViewTarget = PendingViewTarget;
// 	        PendingViewTarget.Actor = nullptr;
// 	    }
// 	}
// 	else
// 	{
// 	    // --- 2. 기본 뷰(Base View) 가져오기 ---
// 	    // 블렌딩 중이 아니면 현재 뷰 타겟의 정보만 사용
// 	    ResultView = GetBaseViewInfo(CurrentViewTarget);
// 	}
//
// 	// --- 3. 카메라 모디파이어(Shakes) 적용 ---
// 	/ (블렌딩 결과물 위에 셰이크 효과 등을 덧입힘)
// 	UpdateModifiers(DeltaTime, ResultView);
//
// 	// --- 4. 전역 효과(Fade) 처리 ---
// 	if (FadeTimeRemaining > 0.0f)
// 	{
// 	    float FadeAlpha = 1.0f - (FadeTimeRemaining / FadeTimeTotal);
// 	    // (FadeStartAmount 같은 변수가 필요하지만, 예시를 위해 단순화)
// 	    FadeAmount = FMath::Lerp(0.0f, FadeTargetAmount, FadeAlpha);
//
// 	    FadeTimeRemaining -= DeltaTime;
// 	}
//
// 	// --- 5. 레터박스(Letterbox) 처리 ---
// 	/ (만약 시네마틱 모드라면 여기서 ResultView.ViewRect와 ResultView.AspectRatio를 수정)
// 	/ 예: if (bIsCinematicMode) { ... }
//
// 	// --- 6. 최종 결과를 캐시 변수에 저장 ---
// 	/ (Fade 값 등도 FMinimalViewInfo에 복사)
// 	ResultView.FadeAmount = this->FadeAmount;
// 	ResultView.FadeColor = this->FadeColor;
//
// 	this->CachedViewInfo = ResultView;
// }
//
// FMinimalViewInfo APlayerCameraManager::GetFinalViewInfo() const
// {
// 	return FMinimalViewInfo();
// }

//void APlayerCameraManager::DuplicateSubObjects()
//{
//	Super::DuplicateSubObjects();
//}
//
//void APlayerCameraManager::Serialize(const bool bInIsLoading, JSON& InOutHandle)
//{
//	Super::DuplicateSubObjects();
//}
