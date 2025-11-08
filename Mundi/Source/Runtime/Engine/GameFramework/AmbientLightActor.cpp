#include "pch.h"
#include "AmbientLightActor.h"
#include "AmbientLightComponent.h"

//BEGIN_PROPERTIES(AAmbientLightActor)
//	MARK_AS_SPAWNABLE("앰비언트 라이트", "주변광을 발산하는 라이트 액터입니다.")
//END_PROPERTIES()

AAmbientLightActor::AAmbientLightActor()
{
	ObjectName = "Ambient Light Actor";
	LightComponent = CreateDefaultSubobject<UAmbientLightComponent>("AmbientLightComponent");

	RootComponent = LightComponent;
}

AAmbientLightActor::~AAmbientLightActor()
{
}

void AAmbientLightActor::DuplicateSubObjects()
{
	Super::DuplicateSubObjects();

	for (UActorComponent* Component : OwnedComponents)
	{
		if (UAmbientLightComponent* Light = Cast<UAmbientLightComponent>(Component))
		{
			LightComponent = Light;
			break;
		}
	}
}

void AAmbientLightActor::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	Super::Serialize(bInIsLoading, InOutHandle);

	if (bInIsLoading)
	{
		LightComponent = Cast<UAmbientLightComponent>(RootComponent);
	}
}
