#pragma once
#include "ShapeComponent.h"

class USphereComponent : public UShapeComponent
{
public:
    DECLARE_CLASS(USphereComponent, UShapeComponent)
    GENERATED_REFLECTION_BODY();

    USphereComponent();
    void OnRegister(UWorld* InWorld) override;

    // Duplication
    virtual void DuplicateSubObjects() override;
    DECLARE_DUPLICATE(USphereComponent)

private:
    float SphereRadius = 0;

    void GetShape(FShape& Out) const override;

public:

    void RenderDebugVolume(class URenderer* Renderer) const override;
};
