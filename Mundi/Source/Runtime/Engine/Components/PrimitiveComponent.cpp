#include "pch.h"
#include "PrimitiveComponent.h"
#include "SceneComponent.h"
#include "Actor.h"
#include "WorldPartitionManager.h"
// IMPLEMENT_CLASS is now auto-generated in .generated.cpp
UPrimitiveComponent::UPrimitiveComponent()
    : bGenerateOverlapEvents(true)
    , bSimulatePhysics(false)
{
}

UPrimitiveComponent::~UPrimitiveComponent()
{
    // @todo OnDestroyPhysicsState는 가상 함수이므로 소멸자에서 호출 불가능
    //       OnUnregister가 항상 PrimitiveComponent에 소멸 이전의 호출되는지 확인 필요함
}

void UPrimitiveComponent::OnPropertyChanged(const FProperty& Prop)
{
    USceneComponent::OnPropertyChanged(Prop);

    if (Prop.Name == "bSimulatePhysics" || Prop.Name == "PhysicalMaterial")
    {
        if (BodyInstance.IsValidBodyInstance())
        {
            OnDestroyPhysicsState();
            OnCreatePhysicsState();
        }
    }
}

void UPrimitiveComponent::OnRegister(UWorld* InWorld)
{
    Super::OnRegister(InWorld);

    // UStaticMeshComponent라면 World Partition에 추가. (null 체크는 Register 내부에서 수행)
    if (InWorld)
    {
        if (UWorldPartitionManager* Partition = InWorld->GetPartitionManager())
        {
            Partition->Register(this);
        }

        if (InWorld->GetPhysicsScene())
        {
            OnCreatePhysicsState();
        }
    }
}

void UPrimitiveComponent::OnUnregister()
{
    OnDestroyPhysicsState();
    
    if (UWorld* World = GetWorld())
    {
        if (UWorldPartitionManager* Partition = World->GetPartitionManager())
        {
            Partition->Unregister(this);
        }
    }

    Super::OnUnregister();
}

void UPrimitiveComponent::OnUpdateTransform(EUpdateTransformFlags UpdateTransformFlags, ETeleportType Teleport)
{
    USceneComponent::OnUpdateTransform(UpdateTransformFlags, Teleport);

    if (BodyInstance.IsValidBodyInstance() &&
        !((int32)UpdateTransformFlags & (int32)EUpdateTransformFlags::SkipPhysicsUpdate))
    {
        bool bIsTeleport = Teleport != ETeleportType::None;
        BodyInstance.SetBodyTransform(GetWorldTransform(), bIsTeleport);
    }
}

void UPrimitiveComponent::SetMaterialByName(uint32 InElementIndex, const FString& InMaterialName)
{
    SetMaterial(InElementIndex, UResourceManager::GetInstance().Load<UMaterial>(InMaterialName));
}

void UPrimitiveComponent::OnCreatePhysicsState()
{
    if (BodyInstance.IsValidBodyInstance())
    {
        return;
    }

    UBodySetup* Setup = GetBodySetup();
    if (!Setup)
    {
        return;
    }

    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    FPhysScene* PhysScene = World->GetPhysicsScene();
    if (!PhysScene)
    {
        return;
    }

    BodyInstance.PhysicalMaterialOverride = PhysicalMaterial;
    BodyInstance.bSimulatePhysics = bSimulatePhysics;
    BodyInstance.Scale3D = GetRelativeScale();

    BodyInstance.InitBody(Setup, GetWorldTransform(), this, PhysScene);
}

void UPrimitiveComponent::OnDestroyPhysicsState()
{
    if (BodyInstance.IsValidBodyInstance())
    {
        BodyInstance.TermBody();
    }
}

void UPrimitiveComponent::DuplicateSubObjects()
{
    Super::DuplicateSubObjects();
}

void UPrimitiveComponent::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
    Super::Serialize(bInIsLoading, InOutHandle);
}

void UPrimitiveComponent::SetSimulatePhysics(bool bInSimulatePhysics)
{
    if (bSimulatePhysics != bInSimulatePhysics)
    {
        bSimulatePhysics = bInSimulatePhysics;

        if (BodyInstance.IsValidBodyInstance())
        {
            OnDestroyPhysicsState();
            OnCreatePhysicsState();
        }
    }
}

bool UPrimitiveComponent::IsOverlappingActor(const AActor* Other) const
{
    if (!Other)
    {
        return false;
    }

    const TArray<FOverlapInfo>& Infos = GetOverlapInfos();
    for (const FOverlapInfo& Info : Infos)
    {
        if (Info.Other)
        {
            if (AActor* Owner = Info.Other->GetOwner())
            {
                if (Owner == Other)
                {
                    return true;
                }
            }
        }
    }
    return false;
}
