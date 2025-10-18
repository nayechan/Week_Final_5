#include "pch.h"
#include "DecalActor.h"
#include "DecalComponent.h"

IMPLEMENT_CLASS(ADecalActor)

BEGIN_PROPERTIES(ADecalActor)
	MARK_AS_SPAWNABLE("데칼", "표면에 투영되는 데칼 액터입니다.")
END_PROPERTIES()

ADecalActor::ADecalActor()
{
	Name = "Static Mesh Actor";
	DecalComponent = CreateDefaultSubobject<UDecalComponent>("DecalComponent");

	// 루트 교체
	USceneComponent* TempRootComponent = RootComponent;
	RootComponent = DecalComponent;
	RemoveOwnedComponent(TempRootComponent);
}

ADecalActor::~ADecalActor()
{
}

void ADecalActor::DuplicateSubObjects()
{
	Super_t::DuplicateSubObjects();

	// 자식을 순회하면서 UDecalComponent를 찾음
	for (UActorComponent* Component : OwnedComponents)
	{
		if (UDecalComponent* Decal = Cast<UDecalComponent>(Component))
		{
			DecalComponent = Decal;
			break;
		}
	}
}

void ADecalActor::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	Super::Serialize(bInIsLoading, InOutHandle);

	if (bInIsLoading)
	{
		DecalComponent = Cast<UDecalComponent>(RootComponent);
	}
}
