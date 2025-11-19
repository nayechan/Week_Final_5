#pragma once

struct FAnimExtractContext;
class UAnimSequenceBase;

class FAnimationRuntime
{
public:
    // pose building/conversion
    static void ConvertLocalToComponentSpace(const FSkeleton& Skeleton, const TArray<FTransform>& LocalPose,
        TArray<FTransform>& OutComponentPose);
    static void ConvertComponentToLocalSpace(const FSkeleton& Skeleton, const TArray<FTransform>& ComponentPose,
        TArray<FTransform>& OutLocalPose);

    // extraction
    static void ExtractPoseFromSequence(const UAnimSequenceBase* Sequence, const FAnimExtractContext& ExtractContext,
        const FSkeleton& Skeleton, TArray<FTransform>& OutComponentPose);

    // blending
    static void BlendTwoPoses(const FSkeleton& Skeleton, const TArray<FTransform>& ComponentPoseA, const TArray<FTransform>& ComponentPoseB,
        float Alpha, TArray<FTransform>& OutComponentPose);
    static void AccumulateAdditivePose(const FSkeleton& Skeleton, const TArray<FTransform>& BasePose,
        const TArray<FTransform>& AdditivePose, float Weight, TArray<FTransform>& OutAdditivePose);

    static void NormalizeRotations(TArray<FTransform>& InComponentPose);

    // Multi-pose blending (component space): blends N component-space poses with weights that should sum to 1.
    // Rotation uses weighted quaternion average with antipodal sign correction; translation/scale use weighted linear sum.
    static void BlendMultiplePoses(const FSkeleton& Skeleton,
        const TArray<TArray<FTransform>>& ComponentPoses,
        const TArray<float>& Weights,
        TArray<FTransform>& OutComponentPose);

    // Convenience: blend exactly three component-space poses with explicit weights.
    static void BlendThreePoses(const FSkeleton& Skeleton,
        const TArray<FTransform>& A,
        const TArray<FTransform>& B,
        const TArray<FTransform>& C,
        float WA, float WB, float WC,
        TArray<FTransform>& OutComponentPose);
};
