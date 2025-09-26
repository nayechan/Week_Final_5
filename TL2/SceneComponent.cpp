#include "pch.h"
#include "SceneComponent.h"
#include <algorithm>
#include "ObjectFactory.h"
#include "PrimitiveComponent.h"
#include "WorldPartitionManager.h"

USceneComponent::USceneComponent()
    : RelativeLocation(0, 0, 0)
    , RelativeRotation(0, 0, 0, 1)
    , RelativeScale(1, 1, 1)
    , AttachParent(nullptr)
{
    UpdateRelativeTransform();
}

USceneComponent::~USceneComponent()
{
    // 자식 메모리 해제
    // 복사본을 만들어 부모 리스트 무효화 문제를 피함
    TArray<USceneComponent*> ChildrenCopy = AttachChildren;
    AttachChildren.clear();
    for (USceneComponent* Child : ChildrenCopy)
    {
        if (Child)
        {
            ObjectFactory::DeleteObject(Child);
        }
    }

    // 부모에서 자신 제거
    if (AttachParent)
    {
        TArray<USceneComponent*>& ParentChildren = AttachParent->AttachChildren;
        ParentChildren.Remove(this);
        AttachParent = nullptr;
    }
}

// ──────────────────────────────
// Relative API
// ──────────────────────────────
void USceneComponent::SetRelativeLocation(const FVector& NewLocation)
{
    RelativeLocation = NewLocation;
    UpdateRelativeTransform();
}
FVector USceneComponent::GetRelativeLocation() const { return RelativeLocation; }

void USceneComponent::SetRelativeRotation(const FQuat& NewRotation)
{
    RelativeRotation = NewRotation;
    UpdateRelativeTransform();
}
FQuat USceneComponent::GetRelativeRotation() const { return RelativeRotation; }

void USceneComponent::SetRelativeScale(const FVector& NewScale)
{
    RelativeScale = NewScale;
    UpdateRelativeTransform();
}
FVector USceneComponent::GetRelativeScale() const { return RelativeScale; }

void USceneComponent::AddRelativeLocation(const FVector& DeltaLocation)
{
    RelativeLocation = RelativeLocation + DeltaLocation;
    UpdateRelativeTransform();
}

void USceneComponent::AddRelativeRotation(const FQuat& DeltaRotation)
{
    RelativeRotation = DeltaRotation * RelativeRotation;
    UpdateRelativeTransform();
}

void USceneComponent::AddRelativeScale3D(const FVector& DeltaScale)
{
    RelativeScale = FVector(RelativeScale.X * DeltaScale.X,
        RelativeScale.Y * DeltaScale.Y,
        RelativeScale.Z * DeltaScale.Z);
    UpdateRelativeTransform();
}

// ──────────────────────────────
// World API
// ──────────────────────────────
FTransform USceneComponent::GetWorldTransform() const
{
    if (AttachParent)
        return AttachParent->GetWorldTransform() * RelativeTransform;
    return RelativeTransform;
}

void USceneComponent::SetWorldTransform(const FTransform& W)
{
    if (AttachParent)
    {
        const FTransform ParentWorld = AttachParent->GetWorldTransform();
        RelativeTransform = ParentWorld.Inverse() * W;
    }
    else
    {
        RelativeTransform = W;
    }

    RelativeLocation = RelativeTransform.Translation;
    RelativeRotation = RelativeTransform.Rotation;
    RelativeScale = RelativeTransform.Scale3D;
}
 
void USceneComponent::SetWorldLocation(const FVector& L)
{
    FTransform W = GetWorldTransform();
    W.Translation = L;
    SetWorldTransform(W);
}
FVector USceneComponent::GetWorldLocation() const
{
    return GetWorldTransform().Translation;
}

void USceneComponent::SetWorldRotation(const FQuat& R)
{
    FTransform W = GetWorldTransform();
    W.Rotation = R;
    SetWorldTransform(W);
}
FQuat USceneComponent::GetWorldRotation() const
{
    return GetWorldTransform().Rotation;
}

void USceneComponent::SetWorldScale(const FVector& S)
{
    FTransform W = GetWorldTransform();
    W.Scale3D = S;
    SetWorldTransform(W);
}
FVector USceneComponent::GetWorldScale() const
{
    return GetWorldTransform().Scale3D;
}

