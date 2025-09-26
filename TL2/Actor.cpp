#include "pch.h"
#include "Actor.h"
#include "SceneComponent.h"
#include "ObjectFactory.h"
#include "ShapeComponent.h"
#include "AABoundingBoxComponent.h"   
#include "MeshComponent.h"
#include "TextRenderComponent.h"
#include "WorldPartitionManager.h"

#include "World.h"
AActor::AActor()
{
    Name = "DefaultActor";
    RootComponent= CreateDefaultSubobject<USceneComponent>(FName("SceneComponent"));
    CollisionComponent = CreateDefaultSubobject<UAABoundingBoxComponent>(FName("CollisionBox"));
    UTextRenderComponent* TextComp = NewObject<UTextRenderComponent>();
    TextComp->SetOwner(this);
    AddComponent(TextComp);    
}

AActor::~AActor()
{
    //// 1) Delete root: cascades to attached children
    //if (RootComponent)
    //{
    //    ObjectFactory::DeleteObject(RootComponent);
    //    RootComponent = nullptr;
    //}
    // 2) Delete any remaining components not under the root tree (safe: DeleteObject checks GUObjectArray)
    for (USceneComponent*& Comp : Components)
    {
        if (Comp)
        {
            ObjectFactory::DeleteObject(Comp);
            Comp = nullptr;
        }
    }
    Components.Empty();
    //TextComp->SetupAttachment(GetRootComponent());
}

void AActor::BeginPlay()
{
}

void AActor::Tick(float DeltaSeconds)
{
}

void AActor::Destroy()
{
    if (!bCanEverTick) return;
    // Prefer world-managed destruction to remove from world actor list
    if (World)
    {
        // Avoid using 'this' after the call
        World->DestroyActor(this);
        return;
    }
    // Fallback: directly delete the actor via factory
    ObjectFactory::DeleteObject(this);
}

// ───────────────
// Transform API
// ───────────────
void AActor::SetActorTransform(const FTransform& NewTransform)
{
    FBound OldBounds = GetBounds();
    if (RootComponent)
    {
        RootComponent->SetWorldTransform(NewTransform);
    }
    if (World)
    {
        FBound NewBounds = GetBounds();
        World->UpdateActorInOctree(this, OldBounds, NewBounds);
    }
}


FTransform AActor::GetActorTransform() const
{
    return RootComponent ? RootComponent->GetWorldTransform() : FTransform();
}

void AActor::SetActorLocation(const FVector& NewLocation)
{
    FBound OldBounds = GetBounds();
    if (RootComponent)
    {
        RootComponent->SetWorldLocation(NewLocation);
    }
    if (World)
    {
        FBound NewBounds = GetBounds();
        World->UpdateActorInOctree(this, OldBounds, NewBounds);
    }
}

FVector AActor::GetActorLocation() const
{
    return RootComponent ? RootComponent->GetWorldLocation() : FVector();
}

void AActor::SetActorRotation(const FVector& EulerDegree)
{
    if (RootComponent)
    {
        RootComponent->SetWorldRotation(FQuat::MakeFromEuler(EulerDegree));
    }
}

void AActor::SetActorRotation(const FQuat& InQuat)
{
    FBound OldBounds = GetBounds();
    if (RootComponent)
    {
        RootComponent->SetWorldRotation(InQuat);
    }
    if (World)
    {
        FBound NewBounds = GetBounds();
        World->UpdateActorInOctree(this, OldBounds, NewBounds);
    }
}

FQuat AActor::GetActorRotation() const
{
    return RootComponent ? RootComponent->GetWorldRotation() : FQuat();
}

void AActor::SetActorScale(const FVector& NewScale)
{
    FBound OldBounds = GetBounds();
    if (RootComponent)
    {
        RootComponent->SetWorldScale(NewScale);
    }
    if (World)
    {
        FBound NewBounds = GetBounds();
        World->UpdateActorInOctree(this, OldBounds, NewBounds);
    }
}

FVector AActor::GetActorScale() const
{
    return RootComponent ? RootComponent->GetWorldScale() : FVector(1, 1, 1);
}

FMatrix AActor::GetWorldMatrix() const
{
    return RootComponent ? RootComponent->GetWorldMatrix() : FMatrix::Identity();
}

void AActor::AddActorWorldRotation(const FQuat& DeltaRotation)
{
    FBound OldBounds = GetBounds();
    if (RootComponent)
    {
        RootComponent->AddWorldRotation(DeltaRotation);
    }
    if (World)
    {
        FBound NewBounds = GetBounds();
        World->UpdateActorInOctree(this, OldBounds, NewBounds);
    }
}

void AActor::AddActorWorldRotation(const FVector& DeltaEuler)
{
    /* if (RootComponent)
     {
         FQuat DeltaQuat = FQuat::FromEuler(DeltaEuler.X, DeltaEuler.Y, DeltaEuler.Z);
         RootComponent->AddWorldRotation(DeltaQuat);
     }*/
}

void AActor::AddActorWorldLocation(const FVector& DeltaLocation)
{
    FBound OldBounds = GetBounds();
    if (RootComponent)
    {
        RootComponent->AddWorldOffset(DeltaLocation);
    }
    if (World)
    {
        FBound NewBounds = GetBounds();
        World->UpdateActorInOctree(this, OldBounds, NewBounds);
    }
}

void AActor::AddActorLocalRotation(const FVector& DeltaEuler)
{
    /*  if (RootComponent)
      {
          FQuat DeltaQuat = FQuat::FromEuler(DeltaEuler.X, DeltaEuler.Y, DeltaEuler.Z);
          RootComponent->AddLocalRotation(DeltaQuat);
      }*/
}

void AActor::AddActorLocalRotation(const FQuat& DeltaRotation)
{
    FBound OldBounds = GetBounds();
    if (RootComponent)
    {
        RootComponent->AddLocalRotation(DeltaRotation);
    }
    if (World)
    {
        FBound NewBounds = GetBounds();
        World->UpdateActorInOctree(this, OldBounds, NewBounds);
    }
}

void AActor::AddActorLocalLocation(const FVector& DeltaLocation)
{
    FBound OldBounds = GetBounds();
    if (RootComponent)
    {
        RootComponent->AddLocalOffset(DeltaLocation);
    }
    if (World)
    {
        FBound NewBounds = GetBounds();
        World->UpdateActorInOctree(this, OldBounds, NewBounds);
    }
}

const TArray<USceneComponent*>& AActor::GetComponents() const
{
    return Components;
}

void AActor::AddComponent(USceneComponent* Component)
{
    if (!Component)
    {
        return;
    }

    Components.push_back(Component);
    if (!RootComponent)
    {
        RootComponent = Component;
        //Component->SetupAttachment(RootComponent);
    }

    // If world is already assigned and this is a primitive, register it now
    if (World)
    {
        if (auto* Prim = Cast<UPrimitiveComponent>(Component))
        {
            if (World->GetPartitionManager())
            {
                World->GetPartitionManager()->Register(Prim);
            }
        }
    }
}
