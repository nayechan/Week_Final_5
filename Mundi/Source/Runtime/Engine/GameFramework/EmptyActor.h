#pragma once

#include "Actor.h"
#include "AEmptyActor.generated.h"

class AEmptyActor : public AActor
{
public:

	GENERATED_REFLECTION_BODY()

	AEmptyActor();
	~AEmptyActor() override = default;

	void DuplicateSubObjects() override;

	void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;
};
