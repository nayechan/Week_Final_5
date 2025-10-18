#pragma once
#include "Actor.h"

class AEmptyActor : public AActor
{
public:
	DECLARE_CLASS(AEmptyActor, AActor)
	GENERATED_REFLECTION_BODY()

	AEmptyActor();
	~AEmptyActor() override = default;

	void DuplicateSubObjects() override;
	DECLARE_DUPLICATE(AEmptyActor)

	void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;
};
