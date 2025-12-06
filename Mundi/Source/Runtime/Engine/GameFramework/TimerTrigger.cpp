#include "pch.h"
#include "TimerTrigger.h"
#include "LevelTransitionManager.h"
#include "World.h"

IMPLEMENT_CLASS(ATimerTrigger)

ATimerTrigger::ATimerTrigger()
{
    ObjectName = "TimerTrigger";
    bCanEverTick = true;
}

ATimerTrigger::~ATimerTrigger()
{
}

void ATimerTrigger::BeginPlay()
{
    Super::BeginPlay();

    RemainingTime = TriggerTime;
    ElapsedTime = 0.0f;
    bHasExpired = false;
    bIsRunning = false;
    LastLogTime = 0.0f;

    if (bAutoStart)
    {
        StartTimer();
    }

    UE_LOG("[info] TimerTrigger: BeginPlay - %s (TriggerTime: %.1fs, AutoStart: %d)",
        ObjectName.ToString(), TriggerTime, bAutoStart);
}

void ATimerTrigger::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    if (!bIsRunning || bHasExpired)
        return;

    // 시간 업데이트
    ElapsedTime += DeltaSeconds;
    RemainingTime = TriggerTime - ElapsedTime;

    // Lua OnTimerTick 호출 (매 프레임)
    CallLuaOnTimerTick();

    // 남은 시간 로그 (5초마다)
    if (bShowRemainingTime && ElapsedTime - LastLogTime >= 5.0f)
    {
        UE_LOG("[info] TimerTrigger: %s - Remaining time: %.1fs", ObjectName.ToString(), RemainingTime);
        LastLogTime = ElapsedTime;
    }

    // 시간 만료
    if (RemainingTime <= 0.0f)
    {
        bHasExpired = true;
        bIsRunning = false;
        RemainingTime = 0.0f;

        UE_LOG("[info] TimerTrigger: Timer expired - %s", ObjectName.ToString());
        CallLuaOnTimerExpired();
    }
}

void ATimerTrigger::StartTimer()
{
    if (bHasExpired)
    {
        UE_LOG("[warning] TimerTrigger: Cannot start - timer already expired. Use ResetTimer() first.");
        return;
    }

    bIsRunning = true;
    UE_LOG("[info] TimerTrigger: Timer started - %s (%.1fs)", ObjectName.ToString(), TriggerTime);
}

void ATimerTrigger::StopTimer()
{
    bIsRunning = false;
    UE_LOG("[info] TimerTrigger: Timer stopped - %s (Elapsed: %.1fs)", ObjectName.ToString(), ElapsedTime);
}

void ATimerTrigger::ResetTimer()
{
    bIsRunning = false;
    bHasExpired = false;
    ElapsedTime = 0.0f;
    RemainingTime = TriggerTime;
    LastLogTime = 0.0f;

    UE_LOG("[info] TimerTrigger: Timer reset - %s", ObjectName.ToString());
}

void ATimerTrigger::CallLuaOnTimerExpired()
{
    // TODO: Lua 스크립트에 OnTimerExpired() 함수가 있으면 호출
    UE_LOG("[info] TimerTrigger: OnTimerExpired - Looking for LevelTransitionManager");

    // LevelTransitionManager 찾아서 자동으로 다음 씬으로 전환
    if (!GetWorld())
    {
        UE_LOG("[error] TimerTrigger: World is null, cannot transition");
        return;
    }

    for (AActor* Actor : GetWorld()->GetActors())
    {
        ALevelTransitionManager* Manager = dynamic_cast<ALevelTransitionManager*>(Actor);
        if (Manager)
        {
            UE_LOG("[info] TimerTrigger: Found LevelTransitionManager, transitioning to next level");
            Manager->TransitionToNextLevel();
            return;
        }
    }

    UE_LOG("[error] TimerTrigger: LevelTransitionManager not found in scene!");
}

void ATimerTrigger::CallLuaOnTimerTick()
{
    // TODO: Lua 스크립트에 OnTimerTick(remainingTime) 함수가 있으면 호출
    // 현재는 아무것도 하지 않음 (로그 스팸 방지)
}
