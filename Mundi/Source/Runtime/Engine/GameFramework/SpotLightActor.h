#pragma once

#include "Actor.h"
#include "ASpotLightActor.generated.h"

class USpotLightComponent;

class ASpotLightActor : public AActor
{
public:

	GENERATED_REFLECTION_BODY()

	ASpotLightActor();
protected:
	~ASpotLightActor() override;

public:
	USpotLightComponent* GetLightComponent() const { return LightComponent; }

	void DuplicateSubObjects() override;

	void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;

protected:
	USpotLightComponent* LightComponent;
};