void USceneComponent::AddWorldOffset(const FVector& Delta)
{
    FTransform W = GetWorldTransform();
    W.Translation = W.Translation + Delta;
    SetWorldTransform(W);
}

void USceneComponent::AddWorldRotation(const FQuat& DeltaRot)
{
    FTransform W = GetWorldTransform();
    W.Rotation = DeltaRot * W.Rotation;
    SetWorldTransform(W);
}


void USceneComponent::SetWorldLocationAndRotation(const FVector& L, const FQuat& R)
{
    FTransform W = GetWorldTransform();
    W.Translation = L;
    W.Rotation = R;
    SetWorldTransform(W);
}

void USceneComponent::AddLocalOffset(const FVector& Delta)
{
    const FVector parentDelta = RelativeRotation.RotateVector(Delta);
    RelativeLocation = RelativeLocation + parentDelta;
    UpdateRelativeTransform();
}

void USceneComponent::AddLocalRotation(const FQuat& DeltaRot)
{
    RelativeRotation = (RelativeRotation * DeltaRot).GetNormalized(); // 로컬: 우측곱
    UpdateRelativeTransform();
}

void USceneComponent::SetLocalLocationAndRotation(const FVector& L, const FQuat& R)
{
    RelativeLocation = L;
    RelativeRotation = R.GetNormalized();
    UpdateRelativeTransform();
}


FMatrix USceneComponent::GetWorldMatrix() const
{
    return GetWorldTransform().ToMatrixWithScaleLocalXYZ();
}

// ──────────────────────────────
// Attach / Detach
// ──────────────────────────────
void USceneComponent::SetupAttachment(USceneComponent* InParent, EAttachmentRule Rule)
{
    if (AttachParent == InParent) return;

    const FTransform OldWorld = GetWorldTransform();

    // 기존 부모에서 제거
    if (AttachParent)
    {
        auto& Siblings = AttachParent->AttachChildren;
        Siblings.erase(std::remove(Siblings.begin(), Siblings.end(), this), Siblings.end());
    }

    // 새 부모 설정
    AttachParent = InParent;
    if (AttachParent)
        AttachParent->AttachChildren.push_back(this);

    // 규칙 적용
    if (AttachParent)
    {
        if (Rule == EAttachmentRule::KeepWorld)
        {
            const FTransform ParentWorld = AttachParent->GetWorldTransform();
            RelativeTransform = ParentWorld.Inverse() * OldWorld;
        }
        // KeepRelative: 기존 RelativeTransform 유지
    }
    else
    {
        if (Rule == EAttachmentRule::KeepWorld)
            RelativeTransform = OldWorld;
    }

    RelativeLocation = RelativeTransform.Translation;
    RelativeRotation = RelativeTransform.Rotation;
    RelativeScale = RelativeTransform.Scale3D;
}

void USceneComponent::DetachFromParent(bool bKeepWorld)
{
    const FTransform OldWorld = GetWorldTransform();

    if (AttachParent)
    {
        auto& Siblings = AttachParent->AttachChildren;
        Siblings.erase(std::remove(Siblings.begin(), Siblings.end(), this), Siblings.end());
        AttachParent = nullptr;
    }

    if (bKeepWorld)
        RelativeTransform = OldWorld;

    RelativeLocation = RelativeTransform.Translation;
    RelativeRotation = RelativeTransform.Rotation;
    RelativeScale = RelativeTransform.Scale3D;
}

// ──────────────────────────────
// 내부 유틸
// ──────────────────────────────
void USceneComponent::UpdateRelativeTransform()
{
    RelativeTransform = FTransform(RelativeLocation, RelativeRotation, RelativeScale);

    // Notify partition manager if this is a primitive component with a world/owner
    if (auto* Prim = Cast<UPrimitiveComponent>(this))
    {
        if (AActor* Owner = GetOwner())
        {
            if (UWorld* World = Owner->GetWorld())
            {
                if (auto* PM = World->GetPartitionManager())
                {
                    PM->MarkDirty(Prim);
                }
            }
        }
    }
}
