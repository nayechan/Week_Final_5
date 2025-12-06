#pragma once
#include "Info.h"
#include "ATimerTrigger.generated.h"

/**
 * 타이머 트리거 - 설정된 시간이 지나면 Lua 함수 호출
 *
 * 사용법:
 * 1. 씬에 TimerTrigger 배치
 * 2. TriggerTime 설정 (초 단위)
 * 3. Lua 스크립트 부착하고 OnTimerExpired() 함수 구현
 *
 * 예시 Lua (수집 스테이지 - 60초 제한):
 * function OnTimerExpired()
 *     print("Time's up! Moving to next stage...")
 *     TransitionToNextLevel()
 * end
 *
 * 예시 Lua (컷씬 - 10초 후 자동 전환):
 * function OnTimerExpired()
 *     TransitionToNextLevel()
 * end
 */
UCLASS(DisplayName="ATimerTrigger", Description="설정된 시간이 지나면 Lua 함수를 호출합니다")
class ATimerTrigger : public AInfo
{
public:
    GENERATED_REFLECTION_BODY()

    ATimerTrigger();
    virtual ~ATimerTrigger() override;

    // ════════════════════════════════════════════════════════════════════════
    // 설정

    /**
     * 트리거가 발동될 시간 (초)
     */
    UPROPERTY(EditAnywhere)
    float TriggerTime = 60.0f;

    /**
     * 자동으로 시작할지 여부 (BeginPlay에서 시작)
     */
    UPROPERTY(EditAnywhere)
    bool bAutoStart = true;

    /**
     * 남은 시간을 주기적으로 출력할지 여부 (디버깅용)
     */
    UPROPERTY(EditAnywhere)
    bool bShowRemainingTime = true;

    // ════════════════════════════════════════════════════════════════════════
    // Actor 오버라이드

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;

    // ════════════════════════════════════════════════════════════════════════
    // API

    /**
     * 타이머 시작
     */
    void StartTimer();

    /**
     * 타이머 정지
     */
    void StopTimer();

    /**
     * 타이머 리셋
     */
    void ResetTimer();

    /**
     * 남은 시간 가져오기 (초)
     */
    float GetRemainingTime() const { return RemainingTime; }

    /**
     * 경과 시간 가져오기 (초)
     */
    float GetElapsedTime() const { return ElapsedTime; }

    /**
     * 타이머가 실행 중인지 확인
     */
    bool IsRunning() const { return bIsRunning; }

    /**
     * 타이머가 만료되었는지 확인
     */
    bool HasExpired() const { return bHasExpired; }

private:
    bool bIsRunning = false;
    bool bHasExpired = false;
    float ElapsedTime = 0.0f;
    float RemainingTime = 0.0f;
    float LastLogTime = 0.0f;  // 마지막으로 로그를 출력한 시간

    /**
     * Lua OnTimerExpired() 함수 호출
     */
    void CallLuaOnTimerExpired();

    /**
     * Lua OnTimerTick(remainingTime) 함수 호출 (매 프레임)
     */
    void CallLuaOnTimerTick();
};
