// ────────────────────────────────────────────────────────────────────────────
// GameStateBase.h
// 게임 전역 상태를 관리하는 클래스
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include "Info.h"
#include "AGameStateBase.generated.h"

// 게임 상태 열거형
enum class EGameState : uint8
{
	NotStarted,  // 게임 시작 전
	Playing,     // 게임 진행 중
	Paused,      // 일시 정지
	GameOver,    // 게임 오버
	Victory      // 승리
};

// 게임 상태 변경 델리게이트 타입 (OldState, NewState)
DECLARE_DELEGATE_TYPE_TwoParam(FOnGameStateChanged, EGameState, EGameState);

// 스코어 변경 델리게이트 타입 (OldScore, NewScore)
DECLARE_DELEGATE_TYPE_TwoParam(FOnScoreChanged, int32, int32);

// 타이머 업데이트 델리게이트 타입 (ElapsedTime)
DECLARE_DELEGATE_TYPE_OneParam(FOnTimerUpdated, float);

/**
 * AGameStateBase
 *
 * 게임 전역 상태를 관리하는 클래스입니다.
 * AInfo를 상속받아 물리적 실체 없이 데이터만 관리합니다.
 *
 * 주요 기능:
 * - 게임 상태 관리 (Playing, Paused, GameOver, Victory 등)
 * - 스코어 관리
 * - 타이머 관리
 */
UCLASS(DisplayName="게임 스테이트", Description="게임 전역 상태를 관리하는 클래스입니다.")
class AGameStateBase : public AInfo
{
public:
	GENERATED_REFLECTION_BODY()

	AGameStateBase();

protected:
	~AGameStateBase() override = default;

public:
	// ────────────────────────────────────────────────
	// 생명주기
	// ────────────────────────────────────────────────

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	// ────────────────────────────────────────────────
	// 게임 상태 관리
	// ────────────────────────────────────────────────

	EGameState GetGameState() const { return CurrentGameState; }
	void SetGameState(EGameState NewState);

	// ────────────────────────────────────────────────
	// 스코어 관리
	// ────────────────────────────────────────────────

	UPROPERTY(EditAnywhere, Category="GameState", Range="0, 999999", Tooltip="현재 게임 스코어입니다.")
	int32 Score;

	int32 GetScore() const { return Score; }
	void SetScore(int32 NewScore);
	void AddScore(int32 Points);

	// ────────────────────────────────────────────────
	// 타이머 관리
	// ────────────────────────────────────────────────

	UPROPERTY(EditAnywhere, Category="GameState", Tooltip="게임 경과 시간(초)입니다.")
	float ElapsedTime;

	float GetElapsedTime() const { return ElapsedTime; }
	void ResetTimer();
	void PauseTimer();
	void ResumeTimer();

	// ────────────────────────────────────────────────
	// 델리게이트
	// ────────────────────────────────────────────────

	FOnGameStateChanged OnGameStateChanged;
	FOnScoreChanged OnScoreChanged;
	FOnTimerUpdated OnTimerUpdated;

	// ────────────────────────────────────────────────
	// 복제 및 직렬화
	// ────────────────────────────────────────────────

	void DuplicateSubObjects() override;
	void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;

protected:
	// ────────────────────────────────────────────────
	// 멤버 변수
	// ────────────────────────────────────────────────

	// 현재 게임 상태
	EGameState CurrentGameState;

	// 타이머 일시정지 플래그
	bool bTimerPaused;
};
