// ────────────────────────────────────────────────────────────────────────────
// PlayerController.cpp
// 플레이어 입력 처리 Controller 구현
// ────────────────────────────────────────────────────────────────────────────
#include "pch.h"
#include "PlayerController.h"
#include "Pawn.h"
#include "Character.h"
#include "InputComponent.h"
#include "InputManager.h"
#include "PlayerCameraManager.h"
#include "CameraComponent.h"
#include "World.h"

APlayerController::APlayerController()
	: InputManager(nullptr)
	, bInputEnabled(true)
	, bShowMouseCursor(true)
	, bIsMouseLocked(false)
	, MouseSensitivity(0.1f)
	, bRequireMouseButtonForRotation(true)
	, CurrentInputMode(EInputMode::GameAndUI)
	, PlayerCameraManager(nullptr)
{
}

APlayerController::~APlayerController()
{
	// PlayerCameraManager는 World가 관리하므로 여기서 직접 삭제하지 않음
}

// ────────────────────────────────────────────────────────────────────────────
// 생명주기
// ────────────────────────────────────────────────────────────────────────────

void APlayerController::BeginPlay()
{
	Super::BeginPlay();

	// InputManager 참조 획득
	InputManager = &UInputManager::GetInstance();

	if (PlayerCameraManager)
	{
		PlayerCameraManager->BeginPlay();
	}
}

void APlayerController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Shift + F1: InputMode 토글 (GameOnly <-> GameAndUI)
	if (InputManager && InputManager->IsKeyPressed(VK_F1) && InputManager->IsKeyDown(VK_SHIFT))
	{
		if (CurrentInputMode == EInputMode::GameOnly)
		{
			SetInputMode(EInputMode::GameAndUI);
		}
		else
		{
			SetInputMode(EInputMode::GameOnly);
		}
	}

	// 입력 처리
	if (bInputEnabled)
	{
		ProcessPlayerInput();
		ProcessMouseInput();
	}
	else
	{
		// 입력 비활성화 상태에서는 마우스 델타를 초기화하여 카메라 움직임 방지
		if (InputManager)
		{
			InputManager->GetMouseDelta(); // 델타 소비
		}
	}

	// 카메라 업데이트는 PlayerCameraManager의 Tick에서 자동으로 처리됨
}

void APlayerController::Possess(APawn* InPawn)
{
	Super::Possess(InPawn);
}

void APlayerController::UnPossess()
{
	Super::UnPossess();
}

void APlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	if (InPawn)
	{
		// Pawn의 InputComponent에 입력 바인딩 설정
		UInputComponent* InputComp = InPawn->GetInputComponent();
		if (InputComp)
		{
			InPawn->SetupPlayerInputComponent(InputComp);
		}

		// PlayerCameraManager 생성 (World에 없을 때만)
		if (!PlayerCameraManager && World && World->bPie)
		{
			// World에 이미 PlayerCameraManager가 있으면 그것을 사용
			APlayerCameraManager* ExistingPCM = World->GetPlayerCameraManager();
			if (ExistingPCM)
			{
				PlayerCameraManager = ExistingPCM;
			}
			else
			{
				PlayerCameraManager = World->SpawnActor<APlayerCameraManager>();
				World->SetPlayerCameraManager(PlayerCameraManager);
			}
		}
	}
}

void APlayerController::OnUnPossess()
{
	Super::OnUnPossess();
}

// ────────────────────────────────────────────────────────────────────────────
// 입력 처리
// ────────────────────────────────────────────────────────────────────────────

void APlayerController::ProcessPlayerInput()
{
	if (!PossessedPawn)
	{
		return;
	}

	// UIOnly 모드에서는 게임 입력 차단
	if (InputManager && !InputManager->CanReceiveGameInput())
	{
		return;
	}

	// Pawn의 InputComponent에 입력 전달
	UInputComponent* InputComp = PossessedPawn->GetInputComponent();
	if (InputComp)
	{
		InputComp->ProcessInput();
	}
}

void APlayerController::ProcessMouseInput()
{
	if (!InputManager)
	{
		return;
	}

	// UIOnly 모드에서는 게임 입력(카메라 회전) 차단
	if (!InputManager->CanReceiveGameInput())
	{
		return;
	}

	// 마우스 버튼 필요 여부에 따라 카메라 회전 조건 결정
	bool bShouldRotate = !bRequireMouseButtonForRotation ||
	                     InputManager->IsMouseButtonDown(EMouseButton::RightButton);

	if (bShouldRotate)
	{
		FVector2D MouseDelta = InputManager->GetMouseDelta();

		// Yaw (좌우 회전) - X축 마우스 이동
		if (MouseDelta.X != 0.0f)
		{
			AddYawInput(MouseDelta.X * MouseSensitivity);
		}

		// Pitch (상하 회전) - Y축 마우스 이동
		if (MouseDelta.Y != 0.0f)
		{
			AddPitchInput(MouseDelta.Y * MouseSensitivity);
		}
	}
}

void APlayerController::ShowMouseCursor()
{
	bShowMouseCursor = true;

	if (InputManager)
	{
		InputManager->SetCursorVisible(true);
	}
}

void APlayerController::HideMouseCursor()
{
	bShowMouseCursor = false;

	if (InputManager)
	{
		InputManager->SetCursorVisible(false);
	}
}

void APlayerController::LockMouseCursor()
{
	bIsMouseLocked = true;

	if (InputManager)
	{
		InputManager->LockCursor();
	}
}

void APlayerController::UnlockMouseCursor()
{
	bIsMouseLocked = false;

	if (InputManager)
	{
		InputManager->ReleaseCursor();
	}
}

UCameraComponent* APlayerController::GetCameraComponentForRendering() const
{
	if (PlayerCameraManager)
	{
		return PlayerCameraManager->GetViewCamera();
	}
	return nullptr;
}

// ────────────────────────────────────────────────────────────────────────────
// Input Mode
// ────────────────────────────────────────────────────────────────────────────

void APlayerController::SetInputMode(EInputMode NewMode)
{
	CurrentInputMode = NewMode;

	// InputManager에도 InputMode 동기화
	if (InputManager)
	{
		InputManager->SetInputMode(NewMode);
	}

	switch (NewMode)
	{
	case EInputMode::GameOnly:
		// 게임 입력만: 커서 숨김, 마우스 잠금, 마우스 버튼 없이 카메라 회전
		if (InputManager)
		{
			InputManager->SetCursorToCenter();
		}
		HideMouseCursor();
		LockMouseCursor();
		SetRequireMouseButtonForRotation(false);
		break;

	case EInputMode::UIOnly:
		// UI 입력만: 커서 표시, 마우스 잠금 해제
		ShowMouseCursor();
		UnlockMouseCursor();
		SetRequireMouseButtonForRotation(true);
		break;

	case EInputMode::GameAndUI:
		// 게임 + UI: 커서 표시, 마우스 잠금 해제, 우클릭으로 카메라 회전
		ShowMouseCursor();
		UnlockMouseCursor();
		SetRequireMouseButtonForRotation(true);
		break;
	}
}

// ────────────────────────────────────────────────────────────────────────────
// 복제
// ────────────────────────────────────────────────────────────────────────────

void APlayerController::DuplicateSubObjects()
{
	Super::DuplicateSubObjects();
}

void APlayerController::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	Super::Serialize(bInIsLoading, InOutHandle);

	// TODO: 필요한 멤버 변수 직렬화 추가
}
