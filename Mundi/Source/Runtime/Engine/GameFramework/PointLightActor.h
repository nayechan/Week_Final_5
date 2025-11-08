#pragma once

#include "Actor.h"
#include "APointLightActor.generated.h"

class UPointLightComponent;

class APointLightActor : public AActor
{
public:

	GENERATED_REFLECTION_BODY()

	APointLightActor();
protected:
	~APointLightActor() override;

public:
	UPointLightComponent* GetLightComponent() const { return LightComponent; }

	void DuplicateSubObjects() override;

	void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;

protected:
	UPointLightComponent* LightComponent;
};
