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
	// 경사면 미끄러짐 설정
	, bEnableSlopeSliding(true)
	, SlopeSlideSpeed(0.5f)
	, SlopeFriction(0.6f)
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

	// 3. 가파른 경사면 미끄러짐 처리
	HandleSlopeSliding(DeltaTime);

	// 4. 위치 업데이트
	MoveUpdatedComponent(DeltaTime);

	// 5. 지면 체크
	bool bWasGrounded = IsGrounded();
	bool bIsNowGrounded = CheckGround();

	// 6. 이동 모드 업데이트
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

	// 7. 공중 시간 체크
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

	FVector Delta = Velocity * DeltaTime;

	if (Delta.SizeSquared() < KINDA_SMALL_NUMBER)
	{
		return;
	}

	// 수평/수직 이동 분리
	// 1. 먼저 침투 해제
	ResolvePenetration();

	// 2. 수평 이동 (XY) - 가파른 경사면 올라가기 방지
	FVector HorizontalDelta = FVector(Delta.X, Delta.Y, 0.0f);
	if (HorizontalDelta.SizeSquared() > KINDA_SMALL_NUMBER)
	{
		FVector BeforeLocation = UpdatedComponent->GetWorldLocation();
		SlideAlongSurface(HorizontalDelta);
		FVector AfterLocation = UpdatedComponent->GetWorldLocation();

		float ZDiff = AfterLocation.Z - BeforeLocation.Z;

		// 수평 이동인데 Z가 올라갔으면 경사면 체크
		if (ZDiff > KINDA_SMALL_NUMBER)
		{
			// 발 밑 바닥 체크해서 걸을 수 있는 경사인지 확인
			FFindFloorResult FloorCheck;
			if (FindFloor(FloorCheck, 0.5f))
			{
				// 걸을 수 없는 가파른 경사면이면 Z 되돌림
				if (!FloorCheck.bWalkableFloor)
				{
					AfterLocation.Z = BeforeLocation.Z;
					UpdatedComponent->SetWorldLocation(AfterLocation);
				}
			}
			else
			{
				// 바닥이 없으면 Z 되돌림
				AfterLocation.Z = BeforeLocation.Z;
				UpdatedComponent->SetWorldLocation(AfterLocation);
			}
		}
	}

	// 3. 수직 이동 (Z)
	FVector VerticalDelta = FVector(0.0f, 0.0f, Delta.Z);
	if (VerticalDelta.SizeSquared() > KINDA_SMALL_NUMBER)
	{
		SlideAlongSurface(VerticalDelta);
	}
}

// ────────────────────────────────────────────────────────────────────────────
// Phase 3: 벽 충돌 + 슬라이딩
// ────────────────────────────────────────────────────────────────────────────

bool UCharacterMovementComponent::SafeMoveUpdatedComponent(const FVector& Delta, FHitResult& OutHit)
{
	OutHit.Init();

	if (!UpdatedComponent || !CharacterOwner)
	{
		return false;
	}

	if (Delta.SizeSquared() < KINDA_SMALL_NUMBER)
	{
		return false;
	}

	UCapsuleComponent* Capsule = CharacterOwner->GetCapsuleComponent();
	if (!Capsule)
	{
		FVector NewLocation = UpdatedComponent->GetWorldLocation() + Delta;
		UpdatedComponent->SetWorldLocation(NewLocation);
		return true;
	}

	UWorld* World = CharacterOwner->GetWorld();
	if (!World || !World->GetPhysicsScene())
	{
		FVector NewLocation = UpdatedComponent->GetWorldLocation() + Delta;
		UpdatedComponent->SetWorldLocation(NewLocation);
		return true;
	}

	FPhysScene* PhysScene = World->GetPhysicsScene();

	// 스킨 두께 - 충돌 감지용 여유 공간
	const float SkinWidth = 0.1f;

	float CapsuleRadius = Capsule->GetScaledCapsuleRadius();
	float CapsuleHalfHeight = Capsule->GetScaledCapsuleHalfHeight();

	// 스윕용 캡슐은 스킨 두께만큼 작게
	float SweepRadius = FMath::Max(0.01f, CapsuleRadius - SkinWidth);
	float SweepHalfHeight = FMath::Max(0.01f, CapsuleHalfHeight - SkinWidth);

	FVector Start = UpdatedComponent->GetWorldLocation();
	FVector End = Start + Delta;

	// 캡슐 스윕으로 충돌 감지
	if (PhysScene->SweepSingleCapsule(Start, End, SweepRadius, SweepHalfHeight, OutHit, CharacterOwner))
	{
		// 시작 위치에서 이미 침투 상태 → 이동하지 않음 (ResolvePenetration에서 처리)
		if (OutHit.Distance <= KINDA_SMALL_NUMBER)
		{
			OutHit.bBlockingHit = true;
			return true;
		}

		// 충돌 발생 - 충돌 지점까지만 이동
		float SafeDistance = OutHit.Distance;
		FVector SafeDelta = Delta.GetNormalized() * SafeDistance;
		FVector NewLocation = Start + SafeDelta;
		UpdatedComponent->SetWorldLocation(NewLocation);
		OutHit.bBlockingHit = true;
		return true;
	}
	else
	{
		// 충돌 없음, 전체 이동
		UpdatedComponent->SetWorldLocation(End);
		return true;
	}
}

