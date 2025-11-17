// ────────────────────────────────────────────────────────────────────────────
// SpringArmComponent.cpp
// SpringArmComponent 구현
// ────────────────────────────────────────────────────────────────────────────
#include "pch.h"
#include "SpringArmComponent.h"
#include "Actor.h"

// ────────────────────────────────────────────────────────────────────────────
// 생성자 / 소멸자
// ────────────────────────────────────────────────────────────────────────────

USpringArmComponent::USpringArmComponent()
	: TargetArmLength(3.0f)
	, CurrentArmLength(3.0f)
	, SocketOffset(FVector())
	, TargetOffset(FVector())
	, bEnableCameraLag(false)
	, CameraLagSpeed(1.0f)
	, CameraLagMaxDistance(0.0f)
	, PreviousDesiredLocation(FVector())
	, PreviousActorLocation(FVector())
	, bEnableCameraRotationLag(false)
	, CameraRotationLagSpeed(1.0f)
	, PreviousDesiredRotation(FQuat::Identity())
	, bDoCollisionTest(true)
	, ProbeSize(1.20f)
	, SocketLocation(FVector())
	, SocketRotation(FQuat::Identity())
{
	bCanEverTick = true;
}

USpringArmComponent::~USpringArmComponent()
{
}

// ────────────────────────────────────────────────────────────────────────────
// 생명주기
// ────────────────────────────────────────────────────────────────────────────

void USpringArmComponent::TickComponent(float DeltaSeconds)
{
	Super::TickComponent(DeltaSeconds);

	FVector DesiredSocketLocation;
	FQuat DesiredSocketRotation;

	UpdateDesiredArmLocation(DeltaSeconds, DesiredSocketLocation, DesiredSocketRotation);

	// Collision Test
	FVector FinalSocketLocation = DesiredSocketLocation;
	if (bDoCollisionTest)
	{
		DoCollisionTest(DesiredSocketLocation, FinalSocketLocation);
	}

	// Socket 위치/회전 저장 (GetSocketLocation/Rotation에서 사용)
	SocketLocation = FinalSocketLocation;
	SocketRotation = DesiredSocketRotation;

	// 자식 컴포넌트(카메라)를 Socket 위치로 이동
	for (USceneComponent* Child : GetAttachChildren())
	{
		if (Child)
		{
			Child->SetWorldLocation(SocketLocation);
			Child->SetWorldRotation(SocketRotation);
		}
	}
}

// ────────────────────────────────────────────────────────────────────────────
// Socket Transform 조회
// ────────────────────────────────────────────────────────────────────────────

FVector USpringArmComponent::GetSocketLocation() const
{
	return SocketLocation;
}

FQuat USpringArmComponent::GetSocketRotation() const
{
	return SocketRotation;
}

FTransform USpringArmComponent::GetSocketTransform() const
{
	return FTransform(SocketLocation, SocketRotation, FVector(1.0f, 1.0f, 1.0f));
}

// ────────────────────────────────────────────────────────────────────────────
// 내부 함수
// ────────────────────────────────────────────────────────────────────────────

void USpringArmComponent::UpdateDesiredArmLocation(float DeltaTime, FVector& OutDesiredLocation, FQuat& OutDesiredRotation)
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		OutDesiredLocation = GetWorldLocation();
		OutDesiredRotation = GetWorldRotation();
		return;
	}

	// 회전 - Owner(캐릭터)의 월드 회전을 따라감
	FQuat OwnerRotation = OwnerActor->GetActorRotation();

	// 기본 위치: Owner의 위치 + TargetOffset (Owner의 로컬 좌표계 기준)
	FVector OwnerLocation = OwnerActor->GetActorLocation();
	FVector RotatedTargetOffset = OwnerRotation.RotateVector(TargetOffset);
	FVector TargetLocation = OwnerLocation + RotatedTargetOffset;

	// Spring Arm 방향 계산 (뒤쪽)
	FVector ArmDirection = OwnerRotation.GetForwardVector() * -1.0f; // Backward direction

	// SocketOffset도 Owner 회전 적용
	FVector RotatedSocketOffset = OwnerRotation.RotateVector(SocketOffset);

	// Desired Location
	FVector UnlaggedDesiredLocation = TargetLocation + ArmDirection * TargetArmLength + RotatedSocketOffset;

	// Camera Lag 적용
	if (bEnableCameraLag)
	{
		// 이전 위치가 초기값이면 즉시 설정
		if (PreviousDesiredLocation.IsZero())
		{
			PreviousDesiredLocation = UnlaggedDesiredLocation;
			PreviousActorLocation = OwnerLocation;
		}

		// Lag 적용
		FVector LagVector = UnlaggedDesiredLocation - PreviousDesiredLocation;
		float LagDistance = LagVector.Size();

		// MaxDistance 제한
		if (CameraLagMaxDistance > 0.0f && LagDistance > CameraLagMaxDistance)
		{
			PreviousDesiredLocation = UnlaggedDesiredLocation - LagVector.GetNormalized() * CameraLagMaxDistance;
		}

		// Lerp
		OutDesiredLocation = FVector::Lerp(PreviousDesiredLocation, UnlaggedDesiredLocation,
		                                   FMath::Min(1.0f, DeltaTime * CameraLagSpeed));
		PreviousDesiredLocation = OutDesiredLocation;
		PreviousActorLocation = OwnerLocation;
	}
	else
	{
		OutDesiredLocation = UnlaggedDesiredLocation;
	}

	// Rotation Lag 적용
	if (bEnableCameraRotationLag)
	{
		if (PreviousDesiredRotation.IsIdentity())
		{
			PreviousDesiredRotation = OwnerRotation;
		}

		float Alpha = FMath::Min(1.0f, DeltaTime * CameraRotationLagSpeed);
		OutDesiredRotation = FQuat::Slerp(PreviousDesiredRotation, OwnerRotation, Alpha);
		PreviousDesiredRotation = OutDesiredRotation;
	}
	else
	{
		OutDesiredRotation = OwnerRotation;
	}
}

bool USpringArmComponent::DoCollisionTest(const FVector& DesiredLocation, FVector& OutLocation)
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		OutLocation = DesiredLocation;
		return false;
	}

	// 간단한 Collision Test 구현
	// 실제로는 World의 CollisionManager를 사용하여 Raycast 수행
	// 여기서는 기본 구현만 제공

	FVector OwnerLocation = OwnerActor->GetActorLocation();
	FVector Direction = DesiredLocation - OwnerLocation;
	float Distance = Direction.Size();

	// TODO: Raycast 구현
	// 현재는 단순히 DesiredLocation 반환
	OutLocation = DesiredLocation;
	CurrentArmLength = TargetArmLength;

	return false;
}

// ────────────────────────────────────────────────────────────────────────────
// 복제 및 직렬화
// ────────────────────────────────────────────────────────────────────────────

void USpringArmComponent::DuplicateSubObjects()
{
	Super::DuplicateSubObjects();
}

void USpringArmComponent::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	Super::Serialize(bInIsLoading, InOutHandle);

	// TODO: 필요한 멤버 변수 직렬화 추가
}
