#pragma once
#include "AnimInstance.h"
#include "AnimSequencePlayer.h"
#include "UAnimSingleNodeInstance.generated.h"

class UAnimationAsset;
class UAnimSequenceBase;

UCLASS(DisplayName="싱글 노드 애니 인스턴스", Description="단일 시퀀스 재생기")
class UAnimSingleNodeInstance : public UAnimInstance
{
public:
    GENERATED_REFLECTION_BODY()

    UAnimSingleNodeInstance() = default;

    // Control API
    void SetAnimationAsset(UAnimationAsset* InAsset, bool bInLooping = true);
    void Play(bool bResetTime = true);
    void Stop();
    void SetPlaying(bool bInPlaying);
    void SetLooping(bool bInLooping);
    void SetPlayRate(float InRate);
    void SetPosition(float InSeconds, bool bFireNotifies = false);
    float GetPosition() const { return Player.GetExtractContext().CurrentTime; }
    bool IsPlaying() const override { return bPlaying; }

    // UAnimInstance overrides
    void NativeUpdateAnimation(float DeltaTime) override;
    void EvaluateAnimation(FPoseContext& Output) override;

    // Additive options (optional for now)
    void SetTreatAsAdditive(bool bInAdditive) { bTreatAssetAsAdditive = bInAdditive; }
    void SetAdditiveType(EAdditiveType InType) { AdditiveType = InType; }
    void SetReferenceTime(float InRefTime) { ReferenceTime = InRefTime; }

private:
    FAnimNode_SequencePlayer Player; // drives playback (time/loop/rate)
    bool bPlaying = false;

    // Additive
    bool bTreatAssetAsAdditive = false;
    EAdditiveType AdditiveType = EAdditiveType::LocalSpace;
    float ReferenceTime = 0.f;
};
