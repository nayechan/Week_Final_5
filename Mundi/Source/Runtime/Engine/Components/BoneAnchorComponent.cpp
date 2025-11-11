#include "pch.h"
#include "BoneAnchorComponent.h"
#include "SkinnedMeshComponent.h"
#include "Renderer.h"
#include "SelectionManager.h"

IMPLEMENT_CLASS(UBoneAnchorComponent)

void UBoneAnchorComponent::SetTarget(USkinnedMeshComponent* InTarget, int32 InBoneIndex)
{
    Target = InTarget;
    BoneIndex = InBoneIndex;
    UpdateAnchorFromBone();
}

void UBoneAnchorComponent::UpdateAnchorFromBone()
{
    if (!Target || BoneIndex < 0)
        return;

    const FMatrix WorldM = Target->ComputeBoneWorldMatrix(BoneIndex);
    // Extract translation (row-vector convention)
    const FVector WorldT(WorldM.M[3][0], WorldM.M[3][1], WorldM.M[3][2]);
    // For now keep anchor rotation/scale identity; future: extract from WorldM if needed
    SetWorldLocation(WorldT);
}

void UBoneAnchorComponent::OnTransformUpdated()
{
    Super::OnTransformUpdated();

    if (bSuppressWriteback)
        return;

    if (!Target || BoneIndex < 0)
        return;

    const FMatrix AnchorWorld = GetWorldTransform().ToMatrix();
    Target->SetBoneWorldMatrix(BoneIndex, AnchorWorld);
}
