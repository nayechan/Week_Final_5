#include "pch.h"
#include "StaticMeshActor.h"
#include "StaticMeshComponent.h"
#include "ObjectFactory.h"
#include "BillboardComponent.h"

IMPLEMENT_CLASS(AStaticMeshActor)

BEGIN_PROPERTIES(AStaticMeshActor)
	MARK_AS_SPAWNABLE("스태틱 메시", "정적 메시를 렌더링하는 액터입니다.")
END_PROPERTIES()

AStaticMeshActor::AStaticMeshActor()
{
    Name = "Static Mesh Actor";
    StaticMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>("StaticMeshComponent");
    
    // 루트 교체
    USceneComponent* TempRootComponent = RootComponent;
    RootComponent = StaticMeshComponent;
    RemoveOwnedComponent(TempRootComponent);
}

void AStaticMeshActor::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    //AddActorWorldLocation(FVector(3.0f, 0.0f, 0.0f) * DeltaTime);
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
    if (StaticMeshComponent)
    {
        return StaticMeshComponent->GetWorldAABB();
    }

    return FAABB();
}

void AStaticMeshActor::SetStaticMeshComponent(UStaticMeshComponent* InStaticMeshComponent)
{
    StaticMeshComponent = InStaticMeshComponent;
}

void AStaticMeshActor::DuplicateSubObjects()
{
    Super::DuplicateSubObjects();

    for (UActorComponent* Component : OwnedComponents)
    {
        if (UStaticMeshComponent* StaticMeshComp = Cast<UStaticMeshComponent>(Component))
        {
            StaticMeshComponent = StaticMeshComp;
            break;
        }
    }
}

void AStaticMeshActor::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
    Super::Serialize(bInIsLoading, InOutHandle);

    if (bInIsLoading)
    {
        StaticMeshComponent = Cast<UStaticMeshComponent>(RootComponent);
    }
}
