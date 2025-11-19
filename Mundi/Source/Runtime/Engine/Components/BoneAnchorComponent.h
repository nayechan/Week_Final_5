#pragma once

#include "SceneComponent.h"
#include "SkeletalMeshComponent.h"

// A single anchor component that represents the transform of a selected bone.
// The viewer selects this component so the editor gizmo latches onto it.
class UBoneAnchorComponent : public USceneComponent
{
public:
    DECLARE_CLASS(UBoneAnchorComponent, USceneComponent)

    void SetTarget(USkeletalMeshComponent* InTarget, int32 InBoneIndex);
    void SetBoneIndex(int32 InBoneIndex) { BoneIndex = InBoneIndex; }
    int32 GetBoneIndex() const { return BoneIndex; }
    USkeletalMeshComponent* GetTarget() const { return Target; }

    // Updates this anchor's world transform from the target bone's current transform
    void UpdateAnchorFromBone();

    // When user moves gizmo, write back to the bone
    void OnTransformUpdated() override;

    // Additive 시스템 사용 시 OnTransformUpdated에서 직접 본 수정을 방지
    void SetSuppressWriteback(bool bSuppress) { bSuppressWriteback = bSuppress; }
    bool IsSuppressingWriteback() const { return bSuppressWriteback; }

private:
    USkeletalMeshComponent* Target = nullptr;
    int32 BoneIndex = -1;
    bool bSuppressWriteback = false;  // true면 OnTransformUpdated에서 본 수정 안함
};
