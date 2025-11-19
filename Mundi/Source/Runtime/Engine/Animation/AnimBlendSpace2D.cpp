#include "pch.h"
#include "AnimBlendSpace2D.h"
#include "AnimSequenceBase.h"
#include "AnimationRuntime.h"

int32 FAnimNode_BlendSpace2D::AddSample(const FVector2D& Pos, UAnimSequenceBase* Seq, float RateScale, bool bLooping)
{
    FBlendSample2D S;
    S.Position = Pos;
    S.Sequence = Seq;
    S.RateScale = RateScale;
    S.bLooping = bLooping;
    const int32 Index = Samples.Add(S);
    return Index;
}

void FAnimNode_BlendSpace2D::AddTriangle(int32 I0, int32 I1, int32 I2)
{
    FBlendTriangle T; T.I0 = I0; T.I1 = I1; T.I2 = I2;
    Triangles.Add(T);
}

void FAnimNode_BlendSpace2D::Clear()
{
    Samples.Empty();
    Triangles.Empty();
    NormalizedTime = 0.f;
    PlayRate = 1.f;
    bLooping = true;
    DriverSampleIndex = 0;
    BlendPosition = FVector2D(0.f, 0.f);
}

void FAnimNode_BlendSpace2D::Update(FAnimationBaseContext& Context)
{
    if (!Context.IsValid()) return;
    const float Dt = Context.GetDeltaSeconds();
    if (bLooping)
    {
        NormalizedTime += Dt * PlayRate;
        // wrap 0..1
        NormalizedTime = std::fmod(NormalizedTime, 1.0f);
        if (NormalizedTime < 0.f) NormalizedTime += 1.0f;
    }
    else
    {
        NormalizedTime += Dt * PlayRate;
        if (NormalizedTime < 0.f) NormalizedTime = 0.f;
        if (NormalizedTime > 1.f) NormalizedTime = 1.f;
    }
}

bool FAnimNode_BlendSpace2D::FindBestTriangle(const FVector2D& P, FTrianglePick& OutPick) const
{
    OutPick = FTrianglePick{};
    float BestOutsidePenalty = std::numeric_limits<float>::max();
    int32 BestIdx = -1; float BestU=0, BestV=0, BestW=0; bool BestInside=false;

    for (int32 t = 0; t < Triangles.Num(); ++t)
    {
        const FBlendTriangle& T = Triangles[t];
        if (T.I0 < 0 || T.I1 < 0 || T.I2 < 0) continue;
        if (T.I0 >= Samples.Num() || T.I1 >= Samples.Num() || T.I2 >= Samples.Num()) continue;

        const FVector2D A = Samples[T.I0].Position;
        const FVector2D B = Samples[T.I1].Position;
        const FVector2D C = Samples[T.I2].Position;

        float u=0, v=0, w=0;
        if (!AnimBlendMath::ComputeBarycentric(P, A, B, C, u, v, w))
        {
            continue; // degenerate triangle
        }

        const bool bInside = AnimBlendMath::IsInsideTriangle(u, v, w);
        if (bInside)
        {
            OutPick.TriangleIndex = t; OutPick.U = u; OutPick.V = v; OutPick.W = w; OutPick.bInside = true; return true;
        }
        // Compute a simple outside penalty: sum of negative parts magnitude
        float penalty = 0.f;
        if (u < 0.f) penalty += -u;
        if (v < 0.f) penalty += -v;
        if (w < 0.f) penalty += -w;
        if (penalty < BestOutsidePenalty)
        {
            BestOutsidePenalty = penalty; BestIdx = t; BestU = u; BestV = v; BestW = w; BestInside = false;
        }
    }

    if (BestIdx != -1)
    {
        // Clamp weights to edge/vertex
        AnimBlendMath::ClampBarycentricToTriangle(BestU, BestV, BestW);
        OutPick.TriangleIndex = BestIdx; OutPick.U = BestU; OutPick.V = BestV; OutPick.W = BestW; OutPick.bInside = false; return true;
    }

    return false;
}

