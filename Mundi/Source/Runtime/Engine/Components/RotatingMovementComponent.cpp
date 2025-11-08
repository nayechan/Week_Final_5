#include "pch.h"
#include "RotatingMovementComponent.h"
#include "SceneComponent.h"
#include "ObjectFactory.h"
// IMPLEMENT_CLASS is now auto-generated in .generated.cpp
URotatingMovementComponent::URotatingMovementComponent()
    : RotationRate(0.0f, 0.0f, 0.0f)
    , PivotTranslation(0.0f, 0.0f, 0.0f)
    , bRotationInLocalSpace(true)
{
    bCanEverTick = true;
}

URotatingMovementComponent::~URotatingMovementComponent()
{   
}

void URotatingMovementComponent::TickComponent(float DeltaSeconds)
{
    if (!UpdatedComponent)
        return;

    // 회전 델타 계산 (초당 도 * 델타 시간)
    FVector RotationDelta = RotationRate * DeltaSeconds;

    // 오일러 각도로부터 쿼터니언 생성 (Pitch, Yaw, Roll)
    FQuat DeltaRotation = FQuat::MakeFromEulerZYX(FVector(RotationDelta.X, RotationDelta.Y, RotationDelta.Z));

    if (bRotationInLocalSpace)
    {
        // 로컬 공간에서 회전 적용
        if (PivotTranslation.IsZero())
        {
            // 컴포넌트 원점을 중심으로 단순 회전
            UpdatedComponent->AddLocalRotation(DeltaRotation);
        }
        else
        {
            // 피벗 지점을 중심으로 회전
            FVector CurrentLocation = UpdatedComponent->GetRelativeLocation();
            FQuat CurrentRotation = UpdatedComponent->GetRelativeRotation();

            // 피벗 주변 회전
            FVector OffsetFromPivot = CurrentLocation - PivotTranslation;
            FVector RotatedOffset = DeltaRotation.RotateVector(OffsetFromPivot);
            FVector NewLocation = PivotTranslation + RotatedOffset;

            // 새로운 회전과 위치 적용
            UpdatedComponent->SetRelativeRotation(DeltaRotation * CurrentRotation);
            UpdatedComponent->SetRelativeLocation(NewLocation);
        }
    }
    else
    {
        // 월드 공간에서 회전 적용
        if (PivotTranslation.IsZero())
        {
            // 컴포넌트 원점을 중심으로 단순 회전
            UpdatedComponent->AddWorldRotation(DeltaRotation);
        }
        else
        {
            // 월드 공간에서 피벗 지점을 중심으로 회전
            FVector WorldPivot = UpdatedComponent->GetWorldLocation() +
                UpdatedComponent->GetWorldRotation().RotateVector(PivotTranslation);

            FVector CurrentWorldLocation = UpdatedComponent->GetWorldLocation();
            FQuat CurrentWorldRotation = UpdatedComponent->GetWorldRotation();

            // 월드 피벗 주변 회전
            FVector OffsetFromPivot = CurrentWorldLocation - WorldPivot;
            FVector RotatedOffset = DeltaRotation.RotateVector(OffsetFromPivot);
            FVector NewWorldLocation = WorldPivot + RotatedOffset;

            // 새로운 회전과 위치 적용
            UpdatedComponent->SetWorldRotation(DeltaRotation * CurrentWorldRotation);
            UpdatedComponent->SetWorldLocation(NewWorldLocation);
        }
    }
}

void URotatingMovementComponent::SetRotationRate(const FVector& NewRotationRate)
{
    RotationRate = NewRotationRate;
}

void URotatingMovementComponent::SetPivotTranslation(const FVector& NewPivotTranslation)
{
    PivotTranslation = NewPivotTranslation;
}

void URotatingMovementComponent::SetRotationInLocalSpace(bool bNewRotationInLocalSpace)
{
    bRotationInLocalSpace = bNewRotationInLocalSpace;
}
