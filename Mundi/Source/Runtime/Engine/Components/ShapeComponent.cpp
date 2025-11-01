#include "pch.h"
#include "ShapeComponent.h"
#include "OBB.h"
#include "Collision.h"
#include "World.h"

IMPLEMENT_CLASS(UShapeComponent)

BEGIN_PROPERTIES(UShapeComponent)
    ADD_PROPERTY(bool, bShapeIsVisible, "Shape", true, "Shape를 가시화 변수입니다.")
    ADD_PROPERTY(bool, bShapeHiddenInGame, "Shape", true, "Shape를 PIE 모드에서의 가시화 변수입니다.")
 END_PROPERTIES()

UShapeComponent::UShapeComponent() : bShapeIsVisible(true), bShapeHiddenInGame(true)
{
    ShapeColor = FVector4(0.2f, 0.8f, 1.0f, 1.0f); 
}

void UShapeComponent::OnRegister(UWorld* InWorld)
{
    Super::OnRegister(InWorld);
    
    GetWorldAABB();
}

void UShapeComponent::OnTransformUpdated()
{
    GetWorldAABB();
    UpdateOverlaps();
    Super::OnTransformUpdated();
}

void UShapeComponent::UpdateOverlaps()
{
    // 모양이 없는 UShapeComponent 자체는 충돌 검사를 수행할 수 없도록 함
    if (GetClass() == UShapeComponent::StaticClass())
    {
        bGenerateOverlapEvents = false;
    }

    if (!bGenerateOverlapEvents)
    {
        OverlapInfos.clear();
        return;
    }
    
    UWorld* World = GetWorld();
    if (!World) return; 
    
    TSet<UShapeComponent*> Now;

    //Test용 O(N^2)
    for (AActor* Actor : World->GetActors())
    {
        for (USceneComponent* Comp : Actor->GetSceneComponents())
        {
            UShapeComponent* Other = Cast<UShapeComponent>(Comp);
            if (!Other || Other == this) continue;
            if (Other->GetOwner() == this->GetOwner()) continue;
            if (!Other->bGenerateOverlapEvents) continue;
 
            // 내로우페이즈: Collision 모듈
            if (!Collision::CheckOverlap(this, Other)) continue;

            Now.Add(Other);
            UE_LOG("Collision!!");
        }
    } 

    // Publish current overlaps
    OverlapInfos.clear();
    for (UShapeComponent* Other : Now)
    {
        FOverlapInfo Info;
        Info.OtherActor = Other->GetOwner();
        Info.Other = Other;
        OverlapInfos.Add(Info);
    } 

    //Begin
    for(UShapeComponent* Comp : Now) 
        OnComponentBeginOverlap.Broadcast(this, Comp);

    //for( UShapeComponent* Comp : Prev)
    //OnComponentEndOverlap.Broadcast(this, Comp);
     
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