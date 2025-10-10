#include "pch.h"
#include "AABoundingBoxComponent.h"
#include "StaticMeshActor.h"
#include "StaticMeshComponent.h"
#include "ObjectFactory.h"
#include "BillboardComponent.h"
AStaticMeshActor::AStaticMeshActor()
{
    Name = "Static Mesh Actor";
    StaticMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>("StaticMeshComponent");
    StaticMeshComponent->SetupAttachment(RootComponent);
    //BillboardComp = CreateDefaultSubobject<UBillboardComponent>("BillboardBox");

    //StaticMeshComponent->SetOwnedActor(this);
    //bTickInEditor = true; // 테스트 용
}

void AStaticMeshActor::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    AddActorWorldLocation(FVector(3.0f, 0.0f, 0.0f) * DeltaTime);
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

FAABB AStaticMeshActor::GetBounds() const
{
    if (CollisionComponent)
    {
        return CollisionComponent->GetWorldBoundFromCube();
    }
    return FAABB();
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

void AStaticMeshActor::DuplicateSubObjects()
{
    Super::DuplicateSubObjects();

    StaticMeshComponent = StaticMeshComponent->Duplicate();
}
