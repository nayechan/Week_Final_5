#pragma once

#include "ShapeComponent.h"
#include "USphereComponent.generated.h"

class USphereComponent : public UShapeComponent
{
public:

    GENERATED_REFLECTION_BODY();

    USphereComponent();
    void OnRegister(UWorld* InWorld) override;

    // Duplication
    virtual void DuplicateSubObjects() override;

private:
    UPROPERTY(EditAnywhere, Category="SphereRaidus")
    float SphereRadius = 0;

    void GetShape(FShape& Out) const override;

public:

    void RenderDebugVolume(class URenderer* Renderer) const override;
};
