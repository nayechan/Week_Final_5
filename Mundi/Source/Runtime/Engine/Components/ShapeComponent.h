#pragma once

#include "PrimitiveComponent.h"
#include "UShapeComponent.generated.h"

enum class EShapeKind : uint8
{
	Box = 0,
	Sphere,
	Capsule,
};

struct FBoxShape { FVector BoxExtent; };
struct FSphereShape { float SphereRadius; };
struct FCapsuleSphere { float CapsuleRadius, CapsuleHalfHeight; };

struct FShape
{
	FShape() {}

	EShapeKind Kind; 
	union {
		FBoxShape Box;
		FSphereShape Sphere;
		FCapsuleSphere Capsule;
	};
}; 

class UShapeComponent : public UPrimitiveComponent
{ 
public:  

	GENERATED_REFLECTION_BODY();

	UShapeComponent();

	virtual void TickComponent(float DeltaSeconds) override;

	virtual void GetShape(FShape& OutShape) const {};
	virtual void BeginPlay() override;
    virtual void OnRegister(UWorld* InWorld) override;
    virtual void OnTransformUpdated() override;

    void UpdateOverlaps(); 

    FAABB GetWorldAABB() const override;
	virtual const TArray<FOverlapInfo>& GetOverlapInfos() const override { return OverlapInfos; }

	// Duplication
	virtual void DuplicateSubObjects() override;

	// ㅡㅡㅡㅡㅡㅡㅡㅡㅡ디버깅용ㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡ
 
protected: 
	mutable FAABB WorldAABB; //브로드 페이즈 용 
	TSet<UShapeComponent*> OverlapNow; // 이번 프레임에서 overlap 된 Shap Comps
	TSet<UShapeComponent*> OverlapPrev; // 지난 프레임에서 overlap 됐으면 Cache
	 

	FVector4 ShapeColor ;
	bool bDrawOnlyIfSelected;

	UPROPERTY(EditAnywhere, Category="Shape")
	bool bShapeIsVisible;

	UPROPERTY(EditAnywhere, Category="Shape")
	bool bShapeHiddenInGame;
	TArray<FOverlapInfo> OverlapInfos; 
	//TODO: float LineThickness;

};
