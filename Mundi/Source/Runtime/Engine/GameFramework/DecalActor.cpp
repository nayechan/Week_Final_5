#include "pch.h"
#include "DecalActor.h"
#include "DecalComponent.h"
// IMPLEMENT_CLASS is now auto-generated in .generated.cpp
//BEGIN_PROPERTIES(ADecalActor)
//	MARK_AS_SPAWNABLE("데칼", "표면에 투영되는 데칼 액터입니다.")
//END_PROPERTIES()

ADecalActor::ADecalActor()
{
	ObjectName = "Static Mesh Actor";
	DecalComponent = CreateDefaultSubobject<UDecalComponent>("DecalComponent");

	RootComponent = DecalComponent;
}

ADecalActor::~ADecalActor()
{
}

void ADecalActor::DuplicateSubObjects()
{
	Super::DuplicateSubObjects();

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

	if(bInIsLoading)
	{
		DecalComponent = Cast<UDecalComponent>(RootComponent);
	}
}
