#include "pch.h"
#include "AnimationRuntime.h"
#include "AnimNodeBase.h"
#include "Vector.h"
#include "VertexData.h"

// Helpers
static void BuildLocalBindPoseFromSkeleton(const FSkeleton& Skeleton, TArray<FTransform>& OutLocalPose)
{
    const int32 NumBones = Skeleton.Bones.Num();
    OutLocalPose.SetNum(NumBones);
    for (int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex)
    {
        const FBone& ThisBone = Skeleton.Bones[BoneIndex];
        const int32 ParentIndex = ThisBone.ParentIndex;
        FMatrix LocalBindMatrix;
        if (ParentIndex == -1)
        {
            LocalBindMatrix = ThisBone.BindPose;
        }
        else
        {
            const FMatrix& ParentInverseBindPose = Skeleton.Bones[ParentIndex].InverseBindPose;
            LocalBindMatrix = ThisBone.BindPose * ParentInverseBindPose;
        }
        OutLocalPose[BoneIndex] = FTransform(LocalBindMatrix);
    }
}

void FAnimationRuntime::ConvertLocalToComponentSpace(const FSkeleton& Skeleton, const TArray<FTransform>& LocalPose,
    TArray<FTransform>& OutComponentPose)
{
    const int32 NumBones = Skeleton.Bones.Num();
    OutComponentPose.SetNum(NumBones);

    for (int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex)
    {
        const int32 ParentIndex = Skeleton.Bones[BoneIndex].ParentIndex;
        if (ParentIndex == -1)
        {
            OutComponentPose[BoneIndex] = LocalPose[BoneIndex];
        }
        else
        {
            OutComponentPose[BoneIndex] = OutComponentPose[ParentIndex].GetWorldTransform(LocalPose[BoneIndex]);
        }
    }
}

void FAnimationRuntime::ConvertComponentToLocalSpace(const FSkeleton& Skeleton, const TArray<FTransform>& ComponentPose,
    TArray<FTransform>& OutLocalPose)
{
    const int32 NumBones = Skeleton.Bones.Num();
    OutLocalPose.SetNum(NumBones);

    for (int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex)
    {
        const int32 ParentIndex = Skeleton.Bones[BoneIndex].ParentIndex;
        if (ParentIndex == -1)
        {
            OutLocalPose[BoneIndex] = ComponentPose[BoneIndex];
        }
        else
        {
            OutLocalPose[BoneIndex] = ComponentPose[ParentIndex].GetRelativeTransform(ComponentPose[BoneIndex]);
        }
    }
}

void FAnimationRuntime::ExtractPoseFromSequence(const UAnimSequenceBase* Sequence, const FAnimExtractContext& ExtractContext,
    const FSkeleton& Skeleton, TArray<FTransform>& OutComponentPose)
{
    const int32 NumBones = Skeleton.Bones.Num();
    if (NumBones <= 0)
    {
        OutComponentPose.Empty();
        return;
    }

    // 1) Extract local pose
    TArray<FTransform> LocalPose;
    LocalPose.SetNum(NumBones);

    if (Sequence)
    {
        Sequence->ExtractBonePose(Skeleton, ExtractContext.CurrentTime, ExtractContext.bLooping, ExtractContext.bEnableInterpolation, LocalPose);
    }
    else
    {
        // Fallback to reference/bind local pose
        BuildLocalBindPoseFromSkeleton(Skeleton, LocalPose);
    }

    // 2) Convert to component space
    ConvertLocalToComponentSpace(Skeleton, LocalPose, OutComponentPose);
}

void FAnimationRuntime::BlendTwoPoses(const FSkeleton& Skeleton, const TArray<FTransform>& ComponentPoseA, const TArray<FTransform>& ComponentPoseB,
    float Alpha, TArray<FTransform>& OutComponentPose)
{
    const int32 NumBones = Skeleton.Bones.Num();
    OutComponentPose.SetNum(NumBones);

    const float ClampedAlpha = std::clamp(Alpha, 0.f, 1.f);

    for (int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex)
    {
        const FTransform& A = ComponentPoseA[BoneIndex];
        const FTransform& B = ComponentPoseB[BoneIndex];
        OutComponentPose[BoneIndex] = FTransform::Lerp(A, B, ClampedAlpha);
    }
}

