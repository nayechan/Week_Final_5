#include "pch.h"
#include "AnimSingleNodeInstance.h"
#include "AnimNodeBase.h"
#include "AnimationRuntime.h"
#include "SkeletalMeshComponent.h"
#include "AnimSequenceBase.h"

void UAnimSingleNodeInstance::SetAnimationAsset(UAnimationAsset* InAsset, bool bInLooping)
{
    UAnimSequenceBase* Seq = Cast<UAnimSequenceBase>(InAsset);
    Player.SetSequence(Seq);
    Player.SetLooping(bInLooping);
    Player.SetPlayRate(1.f);
    Player.SetInterpolationEnabled(true);
    Player.GetExtractContext().CurrentTime = 0.f;
}

void UAnimSingleNodeInstance::Play(bool bResetTime)
{
    bPlaying = true;
    if (bResetTime)
    {
        Player.GetExtractContext().CurrentTime = 0.f;
    }
}

void UAnimSingleNodeInstance::Stop()
{
    bPlaying = false;
}

void UAnimSingleNodeInstance::SetPlaying(bool bInPlaying)
{
    bPlaying = bInPlaying;
}

void UAnimSingleNodeInstance::SetLooping(bool bInLooping)
{
    Player.SetLooping(bInLooping);
}

void UAnimSingleNodeInstance::SetPlayRate(float InRate)
{
    Player.SetPlayRate(InRate);
}

void UAnimSingleNodeInstance::SetPosition(float InSeconds, bool /*bFireNotifies*/)
{
    Player.GetExtractContext().CurrentTime = InSeconds;
}

void UAnimSingleNodeInstance::NativeUpdateAnimation(float DeltaTime)
{
    if (!bPlaying)
    {
        return;
    }
    USkeletalMeshComponent* Comp = GetOwningComponent();
    if (!Comp) return;
    const USkeletalMesh* Mesh = Comp->GetSkeletalMesh();
    const FSkeleton* Skeleton = Mesh ? Mesh->GetSkeleton() : nullptr;
    if (!Skeleton) return;

    FAnimationBaseContext Ctx; Ctx.Initialize(Comp, Skeleton, DeltaTime);
    Player.Update(Ctx);
}

void UAnimSingleNodeInstance::EvaluateAnimation(FPoseContext& Output)
{
    const FSkeleton* Skeleton = Output.Skeleton;
    if (!Skeleton)
    {
        Output.LocalSpacePose.Empty();
        return;
    }

    UAnimSequenceBase* Seq = Player.GetSequence();
    if (!Seq)
    {
        // Fallback: reference pose
        Output.ResetToRefPose();
        return;
    }

    if (!bTreatAssetAsAdditive)
    {
        // Let the player build the pose
        Player.Evaluate(Output);
    }
    else
    {
        // Local-space additive against reference/bind pose
        // 1) Base pose: start from ref pose
        Output.ResetToRefPose();

        // 2) Extract current and reference component poses
        FAnimExtractContext CurrCtx = Player.GetExtractContext();
        FAnimExtractContext RefCtx = CurrCtx;  RefCtx.CurrentTime = ReferenceTime;

        TArray<FTransform> CurrComp, RefComp;
        FAnimationRuntime::ExtractPoseFromSequence(Seq, CurrCtx, *Skeleton, CurrComp);
        FAnimationRuntime::ExtractPoseFromSequence(Seq, RefCtx,  *Skeleton, RefComp);

        // 3) Convert to local-space
        TArray<FTransform> CurrLocal, RefLocal;
        FAnimationRuntime::ConvertComponentToLocalSpace(*Skeleton, CurrComp, CurrLocal);
        FAnimationRuntime::ConvertComponentToLocalSpace(*Skeleton, RefComp,  RefLocal);

        // 4) Compute delta (local) per bone: Ref^-1 * Curr  (use relative helper)
        const int32 NumBones = static_cast<int32>(Skeleton->Bones.Num());
        TArray<FTransform> AdditiveDeltaLocal; AdditiveDeltaLocal.SetNum(NumBones);
        for (int32 i = 0; i < NumBones; ++i)
        {
            AdditiveDeltaLocal[i] = RefLocal[i].GetRelativeTransform(CurrLocal[i]);
        }

        // 5) Accumulate onto base pose
        TArray<FTransform> ResultLocal;
        FAnimationRuntime::AccumulateAdditivePose(*Skeleton, Output.LocalSpacePose, AdditiveDeltaLocal, 1.f, ResultLocal);
        Output.LocalSpacePose = ResultLocal;
    }
}

