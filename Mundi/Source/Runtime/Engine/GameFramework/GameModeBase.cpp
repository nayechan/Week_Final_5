// ────────────────────────────────────────────────────────────────────────────
// GameModeBase.cpp
// GameModeBase 클래스 구현
// ────────────────────────────────────────────────────────────────────────────
#include "pch.h"
#include "GameModeBase.h"
#include "GameStateBase.h"
#include "PlayerController.h"
#include "Pawn.h"
#include "World.h"


// ────────────────────────────────────────────────────────────────────────────
// 생성자
// ────────────────────────────────────────────────────────────────────────────

AGameModeBase::AGameModeBase()
	: PlayerSpawnLocation(FVector(0.0f, 0.0f, 0.0f))
	, GameState(nullptr)
	, DefaultPawnClass(nullptr)
	, PlayerControllerClass(nullptr)
	, PlayerController(nullptr)
	, bGameStarted(false)
	, bAutoSpawnPlayer(true)
{
	// 기본 클래스 설정
	PlayerControllerClass = APlayerController::StaticClass();
	DefaultPawnClass = APawn::StaticClass();
}

// ────────────────────────────────────────────────────────────────────────────
// 생명주기
// ────────────────────────────────────────────────────────────────────────────

void AGameModeBase::BeginPlay()
{
	Super::BeginPlay();

	// TODO: World 시스템과 통합 후 활성화. 2번째 게임잼때 통합하면 될듯?
	// World의 설정을 GameMode에 적용
	//if (World)
	//{
	//	// World 설정이 있으면 우선 사용
	//	if (World->GetDefaultPawnClass())
	//	{
	//		DefaultPawnClass = World->GetDefaultPawnClass();
	//	}
	//
	//	if (World->GetPlayerControllerClass())
	//	{
	//		PlayerControllerClass = World->GetPlayerControllerClass();
	//	}
	//
	//	PlayerSpawnLocation = World->GetPlayerSpawnLocation();
	//}

	// GameState 자동 생성 (없는 경우)
	if (!GameState && World)
	{
		AGameStateBase* NewGameState = World->SpawnActor<AGameStateBase>();
		if (NewGameState)
		{
			GameState = NewGameState;
		}
	}

	// 플레이어 초기화
	if (bAutoSpawnPlayer)
	{
		InitPlayer();
	}
}

void AGameModeBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// 추가 게임 로직 필요 시 여기에 구현
}

// ────────────────────────────────────────────────────────────────────────────
// 게임 라이프사이클
// ────────────────────────────────────────────────────────────────────────────

void AGameModeBase::StartGame()
{
	if (bGameStarted)
	{
		return;
	}

	bGameStarted = true;

	// GameState 상태 변경
	if (GameState)
	{
		GameState->SetGameState(EGameState::Playing);
	}

	// 델리게이트 브로드캐스트
	OnGameStarted.Broadcast();
}

void AGameModeBase::EndGame(bool bVictory)
{
	if (!bGameStarted)
	{
		return;
	}

	bGameStarted = false;

	// GameState 상태 변경
	if (GameState)
	{
		if (bVictory)
		{
			GameState->SetGameState(EGameState::Victory);
		}
		else
		{
			GameState->SetGameState(EGameState::GameOver);
		}
	}

	// 델리게이트 브로드캐스트
	OnGameEnded.Broadcast(bVictory);
}

void AGameModeBase::RestartGame()
{
	// GameState 초기화
	if (GameState)
	{
		GameState->SetGameState(EGameState::NotStarted);
		GameState->SetScore(0);
		GameState->ResetTimer();
	}

	bGameStarted = false;

	// 델리게이트 브로드캐스트
	OnGameRestarted.Broadcast();

	// 자동으로 게임 시작
	StartGame();
}

void AGameModeBase::PauseGame()
{
	if (!bGameStarted)
	{
		return;
	}

	// GameState 상태 변경
	if (GameState)
	{
		GameState->SetGameState(EGameState::Paused);
	}

	// 델리게이트 브로드캐스트
	OnGamePaused.Broadcast();
}

