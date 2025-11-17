#include "pch.h"
#include "AnimStateMachineInstance.h"
#include "SkeletalMeshComponent.h"
#include "AnimSequence.h"
#include "ResourceManager.h"

void UAnimStateMachineInstance::NativeUpdateAnimation(float DeltaSeconds)
{
    USkeletalMeshComponent* Comp = GetOwningComponent();
    if (!Comp) return;
    const USkeletalMesh* Mesh = Comp->GetSkeletalMesh();
    const FSkeleton* Skeleton = Mesh ? Mesh->GetSkeleton() : nullptr;
    if (!Skeleton) return;

    FAnimationBaseContext Ctx;
    Ctx.Initialize(Comp, Skeleton, DeltaSeconds);
    StateMachine.Update(Ctx);
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
    UAnimSequence* Seq = UResourceManager::GetInstance().Get<UAnimSequence>(AssetPath);
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
        UE_LOG("[AnimStateMachine] Setting current state to '%s' (index %d) with blend time %.2f\n", Name.c_str(), Idx, BlendTime);
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
