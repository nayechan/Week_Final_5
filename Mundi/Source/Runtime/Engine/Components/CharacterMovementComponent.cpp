// ────────────────────────────────────────────────────────────────────────────
// CharacterMovementComponent.cpp
// Character 이동 컴포넌트 구현
// ────────────────────────────────────────────────────────────────────────────
#include "pch.h"
#include "CharacterMovementComponent.h"
#include "Character.h"
#include "SceneComponent.h"
#include "CapsuleComponent.h"
#include "World.h"
#include "PhysScene.h"
#include "PhysXPublic.h"

// ────────────────────────────────────────────────────────────────────────────
// 생성자 / 소멸자
// ────────────────────────────────────────────────────────────────────────────

UCharacterMovementComponent::UCharacterMovementComponent()
	: CharacterOwner(nullptr)
	, PendingInputVector(FVector())
	, MovementMode(EMovementMode::Walking)
	, TimeInAir(0.0f)
	, bIsJumping(false)
	// 이동 설정
	, MaxWalkSpeed(6.0f)
	, MaxAcceleration(20.480f)
	, GroundFriction(8.0f)
	, AirControl(0.05f)
	, BrakingDeceleration(20.480f)
	// 중력 설정
	, GravityScale(1.0f)
	, GravityDirection(0.0f, 0.0f, -1.0f)
	// 점프 설정
	, JumpZVelocity(10.0f)
	, MaxAirTime(5.0f)
	, bCanJump(true)
	// 바닥 감지 설정
	, WalkableFloorAngle(44.0f)
	, FloorSnapDistance(0.02f)
	, MaxStepHeight(45.0f)
{
	bCanEverTick = true;
}

UCharacterMovementComponent::~UCharacterMovementComponent()
{
}

// ────────────────────────────────────────────────────────────────────────────
// 생명주기
// ────────────────────────────────────────────────────────────────────────────

void UCharacterMovementComponent::BeginPlay()
{
	Super::BeginPlay();
	CharacterOwner = Cast<ACharacter>(Owner);
}

void UCharacterMovementComponent::TickComponent(float DeltaTime)
{
	Super::TickComponent(DeltaTime);

	if (!CharacterOwner || !UpdatedComponent)
	{
		return;
	}

	// 1. 속도 업데이트 (입력, 마찰, 가속)
	UpdateVelocity(DeltaTime);

	// 2. 중력 적용
	ApplyGravity(DeltaTime);

	// 3. 위치 업데이트
	MoveUpdatedComponent(DeltaTime);

	// 4. 지면 체크
	bool bWasGrounded = IsGrounded();
	bool bIsNowGrounded = CheckGround();

	// 5. 이동 모드 업데이트
	if (bIsNowGrounded && !bWasGrounded)
	{
		// 착지
		SetMovementMode(EMovementMode::Walking);
		float VerticalSpeed = FVector::Dot(Velocity, GravityDirection);
		Velocity -= GravityDirection * VerticalSpeed;
		TimeInAir = 0.0f;
		bIsJumping = false;
	}
	else if (!bIsNowGrounded && bWasGrounded)
	{
		// 낙하 시작
		SetMovementMode(EMovementMode::Falling);
	}

	// 6. 공중 시간 체크
	if (IsFalling())
	{
		TimeInAir += DeltaTime;
	}

	// 입력 초기화
	PendingInputVector = FVector();
}

// ────────────────────────────────────────────────────────────────────────────
// 이동 함수
// ────────────────────────────────────────────────────────────────────────────

void UCharacterMovementComponent::AddInputVector(FVector WorldDirection, float ScaleValue)
{
	if (ScaleValue == 0.0f || WorldDirection.SizeSquared() == 0.0f)
	{
		return;
	}

	float DotWithGravity = FVector::Dot(WorldDirection, GravityDirection);
	FVector HorizontalDirection = WorldDirection - (GravityDirection * DotWithGravity);

	if (HorizontalDirection.SizeSquared() < 0.0001f)
	{
		return;
	}

	FVector NormalizedDirection = HorizontalDirection.GetNormalized();
	PendingInputVector += NormalizedDirection * ScaleValue;
}

bool UCharacterMovementComponent::Jump()
{
	if (!bCanJump || !IsGrounded())
	{
		return false;
	}

	FVector JumpVelocity = GravityDirection * -1.0f * JumpZVelocity;
	Velocity += JumpVelocity;

	SetMovementMode(EMovementMode::Falling);
	bIsJumping = true;

	return true;
}

