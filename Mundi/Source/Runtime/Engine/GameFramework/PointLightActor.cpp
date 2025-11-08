#include "pch.h"
#include "PointLightActor.h"
#include "PointLightComponent.h"
// IMPLEMENT_CLASS is now auto-generated in .generated.cpp
//BEGIN_PROPERTIES(APointLightActor)
//	MARK_AS_SPAWNABLE("포인트 라이트", "전방향으로 빛을 발산하는 라이트 액터입니다.")
//END_PROPERTIES()

APointLightActor::APointLightActor()
{
	ObjectName = "Point Light Actor";
	LightComponent = CreateDefaultSubobject<UPointLightComponent>("PointLightComponent");

	RootComponent = LightComponent;
}

APointLightActor::~APointLightActor()
{
}

void APointLightActor::DuplicateSubObjects()
{
	Super::DuplicateSubObjects();

	for (UActorComponent* Component : OwnedComponents)
	{
		if (UPointLightComponent* Light = Cast<UPointLightComponent>(Component))
		{
			LightComponent = Light;
			break;
		}
	}
}

void APointLightActor::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	Super::Serialize(bInIsLoading, InOutHandle);

	if (bInIsLoading)
	{
		LightComponent = Cast<UPointLightComponent>(RootComponent);
	}
}
