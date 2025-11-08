#pragma once

#include "ShapeComponent.h"
#include "UCapsuleComponent.generated.h"

class UCapsuleComponent : public UShapeComponent
{
public:

	GENERATED_REFLECTION_BODY();

	UCapsuleComponent();
	void OnRegister(UWorld* World) override;

	// Duplication
	virtual void DuplicateSubObjects() override;

protected:
	UPROPERTY(EditAnywhere, Category="CapsuleHalfHeight")
	float CapsuleHalfHeight;

	UPROPERTY(EditAnywhere, Category="CapsuleHalfHeight")
	float CapsuleRadius;

	void GetShape(FShape& Out) const override; 

public:
	void RenderDebugVolume(class URenderer* Renderer) const override;
};