#pragma once
#include "Character.h"
#include "AFirefighterCharacter.generated.h"

class UAudioComponent;

UCLASS(DisplayName = "파이어 파이터 캐릭터", Description = "렛츠고 파이어 파이터")
class AFirefighterCharacter : public ACharacter
{
    GENERATED_REFLECTION_BODY()

public:
    AFirefighterCharacter();

protected:
    ~AFirefighterCharacter() override;

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void PossessedBy(AController* NewController) override;
    virtual void HandleAnimNotify(const FAnimNotifyEvent& NotifyEvent) override;

    // ────────────────────────────────────────────────
    // 입력 바인딩
    // ────────────────────────────────────────────────

    virtual void SetupPlayerInputComponent(UInputComponent* InInputComponent) override;

    // ────────────────────────────────────────────────
    // 이동 입력 (카메라 방향 기준)
    // ────────────────────────────────────────────────

    /** 카메라 방향 기준 앞/뒤 이동 */
    void MoveForwardCamera(float Value);

    /** 카메라 방향 기준 좌/우 이동 */
    void MoveRightCamera(float Value);

private:
    // ────────────────────────────────────────────────
    // 회전 설정
    // ────────────────────────────────────────────────

    /** 이동 방향으로 캐릭터 회전 여부 */
    bool bOrientRotationToMovement;

    /** 회전 보간 속도 (도/초) */
    float RotationRate;

    /** 현재 프레임의 이동 방향 (회전 계산용) */
    FVector CurrentMovementDirection;

    //USound* SorrySound;
    //USound* HitSound;
};
