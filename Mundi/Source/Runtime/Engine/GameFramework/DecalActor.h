#pragma once
#include "Actor.h"

class UDecalComponent;

class ADecalActor : public AActor
{
public:
    DECLARE_CLASS(ADecalActor, AActor)

    ADecalActor();
protected:
    ~ADecalActor() override;

public:
    //virtual FAABB GetBounds() const override;
    UDecalComponent* GetDecalComponent() const { return DecalComponent; }

    // ───── 복사 관련 ────────────────────────────
    void DuplicateSubObjects() override;
    DECLARE_DUPLICATE(ADecalActor)

    // Serialize
    void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;
protected:
    //UBillboardComponent* BillboardComp = nullptr;
    UDecalComponent* DecalComponent;
};
