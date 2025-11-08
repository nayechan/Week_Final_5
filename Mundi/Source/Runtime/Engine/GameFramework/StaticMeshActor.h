#pragma once

#include "Actor.h"
#include "Enums.h"
#include "AStaticMeshActor.generated.h"

class UStaticMeshComponent;
class AStaticMeshActor : public AActor
{
public:

	GENERATED_REFLECTION_BODY()

	AStaticMeshActor(); 
	virtual void Tick(float DeltaTime) override;
protected:
	~AStaticMeshActor() override;

public:
	virtual FAABB GetBounds() const override;
	UStaticMeshComponent* GetStaticMeshComponent() const { return StaticMeshComponent; }
	void SetStaticMeshComponent(UStaticMeshComponent* InStaticMeshComponent);

	// ───── 복사 관련 ────────────────────────────
	void DuplicateSubObjects() override;

	// Serialize
	void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;

protected:
	UStaticMeshComponent* StaticMeshComponent;
};