void UCharacterMovementComponent::StopJumping()
{
	FVector UpDirection = GravityDirection * -1.0f;
	float UpwardSpeed = FVector::Dot(Velocity, UpDirection);

	if (bIsJumping && UpwardSpeed > 0.0f)
	{
		Velocity -= UpDirection * (UpwardSpeed * 0.5f);
	}
}

void UCharacterMovementComponent::SetMovementMode(EMovementMode NewMode)
{
	if (MovementMode == NewMode)
	{
		return;
	}
	MovementMode = NewMode;
}

void UCharacterMovementComponent::SetGravityDirection(const FVector& NewDirection)
{
	GravityDirection = NewDirection.GetNormalized();
}

// ────────────────────────────────────────────────────────────────────────────
// 내부 이동 로직
// ────────────────────────────────────────────────────────────────────────────

void UCharacterMovementComponent::UpdateVelocity(float DeltaTime)
{
	FVector InputVector = PendingInputVector;
	if (InputVector.SizeSquared() > 1.0f)
	{
		InputVector = InputVector.GetNormalized();
	}

	if (InputVector.SizeSquared() > 0.0001f)
	{
		FVector TargetVelocity = InputVector * MaxWalkSpeed;
		FVector CurrentHorizontalVelocity = Velocity;

		float VerticalSpeed = FVector::Dot(Velocity, GravityDirection);
		CurrentHorizontalVelocity -= GravityDirection * VerticalSpeed;

		FVector VelocityDifference = TargetVelocity - CurrentHorizontalVelocity;

		float ControlPower = IsGrounded() ? 1.0f : AirControl;
		FVector Accel = VelocityDifference.GetNormalized() * MaxAcceleration * ControlPower * DeltaTime;

		if (Accel.SizeSquared() > VelocityDifference.SizeSquared())
		{
			Velocity += VelocityDifference;
		}
		else
		{
			Velocity += Accel;
		}
	}
	else if (IsGrounded())
	{
		FVector HorizontalVelocity = Velocity;
		float VerticalSpeed = FVector::Dot(Velocity, GravityDirection);
		HorizontalVelocity -= GravityDirection * VerticalSpeed;

		float Speed = HorizontalVelocity.Size();
		if (Speed > 0.001f)
		{
			float Deceleration = GroundFriction * MaxWalkSpeed * DeltaTime;
			float NewSpeed = FMath::Max(0.0f, Speed - Deceleration);
			HorizontalVelocity = HorizontalVelocity.GetNormalized() * NewSpeed;
			Velocity = HorizontalVelocity + GravityDirection * VerticalSpeed;
		}
	}

	// 최대 속도 제한
	FVector HorizontalVelocity = Velocity;
	float VerticalSpeed = FVector::Dot(Velocity, GravityDirection);
	HorizontalVelocity -= GravityDirection * VerticalSpeed;

	if (HorizontalVelocity.Size() > MaxWalkSpeed)
	{
		HorizontalVelocity = HorizontalVelocity.GetNormalized() * MaxWalkSpeed;
		Velocity = HorizontalVelocity + GravityDirection * VerticalSpeed;
	}
}

void UCharacterMovementComponent::ApplyGravity(float DeltaTime)
{
	if (IsGrounded())
	{
		return;
	}

	FVector GravityAccel = GravityDirection * DefaultGravity * GravityScale * DeltaTime;
	Velocity += GravityAccel;

	float FallSpeed = FVector::Dot(Velocity, GravityDirection);
	if (FallSpeed > 4000.0f)
	{
		Velocity -= GravityDirection * (FallSpeed - 4000.0f);
	}
}

void UCharacterMovementComponent::MoveUpdatedComponent(float DeltaTime)
{
	if (!UpdatedComponent)
	{
		return;
	}

	// 단순 이동 (충돌 처리 없음)
	FVector Delta = Velocity * DeltaTime;
	FVector NewLocation = UpdatedComponent->GetWorldLocation() + Delta;
	UpdatedComponent->SetWorldLocation(NewLocation);
}

// Phase 3용 - 현재 사용 안 함
bool UCharacterMovementComponent::SafeMoveUpdatedComponent(const FVector& Delta, FHitResult& OutHit)
{
	OutHit.Init();
	return false;
}

// Phase 3용 - 현재 사용 안 함
FVector UCharacterMovementComponent::ComputeSlideVector(const FVector& Delta, const FVector& Normal, const FHitResult& Hit) const
{
	return FVector::Zero();
}

