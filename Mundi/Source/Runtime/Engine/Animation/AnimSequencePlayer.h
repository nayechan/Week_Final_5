#pragma once
#include "AnimNodeBase.h"

class UAnimSequenceBase;

// Simple sequence player node that advances time and evaluates a single sequence
struct FAnimNode_SequencePlayer : public FAnimNode_Base
{
public:
    // Authoring/setup API
    void SetSequence(UAnimSequenceBase* InSequence) { Sequence = InSequence; }
    void SetPlayRate(float InPlayRate) { ExtractCtx.PlayRate = InPlayRate; }
    void SetLooping(bool bInLooping) { ExtractCtx.bLooping = bInLooping; }
    void SetInterpolationEnabled(bool bEnable) { ExtractCtx.bEnableInterpolation = bEnable; }
    void SetAdditive(EAdditiveType InType, float InReferenceTime = 0.f)
    {
        ExtractCtx.AdditiveType = InType;
        ExtractCtx.ReferenceTime = InReferenceTime;
    }

    UAnimSequenceBase* GetSequence() const { return Sequence; }

    // FAnimNode_Base overrides
    void Update(FAnimationBaseContext& Context) override;
    void Evaluate(FPoseContext& Output) override;

    // For external control if desired
    FAnimExtractContext& GetExtractContext() { return ExtractCtx; }
    const FAnimExtractContext& GetExtractContext() const { return ExtractCtx; }

private:
    UAnimSequenceBase* Sequence = nullptr;
    FAnimExtractContext ExtractCtx;
};

