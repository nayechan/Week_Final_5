#include "pch.h"
#include "SpotLightActor.h"
#include "SpotLightComponent.h"
// IMPLEMENT_CLASS is now auto-generated in .generated.cpp
//BEGIN_PROPERTIES(ASpotLightActor)
//	MARK_AS_SPAWNABLE("스포트 라이트", "스포트 라이트 액터입니다.")
//END_PROPERTIES()

ASpotLightActor::ASpotLightActor()
{
	ObjectName = "Spot Light Actor";
	LightComponent = CreateDefaultSubobject<USpotLightComponent>("SpotLightComponent");

	RootComponent = LightComponent;
}

ASpotLightActor::~ASpotLightActor()
{
}

void ASpotLightActor::DuplicateSubObjects()
{
	Super::DuplicateSubObjects();

	for (UActorComponent* Component : OwnedComponents)
	{
		if (USpotLightComponent* Light = Cast<USpotLightComponent>(Component))
		{
			LightComponent = Light;
			break;
		}
	}
}

void ASpotLightActor::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	Super::Serialize(bInIsLoading, InOutHandle);

	if (bInIsLoading)
	{
		LightComponent = Cast<USpotLightComponent>(RootComponent);
	}
}
