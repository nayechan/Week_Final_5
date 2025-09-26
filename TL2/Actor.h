#pragma once
#include "Object.h"
#include "Vector.h"
#include "AABoundingBoxComponent.h"
class UWorld;
class USceneComponent;
class UAABoundingBoxComponent;
class UShapeComponent;

class AActor : public UObject
{
public:
    DECLARE_CLASS(AActor, UObject)
    AActor(); 

protected:
    ~AActor() override;

public:
    virtual void BeginPlay();
    virtual void Tick(float DeltaSeconds);
    virtual void Destroy();
    virtual FBound GetBounds() const { return FBound(); }
    // ───────────────
    // Transform API
    // ───────────────
    void SetActorTransform(const FTransform& NewTransform);
    FTransform GetActorTransform() const;

    void SetActorLocation(const FVector& NewLocation);
    FVector GetActorLocation() const;

    void SetActorRotation(const FVector& EulerDegree);
    void SetActorRotation(const FQuat& InQuat);
    FQuat GetActorRotation() const;

    void SetActorScale(const FVector& NewScale);
    FVector GetActorScale() const;

    FMatrix GetWorldMatrix() const;

    FVector GetActorForward() const { return GetActorRotation().RotateVector(FVector(0, 1, 0)); }
    FVector GetActorRight()   const { return GetActorRotation().RotateVector(FVector(1, 0, 0)); }
    FVector GetActorUp()      const { return GetActorRotation().RotateVector(FVector(0, 0, 1)); }

    void AddActorWorldRotation(const FQuat& DeltaRotation);
    void AddActorWorldRotation(const FVector& DeltaEuler);
    void AddActorWorldLocation(const FVector& DeltaRot);

    void AddActorLocalRotation(const FVector& DeltaEuler);

    void AddActorLocalRotation(const FQuat& DeltaRotation);
    void AddActorLocalLocation(const FVector& DeltaRot);

    void SetWorld(UWorld* InWorld) { World = InWorld; }
    UWorld* GetWorld() const { return World; }

    USceneComponent* GetRootComponent() { return RootComponent; }

    void SetIsPicked(bool picked) { bIsPicked = picked; }
    bool GetIsPicked() { return bIsPicked; }



    //-----------------------------
    //----------Getter------------
    const TArray<USceneComponent*>& GetComponents() const;

    void SetName(const FString& InName) { Name = InName; }
    const FName& GetName() { return Name; }

    template<typename T>
    T* CreateDefaultSubobject(const FName& SubobjectName)
    {
        // NewObject를 통해 생성
        T* Comp = ObjectFactory::NewObject<T>();
        Comp->SetOwner(this);
       // Comp->SetName(SubobjectName);  //나중에 추가 구현
        AddComponent(Comp);
        return Comp;
    }

public:
    FName Name;
    USceneComponent* RootComponent = nullptr;
    UAABoundingBoxComponent* CollisionComponent = nullptr;

    UWorld* World = nullptr;
    
    // Visibility properties
    void SetActorHiddenInGame(bool bNewHidden) { bHiddenInGame = bNewHidden; }
    bool GetActorHiddenInGame() const { return bHiddenInGame; }
    bool IsActorVisible() const { return !bHiddenInGame; }
    void AddComponent(USceneComponent* Component);
protected:
    TArray<USceneComponent*> Components;
    bool bIsPicked = false;
    bool bCanEverTick = true;
    bool bHiddenInGame = false;
};
