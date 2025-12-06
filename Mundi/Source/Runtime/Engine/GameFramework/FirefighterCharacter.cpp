#include "pch.h"
#include "FirefighterCharacter.h"
#include "SkeletalMeshComponent.h"
#include "SpringArmComponent.h"
#include "CameraComponent.h"
#include "CapsuleComponent.h"
#include "AudioComponent.h"
#include "FAudioDevice.h"
#include "PlayerController.h"
#include "Controller.h"
#include "InputComponent.h"

AFirefighterCharacter::AFirefighterCharacter()
	: bOrientRotationToMovement(true)
	, RotationRate(540.0f)
	, CurrentMovementDirection(FVector::Zero())
{
	// 캡슐 크기 설정
	if (CapsuleComponent)
	{
		CapsuleComponent->SetCapsuleSize(0.25f, 1.0f);
	}

	// SkeletalMeshComponent 생성 (애니메이션)
	MeshComponent = CreateDefaultSubobject<USkeletalMeshComponent>("MeshComponent");
	if (MeshComponent && CapsuleComponent)
	{
		MeshComponent->SetupAttachment(CapsuleComponent);
		MeshComponent->SetRelativeLocation(FVector(0.0f, 0.0f, -1.25f));
		MeshComponent->SetSkeletalMesh(GDataDir + "/X Bot.fbx");
	}

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

AFirefighterCharacter::~AFirefighterCharacter()
{
}

void AFirefighterCharacter::BeginPlay()
{
	Super::BeginPlay();

	UAnimSequence* AnimToPlay = UResourceManager::GetInstance().Get<UAnimSequence>(GDataDir + "/Animation/BSH_Walk Forward_mixamo.com");

	if (AnimToPlay && GetMesh())
	{
		MeshComponent->PlayAnimation(AnimToPlay, true);
		UE_LOG("AFirefighterCharacter: Started playing animation.");
	}
	else
	{
		UE_LOG("AFirefighterCharacter: Failed to find animation to play");
	}
}

void AFirefighterCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	// 게임 전용 입력 모드 설정 (커서 숨김, 마우스 잠금, 항상 카메라 회전)
	if (APlayerController* PC = Cast<APlayerController>(NewController))
	{
		PC->SetInputMode(EInputMode::GameOnly);
	}
}

void AFirefighterCharacter::HandleAnimNotify(const FAnimNotifyEvent& NotifyEvent)
{
	Super::HandleAnimNotify(NotifyEvent);

	//UE_LOG("AFirefighterCharacter: Notify Triggered!");
	//
	//FString NotifyName = NotifyEvent.NotifyName.ToString();
	//
	//if (NotifyName == "Sorry" && SorrySound)
	//{
	//	FAudioDevice::PlaySound3D(SorrySound, GetActorLocation());
	//}
	//else if (NotifyName == "Hit" && HitSound)
	//{
	//	FAudioDevice::PlaySound3D(HitSound, GetActorLocation());
	//}
}

