#pragma once

#include "Actor.h"
#include "ADecalActor.generated.h"

class UDecalComponent;

class ADecalActor : public AActor
{
public:

    GENERATED_REFLECTION_BODY()

    ADecalActor();
protected:
    ~ADecalActor() override;

public:
    UDecalComponent* GetDecalComponent() const { return DecalComponent; }

    // ───── 복사 관련 ────────────────────────────
    void DuplicateSubObjects() override;

    // Serialize
    void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;

protected:
    UDecalComponent* DecalComponent;
};
