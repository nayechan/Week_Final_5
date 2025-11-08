#pragma once

#include "Actor.h"
#include "ADirectionalLightActor.generated.h"

class UDirectionalLightComponent;

class ADirectionalLightActor : public AActor
{
public:

	GENERATED_REFLECTION_BODY()

	ADirectionalLightActor();
protected:
	~ADirectionalLightActor() override;

public:
	UDirectionalLightComponent* GetLightComponent() const { return LightComponent; }

	void DuplicateSubObjects() override;

	void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;

protected:
	UDirectionalLightComponent* LightComponent;
};