void AFirefighterCharacter::Tick(float DeltaSeconds)
{
	// 이동 방향으로 캐릭터 회전 (Super::Tick 전에 처리해야 SpringArm이 올바른 회전을 사용)
	if (bOrientRotationToMovement && CurrentMovementDirection.SizeSquared() > 0.01f)
	{
		// 이동 방향에서 목표 회전 계산 (Yaw만 사용)
		FVector Direction = CurrentMovementDirection.GetNormalized();
		float TargetYaw = std::atan2(Direction.Y, Direction.X);  // 라디안

		// 현재 Actor 회전에서 Yaw 추출
		FVector CurrentEuler = GetActorRotation().ToEulerZYXDeg();  // (Roll, Pitch, Yaw) in degrees
		float CurrentYaw = CurrentEuler.Z * (PI / 180.0f);  // 라디안으로 변환

		// 각도 차이 계산 (최단 경로)
		float DeltaYaw = TargetYaw - CurrentYaw;

		// -PI ~ PI 범위로 정규화
		while (DeltaYaw > PI) DeltaYaw -= 2.0f * PI;
		while (DeltaYaw < -PI) DeltaYaw += 2.0f * PI;

		// 회전 속도 제한 적용
		float MaxDelta = RotationRate * (PI / 180.0f) * DeltaSeconds;  // 라디안/초
		DeltaYaw = FMath::Clamp(DeltaYaw, -MaxDelta, MaxDelta);

		// 새로운 Yaw 계산
		float NewYaw = CurrentYaw + DeltaYaw;

		// Actor 회전 설정 (Yaw만 변경)
		FQuat NewRotation = FQuat::MakeFromEulerZYX(FVector(0.0f, 0.0f, NewYaw * (180.0f / PI)));
		SetActorRotation(NewRotation);
	}

	// 이동 방향 초기화 (다음 프레임을 위해)
	CurrentMovementDirection = FVector::Zero();

	// Super::Tick에서 Component들의 Tick이 호출됨 (SpringArm 포함)
	Super::Tick(DeltaSeconds);
}

void AFirefighterCharacter::SetupPlayerInputComponent(UInputComponent* InInputComponent)
{
	if (!InInputComponent)
	{
		return;
	}

	// WASD 이동 바인딩 (카메라 방향 기준)
	InInputComponent->BindAxis("MoveForward", 'W', 1.0f, this, &AFirefighterCharacter::MoveForwardCamera);
	InInputComponent->BindAxis("MoveForward", 'S', -1.0f, this, &AFirefighterCharacter::MoveForwardCamera);
	InInputComponent->BindAxis("MoveRight", 'D', 1.0f, this, &AFirefighterCharacter::MoveRightCamera);
	InInputComponent->BindAxis("MoveRight", 'A', -1.0f, this, &AFirefighterCharacter::MoveRightCamera);

	// 점프 바인딩 (부모 클래스 함수 사용)
	InInputComponent->BindAction("Jump", VK_SPACE, Cast<ACharacter>(this), &ACharacter::Jump, &ACharacter::StopJumping);
}

void AFirefighterCharacter::MoveForwardCamera(float Value)
{
	if (Value == 0.0f)
	{
		return;
	}

	AController* MyController = GetController();
	if (!MyController)
	{
		return;
	}

	// Controller의 Yaw만 사용하여 Forward 방향 계산
	FQuat ControlRot = MyController->GetControlRotation();
	FVector ControlEuler = ControlRot.ToEulerZYXDeg();  // (Roll, Pitch, Yaw) in degrees

	// Yaw만으로 회전 생성
	FQuat YawOnlyRot = FQuat::MakeFromEulerZYX(FVector(0.0f, 0.0f, ControlEuler.Z));
	FVector Forward = YawOnlyRot.GetForwardVector();

	// 이동 방향 누적 (회전 계산용)
	CurrentMovementDirection += Forward * Value;

	// 이동 입력 추가
	AddMovementInput(Forward, Value);
}

void AFirefighterCharacter::MoveRightCamera(float Value)
{
	if (Value == 0.0f)
	{
		return;
	}

	AController* MyController = GetController();
	if (!MyController)
	{
		return;
	}

	// Controller의 Yaw만 사용하여 Right 방향 계산
	FQuat ControlRot = MyController->GetControlRotation();
	FVector ControlEuler = ControlRot.ToEulerZYXDeg();  // (Roll, Pitch, Yaw) in degrees

	// Yaw만으로 회전 생성
	FQuat YawOnlyRot = FQuat::MakeFromEulerZYX(FVector(0.0f, 0.0f, ControlEuler.Z));
	FVector Right = YawOnlyRot.GetRightVector();

	// 이동 방향 누적 (회전 계산용)
	CurrentMovementDirection += Right * Value;

	// 이동 입력 추가
	AddMovementInput(Right, Value);
}