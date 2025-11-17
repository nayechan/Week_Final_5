#include "pch.h"
#include "AnimSequencePlayer.h"
#include "AnimSequenceBase.h"
#include "AnimationRuntime.h"

void FAnimNode_SequencePlayer::Update(FAnimationBaseContext& Context)
{
    if (!Context.IsValid()) return;
    const float Length = Sequence ? Sequence->GetPlayLength() : 0.f;
    ExtractCtx.Advance(Context.GetDeltaSeconds(), Length);
}

void FAnimNode_SequencePlayer::Evaluate(FPoseContext& Output)
{
    const FSkeleton* Skeleton = Output.Skeleton;
    if (!Skeleton)
    {
        Output.LocalSpacePose.Empty();
        return;
    }

    // Build component-space pose and convert to local-space for the output
    TArray<FTransform> ComponentPose;
    FAnimationRuntime::ExtractPoseFromSequence(Sequence, ExtractCtx, *Skeleton, ComponentPose);
    FAnimationRuntime::ConvertComponentToLocalSpace(*Skeleton, ComponentPose, Output.LocalSpacePose);
}

