#include "pch.h"
#include "ShapeComponent.h"
#include "OBB.h"
#include "Collision.h"
#include "World.h"
#include "WorldPartitionManager.h"
#include "BVHierarchy.h"
#include "GameObject.h"
// IMPLEMENT_CLASS is now auto-generated in .generated.cpp
UShapeComponent::UShapeComponent() : bShapeIsVisible(true), bShapeHiddenInGame(true)
{
    ShapeColor = FVector4(0.2f, 0.8f, 1.0f, 1.0f); 
    bCanEverTick = true;
}

void UShapeComponent::BeginPlay()
{
    Super::BeginPlay();
}

void UShapeComponent::OnRegister(UWorld* InWorld)
{
    Super::OnRegister(InWorld);
    
    GetWorldAABB();
}

void UShapeComponent::OnTransformUpdated()
{
    GetWorldAABB();

    // Keep BVH up-to-date for broad phase queries
    if (UWorld* World = GetWorld())
    {
        if (UWorldPartitionManager* Partition = World->GetPartitionManager())
        {
            Partition->MarkDirty(this);
        }
    }

    //UpdateOverlaps();
    Super::OnTransformUpdated();
}

void UShapeComponent::TickComponent(float DeltaSeconds)
{
    if (GetClass() == UShapeComponent::StaticClass())
    {
        bGenerateOverlapEvents = false;
    }

    if (!bGenerateOverlapEvents)
    {
        OverlapInfos.clear();
    }

    UWorld* World = GetWorld();
    if (!World) return;

    //Test용 O(N^2) 
    OverlapNow.clear();

    for (AActor* Actor : World->GetActors())
    {
        if (!Actor || !Actor->IsActorActive())
            continue;

        for (USceneComponent* Comp : Actor->GetSceneComponents())
        {
            UShapeComponent* Other = Cast<UShapeComponent>(Comp);
            if (!Other || Other == this) continue;
            if (Other->GetOwner() == this->GetOwner()) continue;
            if (!Other->bGenerateOverlapEvents) continue;

            AActor* Owner = this->GetOwner();
            AActor* OtherOwner = Other->GetOwner();

            // Collision 모듈
            if (!Collision::CheckOverlap(this, Other)) continue;

            OverlapNow.Add(Other);
            //UE_LOG("Collision!!");
        }
    }

    // Publish current overlaps
    OverlapInfos.clear();
    for (UShapeComponent* Other : OverlapNow)
    {
        FOverlapInfo Info;
        Info.OtherActor = Other->GetOwner();
        Info.Other = Other;
        OverlapInfos.Add(Info);
    }

    //Begin
    for (UShapeComponent* Comp : OverlapNow)
    {
        if (!Comp || Comp->IsPendingDestroy())
        {
            continue;
        }

        if (!OverlapPrev.Contains(Comp))
        {
            AActor* Owner = this->GetOwner();
            AActor* OtherOwner = Comp ? Comp->GetOwner() : nullptr;

            // 이전에 호출된 적이 있는 이벤트 인지 확인
            if (!(Owner && OtherOwner && World->TryMarkOverlapPair(Owner, OtherOwner)))
            {
                continue;
            }

            // 양방향 호출 
            Owner->OnComponentBeginOverlap.Broadcast(this, Comp);
            if (AActor* OtherOwner = Comp->GetOwner())
            {
                OtherOwner->OnComponentBeginOverlap.Broadcast(Comp, this);
            }

            // Hit호출 
            Owner->OnComponentHit.Broadcast(this, Comp);
            if (bBlockComponent)
            {
                if (AActor* OtherOwner = Comp->GetOwner())
                {
                    OtherOwner->OnComponentHit.Broadcast(Comp, this);
                }
            }
        }
    }

    //End
    for (UShapeComponent* Comp : OverlapPrev)
    {
        if (!Comp || Comp->IsPendingDestroy())
        {
            continue;
        }

        if (!OverlapNow.Contains(Comp))
        {
            AActor* Owner = this->GetOwner();
            AActor* OtherOwner = Comp ? Comp->GetOwner() : nullptr;

            // 이전에 호출된 적이 있는 이벤트 인지 확인
            if (!(Owner && OtherOwner && World->TryMarkOverlapPair(Owner, OtherOwner)))
            {
                continue;
            }

            // 양방향 호출
            Owner->OnComponentEndOverlap.Broadcast(this, Comp);
            if (AActor* OtherOwner = Comp->GetOwner())
            {
                OtherOwner->OnComponentEndOverlap.Broadcast(Comp, this);
            }
        }
    }

    OverlapPrev.clear();
    for (UShapeComponent* Comp : OverlapNow)
    {
            if (Comp && !Comp->IsPendingDestroy())
            {
                OverlapPrev.Add(Comp);
            }
    }
}

FAABB UShapeComponent::GetWorldAABB() const
{
    if (AActor* Owner = GetOwner())
    {
        FAABB OwnerBounds = Owner->GetBounds();
        const FVector HalfExtent = OwnerBounds.GetHalfExtent();
        WorldAABB = OwnerBounds;
    }
    return WorldAABB;
}
  
void UShapeComponent::DuplicateSubObjects()
{
    Super::DuplicateSubObjects();
}