void FAnimNode_BlendSpace2D::Evaluate(FPoseContext& Output)
{
    const FSkeleton* Skeleton = Output.GetSkeleton();
    if (!Skeleton)
    {
        Output.ResetToRefPose();
        return;
    }

    // Fallbacks for small configurations
    if (Samples.Num() <= 0)
    {
        Output.ResetToRefPose();
        return;
    }

    if (Triangles.Num() <= 0 || Samples.Num() < 3)
    {
        // Use nearest single sample
        int32 Best = -1; float bestDist2 = std::numeric_limits<float>::max();
        for (int32 i = 0; i < Samples.Num(); ++i)
        {
            const FVector2D d = BlendPosition - Samples[i].Position;
            const float d2 = d.X*d.X + d.Y*d.Y;
            if (d2 < bestDist2) { bestDist2 = d2; Best = i; }
        }
        if (Best >= 0 && Samples[Best].Sequence)
        {
            FAnimExtractContext Ctx; Ctx.CurrentTime = 0.f; // driven below
            Ctx.bLooping = Samples[Best].bLooping; Ctx.bEnableInterpolation = true;
            const float Len = Samples[Best].Sequence->GetPlayLength();
            const float Time = NormalizedTime * Len * std::max(0.f, Samples[Best].RateScale);
            Ctx.CurrentTime = (Ctx.bLooping && Len>0.f) ? std::fmod(Time, Len) : FMath::Clamp(Time, 0.f, Len);
            TArray<FTransform> CompPose;
            FAnimationRuntime::ExtractPoseFromSequence(Samples[Best].Sequence, Ctx, *Skeleton, CompPose);
            FAnimationRuntime::ConvertComponentToLocalSpace(*Skeleton, CompPose, Output.LocalSpacePose);
            return;
        }
        Output.ResetToRefPose();
        return;
    }

    // Find triangle and weights
    FTrianglePick Pick;
    if (!FindBestTriangle(BlendPosition, Pick))
    {
        Output.ResetToRefPose();
        return;
    }

    const FBlendTriangle& T = Triangles[Pick.TriangleIndex];
    const int32 idx[3] = { T.I0, T.I1, T.I2 };
    const float w[3] = { Pick.U, Pick.V, Pick.W };

    // Evaluate three component poses
    TArray<FTransform> CompA, CompB, CompC, CompOut;
    for (int si = 0; si < 3; ++si)
    {
        const FBlendSample2D& S = Samples[idx[si]];
        if (!S.Sequence)
        {
            // If a sequence is missing, treat as ref pose for that corner
            FPoseContext Tmp; Tmp.Initialize(Output.GetComponent(), Skeleton, Output.GetDeltaSeconds());
            Tmp.ResetToRefPose();
            FAnimationRuntime::ConvertLocalToComponentSpace(*Skeleton, Tmp.LocalSpacePose,
                (si==0)?CompA:((si==1)?CompB:CompC));
            continue;
        }

        FAnimExtractContext Ctx; Ctx.bLooping = S.bLooping; Ctx.bEnableInterpolation = true;
        const float Len = S.Sequence->GetPlayLength();
        const float Rate = std::max(0.f, S.RateScale);
        const float Time = NormalizedTime * Len * Rate;
        Ctx.CurrentTime = (Ctx.bLooping && Len>0.f) ? std::fmod(Time, Len) : FMath::Clamp(Time, 0.f, Len);
        TArray<FTransform>& OutComp = (si==0)?CompA:((si==1)?CompB:CompC);
        FAnimationRuntime::ExtractPoseFromSequence(S.Sequence, Ctx, *Skeleton, OutComp);
    }

    // Blend three component poses and convert to local
    FAnimationRuntime::BlendThreePoses(*Skeleton, CompA, CompB, CompC, w[0], w[1], w[2], CompOut);
    FAnimationRuntime::ConvertComponentToLocalSpace(*Skeleton, CompOut, Output.LocalSpacePose);
}

bool FAnimNode_BlendSpace2D::SetSamplePosition(int32 Index, const FVector2D& NewPos)
{
    if (Index < 0 || Index >= Samples.Num()) return false;
    Samples[Index].Position = NewPos;
    return true;
}

bool FAnimNode_BlendSpace2D::SetSampleParams(int32 Index, float NewRateScale, bool bNewLooping)
{
    if (Index < 0 || Index >= Samples.Num()) return false;
    Samples[Index].RateScale = NewRateScale;
    Samples[Index].bLooping = bNewLooping;
    return true;
}
