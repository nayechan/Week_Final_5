#include "pch.h"
#include "ProjectileMovementComponent.h"
#include "SceneComponent.h"
#include "Actor.h"
#include "ObjectFactory.h"
// IMPLEMENT_CLASS is now auto-generated in .generated.cpp
UProjectileMovementComponent::UProjectileMovementComponent()
    : Gravity(-9.80f)  // Z-Up 좌표계에서 중력은 Z방향으로 -980 cm/s^2
    , InitialSpeed(30.0f)
    , MaxSpeed(0.0f)  // 0 = 제한 없음
    , HomingTargetActor(nullptr)
    , HomingTargetComponent(nullptr)
    , HomingAccelerationMagnitude(0.0f)
    , bIsHomingProjectile(false)
    , bRotationFollowsVelocity(true)
    , ProjectileLifespan(0.0f)  // 0 = 무제한
    , CurrentLifetime(0.0f)
    , bAutoDestroyWhenLifespanExceeded(false)
    , bIsActive(true)
{
    bCanEverTick = true;
}

UProjectileMovementComponent::~UProjectileMovementComponent()
{
}

void UProjectileMovementComponent::TickComponent(float DeltaSeconds)
{
    if (!UpdatedComponent)
    {
        return;
    }

    // 1. 생명주기 체크
    if (ProjectileLifespan > 0.0f)
    {
        CurrentLifetime += DeltaSeconds;
        if (CurrentLifetime >= ProjectileLifespan)
        {
            if (bAutoDestroyWhenLifespanExceeded)
            {
                // Owner Actor 파괴 (현재 지연 삭제 처리가 없어서 안보이게만 처리.)
                AActor* Owner = UpdatedComponent->GetOwner();
                if (Owner)
                {
                    //Owner->Destroy();
                    Owner->SetActorHiddenInGame(true);
                    return;
                }
            }
            bIsActive = false;
            return;
        }
    }

    // 2. 호밍 가속도 계산
    if (bIsHomingProjectile)
    {
        ComputeHomingAcceleration(DeltaSeconds);
    }

    // 3. 중력 적용
    Velocity.Z += Gravity * DeltaSeconds;

    // 4. 가속도 적용
    Velocity += Acceleration * DeltaSeconds;

    // 5. 속도 제한
    LimitVelocity();

    // 6. 위치 업데이트
    FVector Delta = Velocity * DeltaSeconds;
    if (!Delta.IsZero())
    {
        UpdatedComponent->AddWorldOffset(Delta);
    }

    // 7. 회전 업데이트 (속도 방향 추적)
    if (bRotationFollowsVelocity)
    {
        UpdateRotationFromVelocity();
    }
}

void UProjectileMovementComponent::FireInDirection(const FVector& ShootDirection)
{
    // 방향 벡터를 정규화하고 InitialSpeed를 곱해 속도 설정
    FVector NormalizedDirection = ShootDirection;
    NormalizedDirection.Normalize();

    Velocity = NormalizedDirection * InitialSpeed;

    // 상태 초기화
    bIsActive = true;
    CurrentLifetime = 0.0f;
}

void UProjectileMovementComponent::SetVelocityInLocalSpace(const FVector& NewVelocity)
{
    if (!UpdatedComponent)
        return;

    // 로컬 공간 속도를 월드 공간으로 변환
    FQuat WorldRotation = UpdatedComponent->GetWorldRotation();
    Velocity = WorldRotation.RotateVector(NewVelocity);
}

void UProjectileMovementComponent::SetHomingTarget(AActor* Target)
{
    HomingTargetActor = Target;
    HomingTargetComponent = nullptr;  // Component가 우선순위가 높으므로 초기화
}

void UProjectileMovementComponent::SetHomingTarget(USceneComponent* Target)
{
    HomingTargetComponent = Target;
    HomingTargetActor = nullptr;
}

void UProjectileMovementComponent::LimitVelocity()
{
    if (MaxSpeed <= 0.0f)
        return;

    float CurrentSpeed = Velocity.Size();
    if (CurrentSpeed > MaxSpeed)
    {
        Velocity.Normalize();
        Velocity *= MaxSpeed;
    }
}

void UProjectileMovementComponent::ComputeHomingAcceleration(float DeltaTime)
{
    if (HomingAccelerationMagnitude <= 0.0f)
        return;

    if (!UpdatedComponent)
        return;

    FVector TargetLocation;
    bool bHasValidTarget = false;

    // Component가 Actor보다 우선순위가 높음
    if (HomingTargetComponent)
    {
        TargetLocation = HomingTargetComponent->GetWorldLocation();
        bHasValidTarget = true;
    }
    else if (HomingTargetActor)
    {
        TargetLocation = HomingTargetActor->GetActorLocation();
        bHasValidTarget = true;
    }

    if (!bHasValidTarget)
        return;

    FVector CurrentLocation = UpdatedComponent->GetWorldLocation();
    FVector ToTarget = TargetLocation - CurrentLocation;

    // 타겟까지의 거리가 너무 가까우면 호밍 중지
    if (ToTarget.Size() < 10.0f)  // 10cm 이내
        return;

    ToTarget.Normalize();

    // 호밍 가속도를 타겟 방향으로 설정
    Acceleration = ToTarget * HomingAccelerationMagnitude;
}

void UProjectileMovementComponent::UpdateRotationFromVelocity()
{
    if (!UpdatedComponent)
        return;

    if (Velocity.IsZero())
        return;

    // 속도 방향으로 회전 계산
    FVector Direction = Velocity;
    Direction.Normalize();

    // DirectX Z-Up 좌표계에서 방향 벡터로부터 회전 생성
    // Forward = 속도 방향
    //FQuat NewRotation = FQuat::FromDirectionVector(Direction);

   // UpdatedComponent->SetWorldRotation(NewRotation);
}