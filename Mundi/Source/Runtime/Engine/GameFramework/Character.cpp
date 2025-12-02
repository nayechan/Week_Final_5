// ────────────────────────────────────────────────────────────────────────────
// Character.cpp
// Character 클래스 구현
// ────────────────────────────────────────────────────────────────────────────
#include "pch.h"
#include "Character.h"
#include "CharacterMovementComponent.h"
#include "CapsuleComponent.h"
#include "SkeletalMeshComponent.h"
#include "SpringArmComponent.h"
#include "CameraComponent.h"
#include "InputComponent.h"
#include "World.h"
#include "PlayerCameraManager.h"

// ────────────────────────────────────────────────────────────────────────────
// 생성자 / 소멸자
// ────────────────────────────────────────────────────────────────────────────

ACharacter::ACharacter()
	: CharacterMovement(nullptr)
	, CapsuleComponent(nullptr)
	, MeshComponent(nullptr)
	, SpringArmComponent(nullptr)
	, CameraComponent(nullptr)
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

	// SpringArmComponent 생성 (3인칭 카메라용)
	SpringArmComponent = CreateDefaultSubobject<USpringArmComponent>("SpringArm");
	if (SpringArmComponent && CapsuleComponent)
	{
		SpringArmComponent->SetupAttachment(CapsuleComponent);
		SpringArmComponent->SetTargetArmLength(10.0f);
		SpringArmComponent->SetSocketOffset(FVector(0.0f, 0.0f, 0.0f));
		SpringArmComponent->SetEnableCameraLag(true);
		SpringArmComponent->SetCameraLagSpeed(10.0f);
		SpringArmComponent->SetUsePawnControlRotation(true);  // Controller 회전 사용
	}

	// CameraComponent 생성 (SpringArm 끝에 부착)
	CameraComponent = CreateDefaultSubobject<UCameraComponent>("Camera");
	if (CameraComponent && SpringArmComponent)
	{
		CameraComponent->SetupAttachment(SpringArmComponent);
	}
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

	// 카메라 위치 로그
	if (CameraComponent)
	{
		FVector CamPos = CameraComponent->GetWorldLocation();
		UE_LOG("[Character::Tick] Camera Position: (%.2f, %.2f, %.2f)", CamPos.X, CamPos.Y, CamPos.Z);
	}

	// 쿼터뷰 카메라 업데이트
	UpdateQuarterViewCamera(DeltaSeconds);
}

void ACharacter::UpdateQuarterViewCamera(float DeltaSeconds)
{
	// "캐릭터_0" 이름의 액터만 카메라가 따라감
	if (ObjectName != "캐릭터_0")
	{
		return;
	}

	// World 가져오기
	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG("NO World");
		return;
	}

	// World의 PlayerCameraManager 가져오기
	APlayerCameraManager* CameraManager = World->GetPlayerCameraManager();
	if (!CameraManager)
	{
		UE_LOG("NO CM");
		return;
	}

	// CameraComponent 가져오기
	UCameraComponent* CameraComponent = CameraManager->GetViewCamera();
	if (!CameraComponent)
	{
		UE_LOG("NO CC");
		return;
	}

	// 쿼터뷰 카메라 설정
	// 플레이어 뒤쪽 위에서 내려다보는 각도
	const FVector CameraOffset(-5.0f, 0.0f, 5.0f);  // 플레이어 기준 오프셋

	// 플레이어 위치 가져오기
	FVector PlayerLocation = GetActorLocation();

	// 목표 카메라 위치 계산 (플레이어 위치 + 오프셋)
	FVector TargetCameraLocation = PlayerLocation + CameraOffset;

	// 현재 카메라 위치
	FVector CurrentCameraLocation = CameraComponent->GetWorldLocation();

	// 부드럽게 보간 (Lerp)
	FVector NewCameraLocation = TargetCameraLocation;

	// 카메라 위치 설정
	CameraComponent->SetWorldLocation(NewCameraLocation);

	// 쿼터뷰 카메라 회전 설정 (고정된 각도)
	// Pitch: -45도 (위에서 아래로), Yaw: 0도 (정면), Roll: 0도
	FVector EulerAngles(0.0f, 45.0f, 0.0f);
	FQuat FixedRotation = FQuat::MakeFromEulerZYX(EulerAngles);

	// 카메라 회전 설정 (고정)
	CameraComponent->SetWorldRotation(FixedRotation);
}

void ACharacter::HandleAnimNotify(const FAnimNotifyEvent& NotifyEvent)
{
	Super::HandleAnimNotify(NotifyEvent);

	UE_LOG("[ACharacter] Received Anim Notify: %s", NotifyEvent.NotifyName.ToString().c_str());

	// Example
	//if (NotifyEvent.NotifyName == FName("Sorry"))
	//{
	//	// 죄송합니다 사운드 재생
	//}
}

// ────────────────────────────────────────────────────────────────────────────
// 입력 바인딩
// ────────────────────────────────────────────────────────────────────────────

void ACharacter::SetupPlayerInputComponent(UInputComponent* InInputComponent)
{
	Super::SetupPlayerInputComponent(InInputComponent);

	if (!InInputComponent)
	{
		return;
	}

	// WASD 이동 바인딩
	InInputComponent->BindAxis("MoveForward", 'W', 1.0f, this, &ACharacter::MoveForward);
	InInputComponent->BindAxis("MoveForward", 'S', -1.0f, this, &ACharacter::MoveForward);
	InInputComponent->BindAxis("MoveRight", 'D', 1.0f, this, &ACharacter::MoveRight);
	InInputComponent->BindAxis("MoveRight", 'A', -1.0f, this, &ACharacter::MoveRight);

	// 점프 바인딩
	InInputComponent->BindAction("Jump", VK_SPACE, this, &ACharacter::Jump, &ACharacter::StopJumping);
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