FVector UCharacterMovementComponent::ComputeSlideVector(const FVector& Delta, const FVector& Normal, const FHitResult& Hit) const
{
	// 기본 슬라이드: 충돌면에 평행한 성분만 남김
	// SlideVector = Delta - (Delta · Normal) * Normal
	float DotWithNormal = FVector::Dot(Delta, Normal);
	FVector SlideVector = Delta - Normal * DotWithNormal;

	return SlideVector;
}

void UCharacterMovementComponent::SlideAlongSurface(const FVector& Delta, int32 MaxIterations)
{
	if (!UpdatedComponent || Delta.SizeSquared() < KINDA_SMALL_NUMBER)
	{
		return;
	}

	FVector RemainingDelta = Delta;
	int32 Iterations = 0;

	while (Iterations < MaxIterations && RemainingDelta.SizeSquared() > KINDA_SMALL_NUMBER)
	{
		FHitResult Hit;

		if (!SafeMoveUpdatedComponent(RemainingDelta, Hit))
		{
			break;
		}

		if (!Hit.bBlockingHit)
		{
			// 충돌 없이 이동 완료
			break;
		}

		// 충돌 발생 - 남은 거리 계산
		float MovedDistance = Hit.Distance;
		float TotalDistance = RemainingDelta.Size();

		if (TotalDistance < KINDA_SMALL_NUMBER)
		{
			break;
		}

		float RemainingRatio = 1.0f - (MovedDistance / TotalDistance);
		FVector LeftoverDelta = RemainingDelta * RemainingRatio;

		// 바닥 충돌 시 수직 속도 제거
		if (IsWalkable(Hit.ImpactNormal) && Velocity.Z < 0.0f)
		{
			Velocity.Z = 0.0f;
		}

		// 90도 이상 (법선 Z가 0 이하 = 벽/천장/둔각)이면 슬라이드 중단
		if (Hit.ImpactNormal.Z <= 0.0f)
		{
			// 위로 가던 속도 제거
			if (Velocity.Z > 0.0f)
			{
				Velocity.Z = 0.0f;
				bIsJumping = false;
			}
			break;
		}

		// 슬라이드 벡터 계산
		RemainingDelta = ComputeSlideVector(LeftoverDelta, Hit.ImpactNormal, Hit);

		// 가파른 경사면(45도 초과 ~ 90도 미만)에서 위로 올라가는 것 방지
		if (!IsWalkable(Hit.ImpactNormal))
		{
			// 슬라이드 결과가 위로 향하면 제거
			if (RemainingDelta.Z > 0.0f)
			{
				RemainingDelta.Z = 0.0f;
			}

			// 속도에서도 위로 향하는 성분 제거
			if (Velocity.Z > 0.0f)
			{
				Velocity.Z = 0.0f;
				bIsJumping = false;
			}
		}

		Iterations++;
	}
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
		OutFloorResult.HitActor = Hit.Actor.Get();
		OutFloorResult.HitComponent = Hit.Component.Get();
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

bool UCharacterMovementComponent::ResolvePenetration()
{
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

	float CapsuleRadius = Capsule->GetScaledCapsuleRadius();
	float CapsuleHalfHeight = Capsule->GetScaledCapsuleHalfHeight();
	FVector CurrentLocation = UpdatedComponent->GetWorldLocation();

	FVector MTD;
	float PenetrationDepth;

	// MTD를 사용하여 침투 감지 및 해제
	if (PhysScene->ComputePenetrationCapsule(CurrentLocation, CapsuleRadius, CapsuleHalfHeight,
	                                          MTD, PenetrationDepth, CharacterOwner))
	{
		// 침투가 감지됨 - MTD 방향으로 밀어냄
		const float PushOutDistance = PenetrationDepth + 0.00125f;
		FVector Adjustment = MTD * PushOutDistance;

		// 걸을 수 없는 표면 (벽, 가파른 경사, 천장/둔각 등)
		if (!IsWalkable(MTD))
		{
			// 수평 방향으로만 밀어내기
			FVector HorizontalMTD = FVector(MTD.X, MTD.Y, 0.0f);
			if (HorizontalMTD.SizeSquared() > KINDA_SMALL_NUMBER)
			{
				Adjustment = HorizontalMTD.GetNormalized() * PushOutDistance;
			}

			// 속도를 튕겨내기 (벽으로 들어가는 속도 성분 제거 + 반사)
			FVector PushDir = FVector(MTD.X, MTD.Y, 0.0f).GetNormalized();
			float IncomingSpeed = -FVector::Dot(Velocity, PushDir);
			if (IncomingSpeed > 0.0f)
			{
				Velocity += PushDir * (IncomingSpeed * 1.2f);
			}

			// 위로 가던 속도 제거
			if (Velocity.Z > 0.0f)
			{
				Velocity.Z = 0.0f;
				bIsJumping = false;
			}
		}

		FVector NewLocation = CurrentLocation + Adjustment;
		UpdatedComponent->SetWorldLocation(NewLocation);

		return true;
	}

	return false;
}

// ────────────────────────────────────────────────────────────────────────────
// 경사면 미끄러짐
// ────────────────────────────────────────────────────────────────────────────

bool UCharacterMovementComponent::IsOnSteepSlope() const
{
	// 바닥이 감지되었고, 걸을 수 없는 가파른 경사인 경우
	if (CurrentFloor.bBlockingHit && !CurrentFloor.bWalkableFloor)
	{
		return true;
	}

	return false;
}

void UCharacterMovementComponent::HandleSlopeSliding(float DeltaTime)
{
	if (!bEnableSlopeSliding)
	{
		return;
	}

	if (!UpdatedComponent || !CharacterOwner)
	{
		return;
	}

	// 가파른 경사에 있는지 확인
	// 먼저 바닥을 탐색해서 경사 정보를 얻음
	FFindFloorResult FloorResult;
	if (!FindFloor(FloorResult, FloorSnapDistance + 0.5f))
	{
		return;
	}

	// 걸을 수 있는 바닥이면 미끄러짐 없음
	if (FloorResult.bWalkableFloor)
	{
		return;
	}

	// 바닥이 감지되었지만 걸을 수 없는 가파른 경사면
	if (FloorResult.bBlockingHit)
	{
		// 경사면의 법선 벡터
		FVector FloorNormal = FloorResult.FloorNormal;

		// 경사면 아래 방향 계산 (중력 방향을 경사면에 투영)
		// SlideDirection = GravityDirection - (GravityDirection · FloorNormal) * FloorNormal
		float DotWithNormal = FVector::Dot(GravityDirection, FloorNormal);
		FVector SlideDirection = GravityDirection - FloorNormal * DotWithNormal;

		// 슬라이드 방향 정규화
		if (SlideDirection.SizeSquared() > KINDA_SMALL_NUMBER)
		{
			SlideDirection = SlideDirection.GetNormalized();
		}
		else
		{
			return;
		}

		// 경사 각도에 따른 미끄러짐 강도 계산
		// 각도가 가파를수록 더 빠르게 미끄러짐
		FVector UpDirection = GravityDirection * -1.0f;
		float CosAngle = FVector::Dot(FloorNormal, UpDirection);
		float WalkableCos = std::cos(DegreesToRadians(WalkableFloorAngle));

		// 0 (WalkableFloorAngle 경계) ~ 1 (90도 수직) 사이의 강도
		float SlopeStrength = 1.0f - (CosAngle / WalkableCos);
		SlopeStrength = FMath::Clamp(SlopeStrength, 0.0f, 1.0f);

		// 마찰력 적용 (마찰력이 높을수록 느리게 미끄러짐)
		float FrictionMultiplier = 1.0f - SlopeFriction;

		// 미끄러짐 속도 계산
		float SlideSpeed = SlopeSlideSpeed * SlopeStrength * FrictionMultiplier * DefaultGravity * GravityScale;

		// 속도에 미끄러짐 추가
		FVector SlideVelocity = SlideDirection * SlideSpeed;

		// 기존 수평 속도와 합산 (수평 성분만)
		FVector HorizontalSlide = SlideVelocity;
		float VerticalComponent = FVector::Dot(SlideVelocity, GravityDirection);
		HorizontalSlide -= GravityDirection * VerticalComponent;

		// 수평 속도에 추가
		FVector CurrentHorizontal = Velocity;
		float CurrentVertical = FVector::Dot(Velocity, GravityDirection);
		CurrentHorizontal -= GravityDirection * CurrentVertical;

		// 미끄러짐 방향으로 속도 추가
		Velocity = CurrentHorizontal + HorizontalSlide + GravityDirection * (CurrentVertical + VerticalComponent);

		// 이동 모드를 Falling으로 설정 (가파른 경사에서는 Walking 불가)
		if (MovementMode == EMovementMode::Walking)
		{
			SetMovementMode(EMovementMode::Falling);
		}
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
