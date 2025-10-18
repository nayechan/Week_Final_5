#pragma once
#include "Actor.h"

class UBillboardComponent;
class UPerspectiveDecalComponent;

class AFakeSpotLightActor : public AActor
{
public:
	DECLARE_CLASS(AFakeSpotLightActor, AActor)
	GENERATED_REFLECTION_BODY()

	AFakeSpotLightActor();
protected:
	~AFakeSpotLightActor() override;

public:
	UBillboardComponent* GetBillboardComponent() const { return BillboardComponent; }
	UPerspectiveDecalComponent* GetDecalComponent() const { return DecalComponent; }

	void DuplicateSubObjects() override;
	DECLARE_DUPLICATE(AFakeSpotLightActor)

	void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;
protected:
	UBillboardComponent* BillboardComponent{};
	UPerspectiveDecalComponent* DecalComponent{};
};
