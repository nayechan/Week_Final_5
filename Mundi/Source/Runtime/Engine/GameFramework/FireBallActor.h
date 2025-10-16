#pragma once
#include "Actor.h"
#include "FireBallComponent.h"

class UFireBallComponent;

class AFireBallActor : public AActor
{
public:
	DECLARE_CLASS(AFireBallActor, AActor)
	AFireBallActor();
protected:
	~AFireBallActor() override;

public:
	UFireBallComponent* GetFireBallComponent() const { return FireBallComponent; }
	// ───── 복사 관련 ────────────────────────────

	void DuplicateSubObjects() override;
	DECLARE_DUPLICATE(AFireBallActor)

	// Serialize
	void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;

protected:
	UFireBallComponent* FireBallComponent;
};