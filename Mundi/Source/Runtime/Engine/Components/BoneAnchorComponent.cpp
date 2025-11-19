#include "pch.h"
#include "BoneAnchorComponent.h"
#include "SelectionManager.h"

IMPLEMENT_CLASS(UBoneAnchorComponent)

void UBoneAnchorComponent::SetTarget(USkeletalMeshComponent* InTarget, int32 InBoneIndex)
{
    Target = InTarget;
    BoneIndex = InBoneIndex;
    UpdateAnchorFromBone();
}

void UBoneAnchorComponent::UpdateAnchorFromBone()
{
    if (!Target || BoneIndex < 0)
        return;

    const FTransform BoneWorld = Target->GetBoneWorldTransform(BoneIndex);
    SetWorldTransform(BoneWorld) ;
}

void UBoneAnchorComponent::OnTransformUpdated()
{
    Super::OnTransformUpdated();

    if (!Target || BoneIndex < 0)
        return;

    // Additive 시스템 사용 시 직접 본 수정을 건너뜀
    // (뷰어에서는 UpdateBoneTransformFromGizmo가 additive를 통해 처리함)
    if (bSuppressWriteback)
        return;

    const FTransform AnchorWorld = GetWorldTransform();
    Target->SetBoneWorldTransform(BoneIndex, AnchorWorld);
}
