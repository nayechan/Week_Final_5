#include "pch.h"
#include "FirefighterCharacter.h"
#include "SkeletalMeshComponent.h"
#include "SpringArmComponent.h"
#include "CameraComponent.h"
#include "CapsuleComponent.h"
#include "AudioComponent.h"
#include "FAudioDevice.h"
#include "PlayerController.h"

AFirefighterCharacter::AFirefighterCharacter()
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