void AGameModeBase::ResumeGame()
{
	if (!bGameStarted)
	{
		return;
	}

	// GameState 상태 변경
	if (GameState)
	{
		GameState->SetGameState(EGameState::Playing);
	}

	// 델리게이트 브로드캐스트
	OnGameResumed.Broadcast();
}

// ────────────────────────────────────────────────────────────────────────────
// GameState 관리
// ────────────────────────────────────────────────────────────────────────────

void AGameModeBase::SetGameState(AGameStateBase* NewGameState)
{
	if (NewGameState)
	{
		GameState = NewGameState;
	}
}

// ────────────────────────────────────────────────────────────────────────────
// 플레이어 관리
// ────────────────────────────────────────────────────────────────────────────

void AGameModeBase::InitPlayer()
{
	// 1. PlayerController 생성
	PlayerController = SpawnPlayerController();

	if (!PlayerController)
	{
		return;
	}

	// 2. Pawn 스폰 위치 결정
	FTransform SpawnTransform;
	SpawnTransform.Translation = PlayerSpawnLocation;
	SpawnTransform.Rotation = FQuat::Identity();
	SpawnTransform.Scale3D = FVector(1.0f, 1.0f, 1.0f);

	// 3. Pawn 스폰 및 빙의
	APawn* SpawnedPawn = SpawnDefaultPawnFor(PlayerController, SpawnTransform);
}

APlayerController* AGameModeBase::SpawnPlayerController()
{
	if (!World || !PlayerControllerClass)
	{
		return nullptr;
	}

	// PlayerController 스폰 (위치는 중요하지 않음)
	AActor* NewActor = World->SpawnActor(PlayerControllerClass);
	APlayerController* NewController = Cast<APlayerController>(NewActor);

	return NewController;
}

APawn* AGameModeBase::SpawnDefaultPawnFor(APlayerController* NewPlayer, const FTransform& SpawnTransform)
{
	if (!NewPlayer || !World)
	{
		return nullptr;
	}

	// Pawn 클래스 결정
	UClass* PawnClass = GetDefaultPawnClassForController(NewPlayer);

	if (!PawnClass)
	{
		return nullptr;
	}

	// Pawn 스폰
	AActor* NewActor = World->SpawnActor(PawnClass, SpawnTransform);
	APawn* NewPawn = Cast<APawn>(NewActor);

	if (NewPawn)
	{
		// PlayerController가 Pawn을 빙의
		NewPlayer->Possess(NewPawn);
	}

	return NewPawn;
}

UClass* AGameModeBase::GetDefaultPawnClassForController(APlayerController* InController)
{
	return DefaultPawnClass;
}

void AGameModeBase::RestartPlayer(APlayerController* Player)
{
	if (!Player || !World)
	{
		return;
	}

	// 기존 Pawn 제거
	APawn* OldPawn = Player->GetPawn();
	if (OldPawn)
	{
		Player->UnPossess();
		OldPawn->Destroy();
	}

	// 새 Pawn 스폰
	FTransform SpawnTransform;
	SpawnTransform.Translation = PlayerSpawnLocation;
	SpawnTransform.Rotation = FQuat::Identity();
	SpawnTransform.Scale3D = FVector(1.0f, 1.0f, 1.0f);

	APawn* NewPawn = SpawnDefaultPawnFor(Player, SpawnTransform);
}

// ────────────────────────────────────────────────────────────────────────────
// 복제 및 직렬화
// ────────────────────────────────────────────────────────────────────────────

void AGameModeBase::DuplicateSubObjects()
{
	Super::DuplicateSubObjects();

	// GameMode 복제 시 게임 상태 초기화
	bGameStarted = false;
}

void AGameModeBase::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	Super::Serialize(bInIsLoading, InOutHandle);

	// TODO: 필요한 멤버 변수 직렬화 추가
}
