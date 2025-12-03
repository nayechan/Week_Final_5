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

    // 물리 관련 프로퍼티 변경 시 물리 재생성
    // (Scale 변경은 OnUpdateTransform에서 처리됨 - 자식 컴포넌트 전파 포함)
    if (Prop.Name == "bSimulatePhysics" || Prop.Name == "PhysicalMaterial" || Prop.Name == "CollisionEnabled" || Prop.Name == "BodySetup")
    {
        OnDestroyPhysicsState();
        OnCreatePhysicsState();
    }

    // 충돌 채널 변경 시 BodyInstance에 동기화
    if (Prop.Name == "CollisionChannel" || Prop.Name == "CollisionMask")
    {
        BodyInstance.SetCollisionChannel(CollisionChannel, CollisionMask);
    }
}

void UPrimitiveComponent::OnRegister(UWorld* InWorld)
{
    Super::OnRegister(InWorld);

    OnComponentHit.AddDynamic(this, &UPrimitiveComponent::OnHitDebug);
    
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
        // World Scale이 변경되었으면 물리 재생성 필요 (Shape geometry에 Scale이 베이크되어 있음)
        FVector CurrentWorldScale = GetWorldScale();
        if ((CurrentWorldScale - BodyInstance.Scale3D).SizeSquared() > KINDA_SMALL_NUMBER)
        {
            OnDestroyPhysicsState();
            OnCreatePhysicsState();
        }
        else
        {
            bool bIsTeleport = Teleport != ETeleportType::None;
            BodyInstance.SetBodyTransform(GetWorldTransform(), bIsTeleport);
        }
    }
}

void UPrimitiveComponent::SetMaterialByName(uint32 InElementIndex, const FString& InMaterialName)
{
    SetMaterial(InElementIndex, UResourceManager::GetInstance().Load<UMaterial>(InMaterialName));
}

void UPrimitiveComponent::OnCreatePhysicsState()
{
    // NoCollision이면 물리 상태 생성하지 않음
    if (CollisionEnabled == ECollisionEnabled::NoCollision)
    {
        return;
    }

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

    // 충돌 채널 설정 (InitBody 전에 설정해야 FilterData가 적용됨)
    BodyInstance.ObjectType = CollisionChannel;
    BodyInstance.CollisionMask = CollisionMask;

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

        OnDestroyPhysicsState();
        OnCreatePhysicsState();
    }
}

void UPrimitiveComponent::SetCollisionEnabled(ECollisionEnabled InCollisionEnabled)
{
    if (CollisionEnabled != InCollisionEnabled)
    {
        CollisionEnabled = InCollisionEnabled;

        OnDestroyPhysicsState();
        OnCreatePhysicsState();
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

void UPrimitiveComponent::DispatchBlockingHit(AActor* OtherActor, UPrimitiveComponent* OtherComp, const FVector& NormalImpulse, const FHitResult& Hit)
{
    OnComponentHit.Broadcast(this, OtherActor, OtherComp, NormalImpulse, Hit);
}

void UPrimitiveComponent::DispatchWakeEvents(bool bWake, FName BoneName)
{
    if (bWake)
    {
        OnComponentWake.Broadcast(this, BoneName);
    }
    else
    {
        OnComponentSleep.Broadcast(this, BoneName); 
    }
}

void UPrimitiveComponent::DispatchBeginOverlap(AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
    OnComponentBeginOverlap.Broadcast(this, OtherActor, OtherComp, OtherBodyIndex);
}

void UPrimitiveComponent::DispatchEndOverlap(AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
    OnComponentEndOverlap.Broadcast(this, OtherActor, OtherComp, OtherBodyIndex);
}
