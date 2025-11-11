#pragma once

#include "SceneComponent.h"

class USkinnedMeshComponent;

// A single anchor component that represents the transform of a selected bone.
// The viewer selects this component so the editor gizmo latches onto it.
class UBoneAnchorComponent : public USceneComponent
{
public:
    DECLARE_CLASS(UBoneAnchorComponent, USceneComponent)

    void SetTarget(USkinnedMeshComponent* InTarget, int32 InBoneIndex);
    void SetBoneIndex(int32 InBoneIndex) { BoneIndex = InBoneIndex; }
    int32 GetBoneIndex() const { return BoneIndex; }
    USkinnedMeshComponent* GetTarget() const { return Target; }

    // Updates this anchor's world transform from the target bone's current transform
    void UpdateAnchorFromBone();

    // When user moves gizmo, write back to the bone
    void OnTransformUpdated() override;

    // Suppress writeback while programmatically repositioning the anchor
    void BeginSuppressWriteback() { bSuppressWriteback = true; }
    void EndSuppressWriteback() { bSuppressWriteback = false; }

private:
    USkinnedMeshComponent* Target = nullptr;
    int32 BoneIndex = -1;
    bool bSuppressWriteback = false;
};
