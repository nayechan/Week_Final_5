#pragma once
#include "Vector.h"
#include "ActorComponent.h"



// 부착 시 로컬을 유지할지, 월드를 유지할지
enum class EAttachmentRule
{
    KeepRelative,
    KeepWorld
};

class URenderer;
class USceneComponent : public UActorComponent
{
public:
    DECLARE_CLASS(USceneComponent, UActorComponent)
    USceneComponent();

protected:
    ~USceneComponent() override;


public:
    // ──────────────────────────────
    // Relative Transform API
    // ──────────────────────────────
    void SetRelativeLocation(const FVector& NewLocation);
    FVector GetRelativeLocation() const;

    void SetRelativeRotation(const FQuat& NewRotation);
    FQuat GetRelativeRotation() const;

    void SetRelativeScale(const FVector& NewScale);
    FVector GetRelativeScale() const;

    void AddRelativeLocation(const FVector& DeltaLocation);
    void AddRelativeRotation(const FQuat& DeltaRotation);
    void AddRelativeScale3D(const FVector& DeltaScale);

    // ──────────────────────────────
    // World Transform API
    // ──────────────────────────────
    FTransform GetWorldTransform() const;
    void SetWorldTransform(const FTransform& W);

    void SetWorldLocation(const FVector& L);
    FVector GetWorldLocation() const;

    void SetWorldRotation(const FQuat& R);
    FQuat GetWorldRotation() const;

    void SetWorldScale(const FVector& S);
    FVector GetWorldScale() const;

    void AddWorldOffset(const FVector& Delta);
    void AddWorldRotation(const FQuat& DeltaRot);
    void SetWorldLocationAndRotation(const FVector& L, const FQuat& R);

    void AddLocalOffset(const FVector& Delta);
    void AddLocalRotation(const FQuat& DeltaRot);
    void SetLocalLocationAndRotation(const FVector& L, const FQuat& R);

    FMatrix GetWorldMatrix() const; // ToMatrixWithScale

    // ──────────────────────────────
    // Attach/Detach
    // ──────────────────────────────
    void SetupAttachment(USceneComponent* InParent, EAttachmentRule Rule = EAttachmentRule::KeepWorld);
    void DetachFromParent(bool bKeepWorld = true);

    // ──────────────────────────────
    // Hierarchy Access
    // ──────────────────────────────
    USceneComponent* GetAttachParent() const { return AttachParent; }
    const TArray<USceneComponent*>& GetAttachChildren() const { return AttachChildren; }
    UWorld* GetWorld() { return AttachParent ? AttachParent->GetWorld() : nullptr; }

    // ───── 복사 관련 ────────────────────────────
    void DuplicateSubObjects() override;
    DECLARE_DUPLICATE(USceneComponent)

    

protected:
    FVector RelativeLocation{ 0,0,0 };
    FQuat   RelativeRotation;
    FVector RelativeScale{ 1,1,1 };

    
    // Hierarchy
    USceneComponent* AttachParent = nullptr;
    TArray<USceneComponent*> AttachChildren;

    // 로컬(부모 기준) 트랜스폼
    FTransform RelativeTransform;

    void UpdateRelativeTransform();
    
private:
    // DuplicateSubObjects에서 쓰기 위함
    void SetParent(USceneComponent* InParent)
    {
        AttachParent = InParent;
    }
};
