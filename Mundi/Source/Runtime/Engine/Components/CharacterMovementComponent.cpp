// ────────────────────────────────────────────────────────────────────────────
// CharacterMovementComponent.cpp
// Character 이동 컴포넌트 구현
// ────────────────────────────────────────────────────────────────────────────
#include "pch.h"
#include "CharacterMovementComponent.h"
#include "Character.h"
#include "SceneComponent.h"

// ────────────────────────────────────────────────────────────────────────────
// 생성자 / 소멸자
// ────────────────────────────────────────────────────────────────────────────

UCharacterMovementComponent::UCharacterMovementComponent()
	: CharacterOwner(nullptr)
	, PendingInputVector(FVector())
	, MovementMode(EMovementMode::Falling)
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
	, GravityDirection(0.0f, 0.0f, -1.0f) // 기본값: 아래 방향
	// 점프 설정
	, JumpZVelocity(4.200f)
	, MaxAirTime(5.0f)
	, bCanJump(true)
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

	// Owner를 Character로 캐스팅
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
		// 착지 - 중력 방향의 속도 성분 제거
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

	// 중력 방향에 수직인 평면으로 입력 제한
	// 입력 벡터에서 중력 방향 성분을 제거 (투영 후 빼기)
	float DotWithGravity = FVector::Dot(WorldDirection, GravityDirection);
	FVector HorizontalDirection = WorldDirection - (GravityDirection * DotWithGravity);

	// 방향 벡터가 0이면 무시
	if (HorizontalDirection.SizeSquared() < 0.0001f)
	{
		return;
	}

	FVector NormalizedDirection = HorizontalDirection.GetNormalized();
	PendingInputVector += NormalizedDirection * ScaleValue;
}

bool UCharacterMovementComponent::Jump()
{
	// 점프 가능 조건 체크
	if (!bCanJump || !IsGrounded())
	{
		return false;
	}

	// 중력 반대 방향으로 점프 속도 적용
	FVector JumpVelocity = GravityDirection * -1.0f * JumpZVelocity;
	Velocity += JumpVelocity;

	// 이동 모드 변경
	SetMovementMode(EMovementMode::Falling);
	bIsJumping = true;

	return true;
}

void UCharacterMovementComponent::StopJumping()
{
	// 점프 키를 뗐을 때 상승 속도를 줄임
	// 중력 반대 방향으로의 속도만 감소
	FVector UpDirection = GravityDirection * -1.0f;
	float UpwardSpeed = FVector::Dot(Velocity, UpDirection);

	if (bIsJumping && UpwardSpeed > 0.0f)
	{
		// 상승 속도 성분만 감소
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
	// 입력 정규화
	FVector InputVector = PendingInputVector;
	if (InputVector.SizeSquared() > 1.0f)
	{
		InputVector = InputVector.GetNormalized();
	}

	// 입력이 있으면 가속
	if (InputVector.SizeSquared() > 0.0001f)
	{
		// 가속도 계산
		FVector TargetVelocity = InputVector * MaxWalkSpeed;
		FVector CurrentHorizontalVelocity = Velocity;

		// 중력 방향 속도 성분 제거
		float VerticalSpeed = FVector::Dot(Velocity, GravityDirection);
		CurrentHorizontalVelocity -= GravityDirection * VerticalSpeed;

		// 목표 속도와의 차이 계산
		FVector VelocityDifference = TargetVelocity - CurrentHorizontalVelocity;

		// 가속도 적용 (지면에서는 최대 제어력, 공중에서는 AirControl)
		float ControlPower = IsGrounded() ? 1.0f : AirControl;
		FVector Accel = VelocityDifference.GetNormalized() * MaxAcceleration * ControlPower * DeltaTime;

		// 속도 적용 (최대 속도 제한)
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
		// 입력이 없으면 마찰 적용 (지면에서만)
		FVector HorizontalVelocity = Velocity;
		float VerticalSpeed = FVector::Dot(Velocity, GravityDirection);
		HorizontalVelocity -= GravityDirection * VerticalSpeed;

		float Speed = HorizontalVelocity.Size();
		if (Speed > 0.001f)
		{
			// 마찰력 계산
			float Deceleration = GroundFriction * MaxWalkSpeed * DeltaTime;
			float NewSpeed = FMath::Max(0.0f, Speed - Deceleration);
			HorizontalVelocity = HorizontalVelocity.GetNormalized() * NewSpeed;

			// 수평 속도만 업데이트 (수직 속도 유지)
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
	// 지면에 있으면 중력 적용 안 함
	if (IsGrounded())
	{
		return;
	}

	// 중력 가속도 적용
	FVector GravityAccel = GravityDirection * DefaultGravity * GravityScale * DeltaTime;
	Velocity += GravityAccel;

	// 최대 낙하 속도 제한 (40 m/s = 4000 cm/s)
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

	// 새 위치 계산
	FVector Delta = Velocity * DeltaTime;
	FVector NewLocation = UpdatedComponent->GetWorldLocation() + Delta;

	// 위치 업데이트
	UpdatedComponent->SetWorldLocation(NewLocation);
}

bool UCharacterMovementComponent::CheckGround()
{
	if (!UpdatedComponent)
	{
		return false;
	}

	// 간단한 지면 체크: Z 위치가 0 이하면 지면
	FVector Location = UpdatedComponent->GetWorldLocation();

	if (Location.Z <= 0.0f)
	{
		// 지면에 스냅
		Location.Z = 0.0f;
		UpdatedComponent->SetWorldLocation(Location);
		return true;
	}

	return false;
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

	// TODO: 필요한 멤버 변수 직렬화 추가
}