void FAnimationRuntime::AccumulateAdditivePose(const FSkeleton& Skeleton, const TArray<FTransform>& BasePose,
    const TArray<FTransform>& AdditivePose, float Weight, TArray<FTransform>& OutAdditivePose)
{
    const int32 NumBones = Skeleton.Bones.Num();
    OutAdditivePose.SetNum(NumBones);

    const float W = std::max(0.f, Weight);
    const FQuat Identity(0.f, 0.f, 0.f, 1.f);

    for (int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex)
    {
        const FTransform& Base = BasePose[BoneIndex];
        const FTransform& Delta = AdditivePose[BoneIndex];

        // Rotation: apply weighted delta on top of base
        FQuat RDeltaWeighted = FQuat::Slerp(Identity, Delta.Rotation, W);
        FQuat OutR = RDeltaWeighted * Base.Rotation;
        OutR.Normalize();

        // Translation: linear offset
        FVector OutT = Base.Translation + Delta.Translation * W;

        // Scale: lerp from 1 to delta then component-wise multiply
        const FVector One(1.f, 1.f, 1.f);
        FVector SWeight = FVector::Lerp(One, Delta.Scale3D, W);
        FVector OutS = FVector(Base.Scale3D.X * SWeight.X, Base.Scale3D.Y * SWeight.Y, Base.Scale3D.Z * SWeight.Z);

        OutAdditivePose[BoneIndex] = FTransform(OutT, OutR, OutS);
    }
}

void FAnimationRuntime::NormalizeRotations(TArray<FTransform>& InComponentPose)
{
    for (auto& T : InComponentPose)
    {
        T.Rotation.Normalize();
    }
}

void FAnimationRuntime::BlendMultiplePoses(const FSkeleton& Skeleton,
    const TArray<TArray<FTransform>>& ComponentPoses,
    const TArray<float>& Weights,
    TArray<FTransform>& OutComponentPose)
{
    const int32 NumBones = Skeleton.Bones.Num();
    OutComponentPose.SetNum(NumBones);

    const int32 NumPoses = static_cast<int32>(ComponentPoses.Num());
    if (NumPoses == 0 || NumBones == 0)
    {
        OutComponentPose.Empty();
        return;
    }

    // Early cases
    if (NumPoses == 1)
    {
        OutComponentPose = ComponentPoses[0];
        return;
    }

    // Compute total weight and guard against degenerate input
    float TotalW = 0.f;
    const int32 NumWeights = static_cast<int32>(Weights.Num());
    for (int32 i = 0; i < NumWeights && i < NumPoses; ++i)
    {
        TotalW += std::max(0.f, Weights[i]);
    }
    if (TotalW <= 1e-6f)
    {
        // Fallback: copy first
        OutComponentPose = ComponentPoses[0];
        return;
    }

    // Normalize weights to sum to 1
    TArray<float> NormW; NormW.SetNum(NumPoses);
    for (int32 i = 0; i < NumPoses; ++i)
    {
        const float W = (i < NumWeights) ? std::max(0.f, Weights[i]) : 0.f;
        NormW[i] = W / TotalW;
    }

    for (int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex)
    {
        // Choose reference quaternion from first pose (or first with non-zero weight)
        int32 RefIdx = 0;
        for (int32 i = 0; i < NumPoses; ++i)
        {
            if (NormW[i] > 0.f) { RefIdx = i; break; }
        }

        const FQuat& Qref = ComponentPoses[RefIdx][BoneIndex].Rotation;

        // Weighted quaternion sum with antipodal correction
        float AccX = 0.f, AccY = 0.f, AccZ = 0.f, AccW = 0.f;
        FVector AccT(0.f, 0.f, 0.f);
        FVector AccS(0.f, 0.f, 0.f);

        for (int32 i = 0; i < NumPoses; ++i)
        {
            const float w = NormW[i];
            if (w <= 0.f) continue;

            const FTransform& Ti = ComponentPoses[i][BoneIndex];
            FQuat Qi = Ti.Rotation;
            // Flip sign if needed to avoid averaging antipodal quaternions
            if (FQuat::Dot(Qi, Qref) < 0.f)
            {
                Qi.X = -Qi.X; Qi.Y = -Qi.Y; Qi.Z = -Qi.Z; Qi.W = -Qi.W;
            }

            AccX += Qi.X * w; AccY += Qi.Y * w; AccZ += Qi.Z * w; AccW += Qi.W * w;
            AccT.X += Ti.Translation.X * w; AccT.Y += Ti.Translation.Y * w; AccT.Z += Ti.Translation.Z * w;
            AccS.X += Ti.Scale3D.X * w; AccS.Y += Ti.Scale3D.Y * w; AccS.Z += Ti.Scale3D.Z * w;
        }

        FQuat OutR(AccX, AccY, AccZ, AccW);
        OutR.Normalize();
        const FVector OutT = AccT;
        const FVector OutS = AccS;
        OutComponentPose[BoneIndex] = FTransform(OutT, OutR, OutS);
    }
}

void FAnimationRuntime::BlendThreePoses(const FSkeleton& Skeleton,
    const TArray<FTransform>& A,
    const TArray<FTransform>& B,
    const TArray<FTransform>& C,
    float WA, float WB, float WC,
    TArray<FTransform>& OutComponentPose)
{
    TArray<TArray<FTransform>> Poses;
    Poses.SetNum(3);
    Poses[0] = A; Poses[1] = B; Poses[2] = C;
    TArray<float> Weights; Weights.SetNum(3);
    Weights[0] = WA; Weights[1] = WB; Weights[2] = WC;
    BlendMultiplePoses(Skeleton, Poses, Weights, OutComponentPose);
}
