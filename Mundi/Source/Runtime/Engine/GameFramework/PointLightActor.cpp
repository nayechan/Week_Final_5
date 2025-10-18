#include "pch.h"
#include "PointLightActor.h"
#include "PointLightComponent.h"

IMPLEMENT_CLASS(APointLightActor)

BEGIN_PROPERTIES(APointLightActor)
	MARK_AS_SPAWNABLE("포인트 라이트", "전방향으로 빛을 발산하는 라이트 액터입니다.")
END_PROPERTIES()

APointLightActor::APointLightActor()
{
	Name = "Point Light Actor";
	LightComponent = CreateDefaultSubobject<UPointLightComponent>("PointLightComponent");

	USceneComponent* TempRootComponent = RootComponent;
	RootComponent = LightComponent;
	RemoveOwnedComponent(TempRootComponent);
}

APointLightActor::~APointLightActor()
{
}

void APointLightActor::DuplicateSubObjects()
{
	Super_t::DuplicateSubObjects();

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
