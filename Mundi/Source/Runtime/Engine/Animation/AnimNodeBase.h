#pragma once
#include "Vector.h"
#include "VertexData.h"

class USkeletalMeshComponent;

enum class EAdditiveType
{
    None,
    LocalSpace,
    MeshSpace
};

struct FAnimationBaseContext
{
    USkeletalMeshComponent* Component = nullptr;
    const FSkeleton* Skeleton = nullptr;
    float DeltaSeconds = 0.f;

    void Initialize(USkeletalMeshComponent* InComponent, const FSkeleton* InSkeleton, float InDeltaSeconds = 0.f)
    {
        Component = InComponent;
        Skeleton = InSkeleton;
        DeltaSeconds = InDeltaSeconds;
    }

    bool IsValid() const { return Component != nullptr && Skeleton != nullptr; }

    USkeletalMeshComponent* GetComponent() const { return Component; }
    const FSkeleton* GetSkeleton() const { return Skeleton; }
    float GetDeltaSeconds() const { return DeltaSeconds; }
};

struct FPoseContext : public FAnimationBaseContext
{
    TArray<FTransform> LocalSpacePose;

    void Initialize(USkeletalMeshComponent* InComponent, const FSkeleton* InSkeleton, float InDeltaSeconds = 0.f)
    {
        FAnimationBaseContext::Initialize(InComponent, InSkeleton, InDeltaSeconds);
        const int32 NumBones = (Skeleton) ? static_cast<int32>(Skeleton->Bones.Num()) : 0;
        LocalSpacePose.SetNum(NumBones);
    }

    void ResetToRefPose()
    {
        if (!Skeleton)
        {
            LocalSpacePose.Empty();
            return;
        }

        const int32 NumBones = static_cast<int32>(Skeleton->Bones.Num());
        LocalSpacePose.SetNum(NumBones);

        for (int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex)
        {
            const FBone& ThisBone = Skeleton->Bones[BoneIndex];
            const int32 ParentIndex = ThisBone.ParentIndex;

            FMatrix LocalBindMatrix;
            if (ParentIndex == -1)
            {
                LocalBindMatrix = ThisBone.BindPose;
            }
            else
            {
                const FMatrix& ParentInverseBindPose = Skeleton->Bones[ParentIndex].InverseBindPose;
                LocalBindMatrix = ThisBone.BindPose * ParentInverseBindPose;
            }

            LocalSpacePose[BoneIndex] = FTransform(LocalBindMatrix);
        }
    }

    int32 GetNumBones() const { return static_cast<int32>(LocalSpacePose.Num()); }
};

struct FAnimExtractContext
{
    float CurrentTime = 0.f;
    float DeltaTime = 0.f;
    float PlayRate = 1.f;
    bool bLooping = true;
    bool bEnableInterpolation = true;
    EAdditiveType AdditiveType = EAdditiveType::None;
    float ReferenceTime = 0.f;

    void Advance(float InDeltaSeconds, float SequenceLength)
    {
        DeltaTime = InDeltaSeconds;
        const float Move = InDeltaSeconds * PlayRate;

        if (SequenceLength <= 0.f)
        {
            CurrentTime = 0.f;
            return;
        }

        CurrentTime += Move;

        if (bLooping)
        {
            float T = std::fmod(CurrentTime, SequenceLength);
            if (T < 0.f) T += SequenceLength;
            CurrentTime = T;
        }
        else
        {
            if (CurrentTime < 0.f) CurrentTime = 0.f;
            if (CurrentTime > SequenceLength) CurrentTime = SequenceLength;
        }
    }
};

// Minimal base for graph-like animation nodes
struct FAnimNode_Base
{
    virtual ~FAnimNode_Base() = default;

    // Called before Evaluate to advance internal state based on DeltaSeconds
    virtual void Update(FAnimationBaseContext& /*Context*/) {}

    // Fills Output.LocalSpacePose. Default: ref pose.
    virtual void Evaluate(FPoseContext& Output)
    {
        Output.ResetToRefPose();
    }
};

