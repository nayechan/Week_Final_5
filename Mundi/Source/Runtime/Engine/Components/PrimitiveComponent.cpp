#include "pch.h"
#include "PrimitiveComponent.h"
#include "SceneComponent.h"
#include "Actor.h"
#include "WorldPartitionManager.h"
// IMPLEMENT_CLASS is now auto-generated in .generated.cpp
UPrimitiveComponent::UPrimitiveComponent() : bGenerateOverlapEvents(true)
{
}

void UPrimitiveComponent::BeginPlay()
{
    Super::BeginPlay();
    CreatePhysicsState();
}

void UPrimitiveComponent::EndPlay()
{
    Super::EndPlay();
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
    }
}

void UPrimitiveComponent::OnUnregister()
{
    if (UWorld* World = GetWorld())
    {
        if (UWorldPartitionManager* Partition = World->GetPartitionManager())
        {
            Partition->Unregister(this);
        }
    }

    Super::OnUnregister();
}

void UPrimitiveComponent::SetMaterialByName(uint32 InElementIndex, const FString& InMaterialName)
{
    SetMaterial(InElementIndex, UResourceManager::GetInstance().Load<UMaterial>(InMaterialName));
} 
 
void UPrimitiveComponent::DuplicateSubObjects()
{
    Super::DuplicateSubObjects();
}

void UPrimitiveComponent::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
    Super::Serialize(bInIsLoading, InOutHandle);
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

void UPrimitiveComponent::SyncComponentToPhysics()
{
    // 물리가 켜져 있고, 바디 인스턴스가 유효한 경우에만 동기화
    if (BodyInstance.IsValidBodyInstance() && IsSimulatingPhysics())
    {
        // 1. 물리 엔진에서 계산된 최신 Transform을 가져옴
        FTransform NewTransform = BodyInstance.GetWorldTransform();

        // 2. 컴포넌트의 월드 트랜스폼 업데이트
        SetWorldTransform(NewTransform);
    }
}

void UPrimitiveComponent::CreatePhysicsState()
{
    FPhysicsScene* PhysScene = GetWorld() ? GetWorld()->GetPhysicsScene() : nullptr;
    BodyInstance.InitBody(GetBodySetup(), GetWorldTransform(), this, PhysScene);
}
