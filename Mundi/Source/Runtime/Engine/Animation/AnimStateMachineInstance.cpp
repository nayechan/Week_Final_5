#include "pch.h"
#include "AnimStateMachineInstance.h"
#include "SkeletalMeshComponent.h"
#include "AnimSequence.h"
#include "ResourceManager.h"
#include <filesystem>

void UAnimStateMachineInstance::NativeUpdateAnimation(float DeltaSeconds)
{
    USkeletalMeshComponent* Comp = GetOwningComponent();
    if (!Comp) return;
    const USkeletalMesh* Mesh = Comp->GetSkeletalMesh();
    const FSkeleton* Skeleton = Mesh ? Mesh->GetSkeleton() : nullptr;
    if (!Skeleton) return;

    // 현재 상태의 시간 저장 (Notify용)
    const int32 CurrIdx = StateMachine.GetCurrentStateIndex();
    float CurrentTime = (CurrIdx >= 0) ? StateMachine.GetStateTime(CurrIdx) : 0.f;

    // StateMachine 업데이트
    FAnimationBaseContext Ctx;
    Ctx.Initialize(Comp, Skeleton, DeltaSeconds);
    StateMachine.Update(Ctx);

    // Notify 트리거 (현재 상태만)
    if (CurrIdx >= 0)
    {
        float NewTime = StateMachine.GetStateTime(CurrIdx);
        TriggerNotifiesForState(CurrIdx, PreviousTime, NewTime);
        PreviousTime = NewTime;
    }
}

void UAnimStateMachineInstance::EvaluateAnimation(FPoseContext& Output)
{
    StateMachine.Evaluate(Output);
}


// ==== Lua-facing API implementations ====
void UAnimStateMachineInstance::Clear()
{
    StateMachine.Reset();
}

int32 UAnimStateMachineInstance::Lua_AddState(const FString& Name, const FString& AssetPath, float Rate, bool bLooping)
{
    UAnimSequence* Seq = UResourceManager::GetInstance().Load<UAnimSequence>(AssetPath);
    if (!Seq)
    {
        const auto& AllAnims = UResourceManager::GetInstance().GetAnimations();
        for (const UAnimSequence* Anim : AllAnims)
        {
            if (Anim)
            {
                UE_LOG("[AnimStateMachine]   - %s\n", Anim->GetFilePath().c_str());
            }
        }
        return -1;
    }

    // .animsequence 파일에서 NotifyTracks 로드 시도
    // AssetPath에서 파일명 추출하여 Data/Animations/ 폴더에서 .animsequence 파일 검색
    std::filesystem::path FbxPath(AssetPath);
    FString AnimName = FbxPath.stem().string();  // 확장자 제외한 파일명
    FString AnimSequencePath = "Data/Animations/" + AnimName + ".animsequence";

    if (std::filesystem::exists(AnimSequencePath))
    {
        if (Seq->LoadAnimSequenceFile(AnimSequencePath))
        {
            UE_LOG("[AnimStateMachine] Loaded .animsequence file: %s (NotifyTracks: %d)\n",
                AnimSequencePath.c_str(), Seq->GetNotifyTracks().Num());
        }
    }

    UE_LOG("[AnimStateMachine] Adding state '%s' with asset '%s', PlayLength=%.2f\n", Name.c_str(), AssetPath.c_str(), Seq->GetPlayLength());
    FAnimState StateDesc; StateDesc.Name = Name; StateDesc.PlayRate = Rate; StateDesc.bLooping = bLooping;
    int32 Index = AddState(StateDesc, Seq);
    UE_LOG("[AnimStateMachine] State '%s' added at index %d\n", Name.c_str(), Index);
    return Index;
}

void UAnimStateMachineInstance::Lua_AddTransitionByName(const FString& FromName, const FString& ToName, float BlendTime)
{
    const int32 From = FindStateByName(FromName);
    const int32 To = FindStateByName(ToName);
    if (From < 0 || To < 0) return;
    FAnimTransition T; T.FromStateIndex = From; T.ToStateIndex = To; T.BlendTime = BlendTime;
    AddTransition(T);
}

void UAnimStateMachineInstance::Lua_SetState(const FString& Name, float BlendTime)
{
    const int32 Idx = FindStateByName(Name);
    if (Idx >= 0)
    {
        //UE_LOG("[AnimStateMachine] Setting current state to '%s' (index %d) with blend time %.2f\n", Name.c_str(), Idx, BlendTime);
        SetCurrentState(Idx, BlendTime);
    }
    else
    {
        UE_LOG("[AnimStateMachine] Failed to find state '%s'\n", Name.c_str());
    }
}

FString UAnimStateMachineInstance::Lua_GetCurrentStateName() const
{
    const int32 Curr = GetCurrentStateIndex();
    return (Curr >= 0) ? GetStateName(Curr) : "";
}

int32 UAnimStateMachineInstance::Lua_GetStateIndex(const FString& Name) const
{
    return FindStateByName(Name);
}

void UAnimStateMachineInstance::Lua_SetStatePlayRate(const FString& Name, float Rate)
{
    const int32 Idx = FindStateByName(Name);
    if (Idx >= 0) SetStatePlayRate(Idx, Rate);
}

void UAnimStateMachineInstance::Lua_SetStateLooping(const FString& Name, bool bLooping)
{
    const int32 Idx = FindStateByName(Name);
    if (Idx >= 0) SetStateLooping(Idx, bLooping);
}

float UAnimStateMachineInstance::Lua_GetStateTime(const FString& Name) const
{
    const int32 Idx = FindStateByName(Name);
    return (Idx >= 0) ? GetStateTime(Idx) : 0.f;
}

void UAnimStateMachineInstance::Lua_SetStateTime(const FString& Name, float TimeSeconds)
{
    const int32 Idx = FindStateByName(Name);
    if (Idx >= 0) SetStateTime(Idx, TimeSeconds);
}

float UAnimStateMachineInstance::Lua_GetStateLength(const FString& Name) const
{
    const int32 Idx = FindStateByName(Name);
    return (Idx >= 0) ? StateMachine.GetStateLength(Idx) : 0.f;
}

void UAnimStateMachineInstance::TriggerNotifiesForState(int32 StateIndex, float PrevTime, float CurrTime)
{
    const FAnimState* State = StateMachine.GetStateChecked(StateIndex);
    if (!State) return;

    UAnimSequence* Seq = Cast<UAnimSequence>(State->Player.GetSequence());
    if (!Seq) return;

    USkeletalMeshComponent* Comp = GetOwningComponent();
    if (!Comp) return;

    // 루핑 처리: 시간이 되돌아갔으면 0부터 다시 체크
    if (CurrTime < PrevTime)
    {
        PrevTime = 0.f;
    }

    for (const FNotifyTrack& Track : Seq->GetNotifyTracks())
    {
        for (const FAnimNotifyEvent& Notify : Track.Notifies)
        {
            if (Notify.TriggerTime > PrevTime && Notify.TriggerTime <= CurrTime)
            {
                Comp->TriggerAnimNotify(Notify);
            }
        }
    }
}
