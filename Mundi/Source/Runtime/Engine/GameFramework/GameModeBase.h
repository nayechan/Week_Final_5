// ────────────────────────────────────────────────────────────────────────────
// GameModeBase.h
// 게임 모드를 관리하는 클래스
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include "Info.h"
#include "AGameModeBase.generated.h"

// 전방 선언
class AGameStateBase;
class APawn;
class APlayerController;

// 게임 시작 델리게이트 타입
DECLARE_DELEGATE_TYPE(FOnGameStarted);

// 게임 종료 델리게이트 타입 (bVictory)
DECLARE_DELEGATE_TYPE_OneParam(FOnGameEnded, bool);

// 게임 재시작 델리게이트 타입
DECLARE_DELEGATE_TYPE(FOnGameRestarted);

// 게임 일시정지 델리게이트 타입
DECLARE_DELEGATE_TYPE(FOnGamePaused);

// 게임 재개 델리게이트 타입
DECLARE_DELEGATE_TYPE(FOnGameResumed);

/**
 * AGameModeBase
 *
 * 게임 모드를 관리하는 클래스입니다.
 * 게임 규칙, 플레이어 스폰, 게임 상태 전환 등을 담당합니다.
 *
 * 주요 기능:
 * - 게임 라이프사이클 관리 (StartGame, EndGame, RestartGame)
 * - 플레이어 스폰 및 관리
 * - GameState 관리
 */
UCLASS(DisplayName="게임 모드", Description="게임 규칙, 플레이어 스폰, 게임 상태 전환 등을 담당하는 클래스입니다.")
class AGameModeBase : public AInfo
{
public:
	GENERATED_REFLECTION_BODY()

	AGameModeBase();

protected:
	~AGameModeBase() override = default;

public:
	// ────────────────────────────────────────────────
	// 생명주기
	// ────────────────────────────────────────────────

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	// ────────────────────────────────────────────────
	// 게임 라이프사이클
	// ────────────────────────────────────────────────

	virtual void StartGame();
	virtual void EndGame(bool bVictory);
	virtual void RestartGame();
	virtual void PauseGame();
	virtual void ResumeGame();

	// ────────────────────────────────────────────────
	// 플레이어 관리
	// ────────────────────────────────────────────────

	UPROPERTY(EditAnywhere, Category="GameMode", Tooltip="플레이어 스폰 위치입니다.")
	FVector PlayerSpawnLocation;

	virtual FVector GetPlayerSpawnLocation() const { return PlayerSpawnLocation; }
	virtual void SetPlayerSpawnLocation(const FVector& Location) { PlayerSpawnLocation = Location; }

	/** PlayerController 생성 */
	virtual APlayerController* SpawnPlayerController();

	/** 플레이어를 위한 기본 Pawn 스폰 */
	virtual APawn* SpawnDefaultPawnFor(APlayerController* NewPlayer, const FTransform& SpawnTransform);

	/** 플레이어를 위한 기본 Pawn 클래스 반환 */
	virtual UClass* GetDefaultPawnClassForController(APlayerController* InController);

	/** 플레이어 리스폰 */
	virtual void RestartPlayer(APlayerController* Player);

	/** 메인 플레이어 컨트롤러 반환 */
	APlayerController* GetPlayerController() const { return PlayerController; }

	// ────────────────────────────────────────────────
	// GameState 관리
	// ────────────────────────────────────────────────

	AGameStateBase* GetGameState() const { return GameState; }
	void SetGameState(AGameStateBase* NewGameState);

	// ────────────────────────────────────────────────
	// 델리게이트
	// ────────────────────────────────────────────────

	FOnGameStarted OnGameStarted;
	FOnGameEnded OnGameEnded;
	FOnGameRestarted OnGameRestarted;
	FOnGamePaused OnGamePaused;
	FOnGameResumed OnGameResumed;

	// ────────────────────────────────────────────────
	// 복제 및 직렬화
	// ────────────────────────────────────────────────

	void DuplicateSubObjects() override;
	void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;

protected:
	// ────────────────────────────────────────────────
	// 멤버 변수
	// ────────────────────────────────────────────────

	// GameState 참조
	AGameStateBase* GameState;

	// DefaultPawn, PlayerController 클래스
	UClass* DefaultPawnClass;
	UClass* PlayerControllerClass;

	// 메인 플레이어 컨트롤러 인스턴스
	APlayerController* PlayerController;

	// 게임 시작 여부
	bool bGameStarted;

	// 자동으로 플레이어 스폰 여부
	bool bAutoSpawnPlayer;

	// ────────────────────────────────────────────────
	// 내부 함수
	// ────────────────────────────────────────────────

	/** 플레이어 초기화 */
	virtual void InitPlayer();
};
