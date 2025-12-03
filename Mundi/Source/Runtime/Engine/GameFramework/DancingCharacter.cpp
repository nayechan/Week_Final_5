#include "pch.h"
#include "DancingCharacter.h"
#include "SkeletalMeshComponent.h"
#include "AudioComponent.h"
#include "FAudioDevice.h"

ADancingCharacter::ADancingCharacter()
{
	if (MeshComponent)
	{
		MeshComponent->SetSkeletalMesh(GDataDir + "/SillyDancing.fbx");
		MeshComponent->PhysicsAsset = RESOURCE.Load<UPhysicsAsset>(GDataDir + "/PhysicsAssets/TestPhysicsAsset.physicsasset");
	}

	SorrySound = UResourceManager::GetInstance().Load<USound>(GDataDir + "/Audio/Die.wav");
	HitSound = UResourceManager::GetInstance().Load<USound>(GDataDir + "/Audio/SHC2.wav");
}

ADancingCharacter::~ADancingCharacter()
{
}

void ADancingCharacter::BeginPlay()
{
	Super::BeginPlay();

	UAnimSequence* AnimToPlay = UResourceManager::GetInstance().Get<UAnimSequence>(GDataDir + "/SillyDancing_mixamo.com");

	if (AnimToPlay && GetMesh())
	{
		MeshComponent->PlayAnimation(AnimToPlay, true);
		UE_LOG("ADancingCharacter: Started playing animation.");
	}
	else
	{
		UE_LOG("ADancingCharacter: Failedd to find animation to play");
	}
}

void ADancingCharacter::HandleAnimNotify(const FAnimNotifyEvent& NotifyEvent)
{
	Super::HandleAnimNotify(NotifyEvent);

	UE_LOG("ADancingCharacter: Notify Triggered!");

	FString NotifyName = NotifyEvent.NotifyName.ToString();

	if (NotifyName == "Sorry" && SorrySound)
	{
		FAudioDevice::PlaySound3D(SorrySound, GetActorLocation());
	}
	else if (NotifyName == "Hit" && HitSound)
	{
		FAudioDevice::PlaySound3D(HitSound, GetActorLocation());
	}
}