#pragma once
#include "Actor.h"

class FGameObject
{
public:
    uint32  UUID;
    FVector Velocity;
    FVector Scale;
    FVector Forward;
    bool bIsActive;
    
    void SetTag(FString NewTag) { Owner->SetTag(NewTag); }
    FString GetTag() { return Owner->GetTag(); }

    void SetLocation(FVector NewLocation) { Owner->SetActorLocation(NewLocation); }
    FVector GetLocation() { return Owner->GetActorLocation(); }

    void SetScale(FVector NewScale) { Owner->SetActorScale(NewScale); }
    FVector GetScale() { return Owner->GetActorScale(); }

    void SetRotation(FVector NewRotation)
    {
        FQuat NewQuat = FQuat::MakeFromEulerZYX(NewRotation);
        Owner->SetActorRotation(NewQuat);
    }
    FVector GetRotation() { return Owner->GetActorRotation().ToEulerZYXDeg(); }
    
    void SetIsActive(bool NewIsActive)
    {
        Owner->SetActorIsVisible(NewIsActive);
    }
    
    bool GetIsActive()
    {
        return Owner->GetActorIsVisible();
    }

    void PrintLocation()
    {
        FVector Location =  Owner->GetActorLocation();
        UE_LOG("Location %f %f %f\n", Location.X, Location.Y, Location.Z);
    }

    void SetOwner(AActor* NewOwner) { Owner = NewOwner; }
    AActor* GetOwner() { return Owner; }

    // Returns the owner's current forward direction (unit vector)
    FVector GetForward() { return Owner ? Owner->GetActorRight() : FVector(0, 0, 0); }

private:
    // TODO : 순환 참조 해결
    AActor* Owner;
};
