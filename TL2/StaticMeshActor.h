#pragma once
#include "Actor.h"
#include "StaticMeshComponent.h"
#include "Enums.h"
class AStaticMeshActor : public AActor
{
public:
    DECLARE_CLASS(AStaticMeshActor, AActor)

    AStaticMeshActor();
    virtual void Tick(float DeltaTime) override;
protected:
    ~AStaticMeshActor() override;

public:
    virtual FBound GetBounds() const override;
    UStaticMeshComponent* GetStaticMeshComponent() const { return StaticMeshComponent; }
    void SetStaticMeshComponent(UStaticMeshComponent* InStaticMeshComponent);
	void SetCollisionComponent(EPrimitiveType InType = EPrimitiveType::Default);

protected:
    UStaticMeshComponent* StaticMeshComponent;
};

