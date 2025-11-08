#pragma once

#include "ShapeComponent.h"
#include "UBoxComponent.generated.h"

class UBoxComponent : public UShapeComponent
{
public:

	GENERATED_REFLECTION_BODY();

	UBoxComponent(); 
	void OnRegister(UWorld* InWorld) override;

	void SetBoxExtent(const FVector& InExtent) { BoxExtent = InExtent; }

	// Duplication
	virtual void DuplicateSubObjects() override;

private:
	UPROPERTY(EditAnywhere, Category="BoxExtent")
	FVector BoxExtent; // Half Extent

	void GetShape(FShape& Out) const override;

public:
	//GetReenderCollection 
	void RenderDebugVolume(class URenderer* Renderer) const override;
};