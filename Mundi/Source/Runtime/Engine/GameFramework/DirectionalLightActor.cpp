#include "pch.h"
#include "DirectionalLightActor.h"
#include "DirectionalLightComponent.h"
// IMPLEMENT_CLASS is now auto-generated in .generated.cpp
//BEGIN_PROPERTIES(ADirectionalLightActor)
//	MARK_AS_SPAWNABLE("디렉셔널 라이트", "방향성 빛을 발산하는 라이트 액터입니다.")
//END_PROPERTIES()

ADirectionalLightActor::ADirectionalLightActor()
{
	ObjectName = "Directional Light Actor";
	LightComponent = CreateDefaultSubobject<UDirectionalLightComponent>("DirectionalLightComponent");

	RootComponent = LightComponent;
}

ADirectionalLightActor::~ADirectionalLightActor()
{
}

void ADirectionalLightActor::DuplicateSubObjects()
{
	Super::DuplicateSubObjects();

	for (UActorComponent* Component : OwnedComponents)
	{
		if (UDirectionalLightComponent* Light = Cast<UDirectionalLightComponent>(Component))
		{
			LightComponent = Light;
			break;
		}
	}
}

void ADirectionalLightActor::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	Super::Serialize(bInIsLoading, InOutHandle);

	if (bInIsLoading)
	{
		LightComponent = Cast<UDirectionalLightComponent>(RootComponent);
	}
}
