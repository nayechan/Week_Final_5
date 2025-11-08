#pragma once

#include "Actor.h"
#include "AAmbientLightActor.generated.h"

class UAmbientLightComponent;

class AAmbientLightActor : public AActor
{
public:
	GENERATED_REFLECTION_BODY()

	AAmbientLightActor();
protected:
	~AAmbientLightActor() override;

public:
	UAmbientLightComponent* GetLightComponent() const { return LightComponent; }

	void DuplicateSubObjects() override;

	void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;

protected:
	UAmbientLightComponent* LightComponent;
};