// Phase 3용 - 현재 사용 안 함
void UCharacterMovementComponent::SlideAlongSurface(const FVector& Delta, int32 MaxIterations)
{
}

// Phase 4용 - 현재 사용 안 함
bool UCharacterMovementComponent::TryStepUp(const FVector& Delta, const FHitResult& Hit)
{
	return false;
}

// ────────────────────────────────────────────────────────────────────────────
// Phase 2: 바닥 감지 (캡슐 스윕)
// ────────────────────────────────────────────────────────────────────────────

bool UCharacterMovementComponent::CheckGround()
{
	if (!UpdatedComponent || !CharacterOwner)
	{
		return false;
	}

	// 위로 올라가는 중이면 (점프 중) 지면 체크 스킵
	float VerticalVelocity = FVector::Dot(Velocity, GravityDirection);
	if (VerticalVelocity < -0.1f)
	{
		CurrentFloor.Clear();
		return false;
	}

	// FindFloor로 바닥 탐색
	FFindFloorResult FloorResult;
	if (FindFloor(FloorResult))
	{
		CurrentFloor = FloorResult;

		if (FloorResult.IsWalkableFloor() && FloorResult.FloorDist <= FloorSnapDistance)
		{
			// 바닥에 스냅
			SnapToFloor();
			return true;
		}
	}
	else
	{
		CurrentFloor.Clear();
	}

	return false;
}

bool UCharacterMovementComponent::FindFloor(FFindFloorResult& OutFloorResult, float SweepDistance)
{
	OutFloorResult.Clear();

	if (!UpdatedComponent || !CharacterOwner)
	{
		return false;
	}

	UCapsuleComponent* Capsule = CharacterOwner->GetCapsuleComponent();
	if (!Capsule)
	{
		return false;
	}

	UWorld* World = CharacterOwner->GetWorld();
	if (!World || !World->GetPhysicsScene())
	{
		return false;
	}

	FPhysScene* PhysScene = World->GetPhysicsScene();

	if (SweepDistance < 0.0f)
	{
		SweepDistance = FloorSnapDistance + 3.0f;
	}

	float CapsuleRadius = Capsule->GetScaledCapsuleRadius();
	float CapsuleHalfHeight = Capsule->GetScaledCapsuleHalfHeight();

	FVector CapsuleLocation = UpdatedComponent->GetWorldLocation();
	FVector Start = CapsuleLocation;
	FVector End = Start + GravityDirection * SweepDistance;

	FHitResult Hit;
	if (PhysScene->SweepSingleCapsule(Start, End, CapsuleRadius, CapsuleHalfHeight, Hit, CharacterOwner))
	{
		OutFloorResult.bBlockingHit = true;
		OutFloorResult.FloorDist = Hit.Distance;
		OutFloorResult.HitLocation = Hit.ImpactPoint;
		OutFloorResult.FloorNormal = Hit.ImpactNormal;
		OutFloorResult.FloorZ = Hit.ImpactPoint.Z;
		OutFloorResult.HitActor = Hit.Actor;
		OutFloorResult.HitComponent = Hit.Component;
		OutFloorResult.bWalkableFloor = IsWalkable(Hit.ImpactNormal);

		return true;
	}

	return false;
}

bool UCharacterMovementComponent::IsWalkable(const FVector& Normal) const
{
	FVector UpDirection = GravityDirection * -1.0f;
	float CosAngle = FVector::Dot(Normal, UpDirection);
	float WalkableCos = std::cos(DegreesToRadians(WalkableFloorAngle));
	return CosAngle >= WalkableCos;
}

void UCharacterMovementComponent::SnapToFloor()
{
	if (!UpdatedComponent || !CurrentFloor.bBlockingHit)
	{
		return;
	}

	FVector Location = UpdatedComponent->GetWorldLocation();

	if (CurrentFloor.FloorDist > 0.01f)
	{
		Location.Z -= CurrentFloor.FloorDist;
		UpdatedComponent->SetWorldLocation(Location);
	}
}

// ────────────────────────────────────────────────────────────────────────────
// 복제
// ────────────────────────────────────────────────────────────────────────────

void UCharacterMovementComponent::DuplicateSubObjects()
{
	Super::DuplicateSubObjects();
}

void UCharacterMovementComponent::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	Super::Serialize(bInIsLoading, InOutHandle);
}
