// ────────────────────────────────────────────────────────────────────────────
// PlayerController.cpp
// 플레이어 입력 처리 Controller 구현
// ────────────────────────────────────────────────────────────────────────────
#include "pch.h"
#include "PlayerController.h"
#include "Pawn.h"
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
	, MouseSensitivity(1.0f)
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

	// 입력 처리
	if (bInputEnabled)
	{
		ProcessPlayerInput();
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

		// PlayerCameraManager 생성 (OnPossess가 BeginPlay보다 먼저 호출될 수 있음)
		if (!PlayerCameraManager && World && World->bPie)
		{
			PlayerCameraManager = World->SpawnActor<APlayerCameraManager>();
		}

		// TODO: Pawn의 CameraComponent를 찾아서 PlayerCameraManager에 설정
		// Week11에서는 SetViewCamera(UCameraComponent*)를 사용
		// 2단계에서 Pawn 구현 후 활성화
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

	// Pawn의 InputComponent에 입력 전달
	UInputComponent* InputComp = PossessedPawn->GetInputComponent();
	if (InputComp)
	{
		InputComp->ProcessInput();
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
