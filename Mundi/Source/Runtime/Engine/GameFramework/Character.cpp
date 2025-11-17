// ────────────────────────────────────────────────────────────────────────────
// Character.cpp
// Character 클래스 구현
// ────────────────────────────────────────────────────────────────────────────
#include "pch.h"
#include "Character.h"
#include "CharacterMovementComponent.h"
#include "CapsuleComponent.h"
#include "SkeletalMeshComponent.h"
#include "InputComponent.h"

// ────────────────────────────────────────────────────────────────────────────
// 생성자 / 소멸자
// ────────────────────────────────────────────────────────────────────────────

ACharacter::ACharacter()
	: CharacterMovement(nullptr)
	, CapsuleComponent(nullptr)
	, MeshComponent(nullptr)
	, bIsCrouched(false)
	, CrouchedHeightRatio(0.5f)
{
	// CapsuleComponent 생성 (RootComponent, 충돌 및 물리)
	CapsuleComponent = CreateDefaultSubobject<UCapsuleComponent>("CapsuleComponent");
	if (CapsuleComponent)
	{
		SetRootComponent(CapsuleComponent);
	}

	// SkeletalMeshComponent 생성 (애니메이션)
	MeshComponent = CreateDefaultSubobject<USkeletalMeshComponent>("MeshComponent");
	if (MeshComponent && CapsuleComponent)
	{
		MeshComponent->SetupAttachment(CapsuleComponent);
	}

	// CharacterMovementComponent 생성
	CharacterMovement = CreateDefaultSubobject<UCharacterMovementComponent>("CharacterMovement");
}

ACharacter::~ACharacter()
{
}

// ────────────────────────────────────────────────────────────────────────────
// 생명주기
// ────────────────────────────────────────────────────────────────────────────

void ACharacter::BeginPlay()
{
	Super::BeginPlay();
}

void ACharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
}

// ────────────────────────────────────────────────────────────────────────────
// 입력 바인딩
// ────────────────────────────────────────────────────────────────────────────

void ACharacter::SetupPlayerInputComponent(UInputComponent* InInputComponent)
{
	Super::SetupPlayerInputComponent(InInputComponent);

	// 입력 바인딩은 파생 클래스 또는 Lua 스크립트에서 처리
}

// ────────────────────────────────────────────────────────────────────────────
// 이동 입력 처리
// ────────────────────────────────────────────────────────────────────────────

void ACharacter::AddMovementInput(FVector WorldDirection, float ScaleValue)
{
	if (CharacterMovement)
	{
		CharacterMovement->AddInputVector(WorldDirection, ScaleValue);
	}
}

void ACharacter::MoveForward(float Value)
{
	if (Value != 0.0f)
	{
		// Forward 방향으로 이동
		FVector Forward = GetActorForward();
		AddMovementInput(Forward, Value);
	}
}

void ACharacter::MoveRight(float Value)
{
	if (Value != 0.0f)
	{
		// Right 방향으로 이동
		FVector Right = GetActorRight();
		AddMovementInput(Right, Value);
	}
}

void ACharacter::Turn(float Value)
{
	if (Value != 0.0f)
	{
		AddControllerYawInput(Value);
	}
}

void ACharacter::LookUp(float Value)
{
	if (Value != 0.0f)
	{
		AddControllerPitchInput(Value);
	}
}

// ────────────────────────────────────────────────────────────────────────────
// Character 동작
// ────────────────────────────────────────────────────────────────────────────

void ACharacter::Jump()
{
	if (CharacterMovement)
	{
		CharacterMovement->Jump();
	}
}

void ACharacter::StopJumping()
{
	if (CharacterMovement)
	{
		CharacterMovement->StopJumping();
	}
}

bool ACharacter::CanJump() const
{
	return CharacterMovement && CharacterMovement->bCanJump && IsGrounded();
}

void ACharacter::Crouch()
{
	if (bIsCrouched)
	{
		return;
	}

	bIsCrouched = true;

	// 웅크릴 때 이동 속도 감소 (옵션)
	if (CharacterMovement)
	{
		CharacterMovement->MaxWalkSpeed *= CrouchedHeightRatio;
	}
}

void ACharacter::UnCrouch()
{
	if (!bIsCrouched)
	{
		return;
	}

	bIsCrouched = false;

	// 이동 속도 복원
	if (CharacterMovement)
	{
		CharacterMovement->MaxWalkSpeed /= CrouchedHeightRatio;
	}
}

// ────────────────────────────────────────────────────────────────────────────
// 상태 조회
// ────────────────────────────────────────────────────────────────────────────

FVector ACharacter::GetVelocity() const
{
	if (CharacterMovement)
	{
		return CharacterMovement->GetVelocity();
	}

	return FVector();
}

bool ACharacter::IsGrounded() const
{
	return CharacterMovement && CharacterMovement->IsGrounded();
}

bool ACharacter::IsFalling() const
{
	return CharacterMovement && CharacterMovement->IsFalling();
}

// ────────────────────────────────────────────────────────────────────────────
// 복제
// ────────────────────────────────────────────────────────────────────────────

void ACharacter::DuplicateSubObjects()
{
	Super::DuplicateSubObjects();

	// 컴포넌트 참조 재설정
	for (UActorComponent* Component : OwnedComponents)
	{
		if (UCharacterMovementComponent* Movement = Cast<UCharacterMovementComponent>(Component))
		{
			CharacterMovement = Movement;
		}
		else if (USkeletalMeshComponent* SkeletalMesh = Cast<USkeletalMeshComponent>(Component))
		{
			MeshComponent = SkeletalMesh;
		}
	}

	// CapsuleComponent는 RootComponent
	CapsuleComponent = Cast<UCapsuleComponent>(RootComponent);
}

void ACharacter::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	Super::Serialize(bInIsLoading, InOutHandle);

	// TODO: 필요한 멤버 변수 직렬화 추가
}
