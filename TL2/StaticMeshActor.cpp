#include "pch.h"
#include "AABoundingBoxComponent.h"
#include "StaticMeshActor.h"
#include "ObjectFactory.h"

AStaticMeshActor::AStaticMeshActor()
{
    Name = "Static Mesh Actor";
    StaticMeshComponent = NewObject<UStaticMeshComponent>();
    AddComponent(StaticMeshComponent);
    StaticMeshComponent->SetupAttachment(RootComponent);

    CollisionComponent = CreateDefaultSubobject<UAABoundingBoxComponent>(FName("CollisionBox"));
    AddComponent(CollisionComponent);
	CollisionComponent->SetupAttachment(RootComponent);
}

void AStaticMeshActor::Tick(float DeltaTime)
{
    if (bIsPicked)
    {
        //CollisionComponent->SetFromVertices(StaticMeshComponent->GetStaticMesh()->GetStaticMeshAsset()->Vertices);
    }
}

AStaticMeshActor::~AStaticMeshActor()
{
    if (StaticMeshComponent)
    {
        ObjectFactory::DeleteObject(StaticMeshComponent);
    }
    StaticMeshComponent = nullptr;
}

FBound AStaticMeshActor::GetBounds() const
{
    if (CollisionComponent)
    {
        return CollisionComponent->GetWorldBoundFromCube();
    }
    return FBound();
}

void AStaticMeshActor::SetStaticMeshComponent(UStaticMeshComponent* InStaticMeshComponent)
{
    StaticMeshComponent = InStaticMeshComponent;
}

void AStaticMeshActor::SetCollisionComponent(EPrimitiveType InType)
{
    CollisionComponent->SetFromVertices(StaticMeshComponent->GetStaticMesh()->GetStaticMeshAsset()->Vertices);
    CollisionComponent->SetPrimitiveType(InType);
}
