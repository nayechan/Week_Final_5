#pragma once

#include "MovementComponent.h"
#include "Vector.h"
#include "UProjectileMovementComponent.generated.h"

class AActor;
class USceneComponent;

/**
 * UProjectileMovementComponent
 * 발사체(Projectile)의 움직임을 시뮬레이션하는 컴포넌트
 * 중력, 바운스, 호밍 등의 기능을 지원
 */
class UProjectileMovementComponent : public UMovementComponent
{
public:

    GENERATED_REFLECTION_BODY()

    UProjectileMovementComponent();

protected:
    ~UProjectileMovementComponent() override;

public:
    // Life Cycle
    virtual void TickComponent(float DeltaSeconds) override;

    // 발사 API
    void FireInDirection(const FVector& ShootDirection);
    void SetVelocityInLocalSpace(const FVector& NewVelocity);

    // 물리 속성 Getter/Setter
    void SetGravity(float NewGravity) { Gravity = NewGravity; }
    float GetGravity() const { return Gravity; }

    void SetInitialSpeed(float NewInitialSpeed) { InitialSpeed = NewInitialSpeed; }
    float GetInitialSpeed() const { return InitialSpeed; }

    void SetMaxSpeed(float NewMaxSpeed) { MaxSpeed = NewMaxSpeed; }
    float GetMaxSpeed() const { return MaxSpeed; }

    // 호밍 속성 Getter/Setter
    void SetHomingTarget(AActor* Target);
    void SetHomingTarget(USceneComponent* Target);
    AActor* GetHomingTargetActor() const { return HomingTargetActor; }
    USceneComponent* GetHomingTargetComponent() const { return HomingTargetComponent; }

    void SetHomingAccelerationMagnitude(float NewMagnitude) { HomingAccelerationMagnitude = NewMagnitude; }
    float GetHomingAccelerationMagnitude() const { return HomingAccelerationMagnitude; }

    void SetIsHomingProjectile(bool bNewIsHoming) { bIsHomingProjectile = bNewIsHoming; }
    bool IsHomingProjectile() const { return bIsHomingProjectile; }

    // 회전 속성 Getter/Setter
    void SetRotationFollowsVelocity(bool bNewRotationFollows) { bRotationFollowsVelocity = bNewRotationFollows; }
    bool GetRotationFollowsVelocity() const { return bRotationFollowsVelocity; }

    // 생명주기 Getter/Setter
    void SetProjectileLifespan(float NewLifespan) { ProjectileLifespan = NewLifespan; }
    float GetProjectileLifespan() const { return ProjectileLifespan; }

    void SetAutoDestroyWhenLifespanExceeded(bool bNewAutoDestroy) { bAutoDestroyWhenLifespanExceeded = bNewAutoDestroy; }
    bool GetAutoDestroyWhenLifespanExceeded() const { return bAutoDestroyWhenLifespanExceeded; }

    // 상태 API
    void SetActive(bool bNewActive) { bIsActive = bNewActive; }
    bool IsActive() const { return bIsActive; }

    void ResetLifetime() { CurrentLifetime = 0.0f; }
    float GetCurrentLifetime() const { return CurrentLifetime; }

protected:
    // 내부 헬퍼 함수
    void LimitVelocity();
    void ComputeHomingAcceleration(float DeltaTime);
    void UpdateRotationFromVelocity();

protected:
    // [PIE] 값 복사

    // === 물리 속성 ===
    // 중력 가속도 (cm/s^2), Z-Up 좌표계에서 Z가 음수면 아래로 떨어짐
    UPROPERTY(EditAnywhere, Category="발사체", Tooltip="발사체 중력 가속도입니다")
    float Gravity;

    UPROPERTY(EditAnywhere, Category="속도", Tooltip="초기 속도입니다")
    FVector Velocity;

    // 발사 시 초기 속도 (cm/s)
    UPROPERTY(EditAnywhere, Category="발사체", Tooltip="발사체 초기 속도입니다")
    float InitialSpeed;

    // 최대 속도 제한 (cm/s), 0이면 제한 없음
    UPROPERTY(EditAnywhere, Category="발사체", Tooltip="발사체 최대 속도입니다")
    float MaxSpeed;

    // === 호밍 속성 ===
    // 호밍 타겟 액터
    AActor* HomingTargetActor;

    // 호밍 타겟 컴포넌트 (우선순위가 더 높음)
    USceneComponent* HomingTargetComponent;

    // 호밍 가속도 크기 (cm/s^2)
    UPROPERTY(EditAnywhere, Category="호밍", Tooltip="호밍 가속도 크기입니다")
    float HomingAccelerationMagnitude;

    // 호밍 기능 활성화 여부
    UPROPERTY(EditAnywhere, Category="호밍", Tooltip="호밍 기능을 활성화합니다")
    bool bIsHomingProjectile;

    // === 회전 속성 ===
    // 속도 방향으로 회전 여부
    bool bRotationFollowsVelocity;

    // === 생명주기 속성 ===
    // 발사체 생명 시간 (초), 0이면 무제한
    UPROPERTY(EditAnywhere, Category="발사체", Tooltip="발사체 생명 시간입니다")
    float ProjectileLifespan;

    // 현재 생존 시간 (초)
    float CurrentLifetime;

    // 생명 시간 초과 시 자동 파괴 여부
    UPROPERTY(EditAnywhere, Category="발사체", Tooltip="생명 시간 초과시 발사체를 파괴합니다")
    bool bAutoDestroyWhenLifespanExceeded;

    // === 상태 ===
    // 활성화 상태
    bool bIsActive;
};
