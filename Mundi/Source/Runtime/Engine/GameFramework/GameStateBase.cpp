// ────────────────────────────────────────────────────────────────────────────
// GameStateBase.cpp
// GameStateBase 클래스 구현
// ────────────────────────────────────────────────────────────────────────────
#include "pch.h"
#include "GameStateBase.h"

// ────────────────────────────────────────────────────────────────────────────
// 생성자
// ────────────────────────────────────────────────────────────────────────────

AGameStateBase::AGameStateBase()
	: Score(0)
	, ElapsedTime(0.0f)
	, CurrentGameState(EGameState::NotStarted)
	, bTimerPaused(true)
{
	// GameState는 물리/렌더링 대상이 아님 (Info 상속)
}

// ────────────────────────────────────────────────────────────────────────────
// 생명주기
// ────────────────────────────────────────────────────────────────────────────

void AGameStateBase::BeginPlay()
{
	Super::BeginPlay();

	// 게임 시작 시 타이머 초기화
	ElapsedTime = 0.0f;
	bTimerPaused = true;
}

void AGameStateBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// 타이머가 일시정지 상태가 아니고 게임이 진행 중일 때만 시간 증가
	if (!bTimerPaused && (CurrentGameState == EGameState::Playing))
	{
		ElapsedTime += DeltaTime;
		OnTimerUpdated.Broadcast(ElapsedTime);
	}
}

// ────────────────────────────────────────────────────────────────────────────
// 게임 상태 관리
// ────────────────────────────────────────────────────────────────────────────

void AGameStateBase::SetGameState(EGameState NewState)
{
	if (CurrentGameState != NewState)
	{
		EGameState OldState = CurrentGameState;
		CurrentGameState = NewState;

		// 게임 상태에 따라 타이머 자동 제어
		if (NewState == EGameState::Playing)
		{
			ResumeTimer();
		}
		else if (NewState == EGameState::Paused || NewState == EGameState::GameOver || NewState == EGameState::Victory)
		{
			PauseTimer();
		}

		// 델리게이트 브로드캐스트
		OnGameStateChanged.Broadcast(OldState, NewState);
	}
}

// ────────────────────────────────────────────────────────────────────────────
// 스코어 관리
// ────────────────────────────────────────────────────────────────────────────

void AGameStateBase::SetScore(int32 NewScore)
{
	if (Score != NewScore)
	{
		int32 OldScore = Score;
		Score = NewScore;

		// 델리게이트 브로드캐스트
		OnScoreChanged.Broadcast(OldScore, NewScore);
	}
}

void AGameStateBase::AddScore(int32 Points)
{
	SetScore(Score + Points);
}

// ────────────────────────────────────────────────────────────────────────────
// 타이머 관리
// ────────────────────────────────────────────────────────────────────────────

void AGameStateBase::ResetTimer()
{
	ElapsedTime = 0.0f;
	bTimerPaused = true;
	OnTimerUpdated.Broadcast(ElapsedTime);
}

void AGameStateBase::PauseTimer()
{
	bTimerPaused = true;
}

void AGameStateBase::ResumeTimer()
{
	bTimerPaused = false;
}

// ────────────────────────────────────────────────────────────────────────────
// 복제 및 직렬화
// ────────────────────────────────────────────────────────────────────────────

void AGameStateBase::DuplicateSubObjects()
{
	Super::DuplicateSubObjects();

	// GameState는 복제 시 상태 초기화
	CurrentGameState = EGameState::NotStarted;
	Score = 0;
	ElapsedTime = 0.0f;
	bTimerPaused = true;
}

void AGameStateBase::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	Super::Serialize(bInIsLoading, InOutHandle);

	// TODO: 필요한 멤버 변수 직렬화 추가
}
