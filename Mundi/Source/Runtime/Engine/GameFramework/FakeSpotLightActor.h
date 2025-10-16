#pragma once
#include "Actor.h"

class UBillboardComponent;
class UPerspectiveDecalComponent;

class AFakeSpotLightActor : public AActor
{
public:
	DECLARE_CLASS(AFakeSpotLightActor, AActor)

	AFakeSpotLightActor();
protected:
	~AFakeSpotLightActor() override;

public:
	UBillboardComponent* GetBillboardComponent() const { return BillboardComponent; }
	UPerspectiveDecalComponent* GetDecalComponent() const { return DecalComponent; }

	// ───── 복사 관련 ────────────────────────────
	void DuplicateSubObjects() override;
	DECLARE_DUPLICATE(AFakeSpotLightActor)

	// Serialize
	void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;
protected:
	UBillboardComponent* BillboardComponent{};
	UPerspectiveDecalComponent* DecalComponent{};
};
