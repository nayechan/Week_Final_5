#pragma once
#include "AnimInstance.h"
#include "AnimStateMachine.h"
#include "UAnimStateMachineInstance.generated.h"

// Thin UAnimInstance wrapper that hosts a graph node state machine
UCLASS(DisplayName="애님 상태 머신 인스턴스", Description="그래프 노드 기반 상태 머신")
class UAnimStateMachineInstance : public UAnimInstance
{
public:
    GENERATED_REFLECTION_BODY()

    UAnimStateMachineInstance() = default;

    // Authoring API (for convenience)
    int32 AddState(const FAnimState& State, UAnimSequenceBase* Sequence) { return StateMachine.AddState(State, Sequence); }
    void AddTransition(const FAnimTransition& Transition) { StateMachine.AddTransition(Transition); }
    void SetCurrentState(int32 StateIndex, float BlendTime = -1.f) { StateMachine.SetCurrentState(StateIndex, BlendTime); }
    int32 FindStateByName(const FString& Name) const { return StateMachine.FindStateByName(Name); }
    int32 GetCurrentStateIndex() const { return StateMachine.GetCurrentStateIndex(); }
    int32 GetStateCount() const { return StateMachine.GetNumStates(); }
    const FString& GetStateName(int32 Index) const { return StateMachine.GetStateName(Index); }
    bool SetStatePlayRate(int32 Index, float Rate) { return StateMachine.SetStatePlayRate(Index, Rate); }
    bool SetStateLooping(int32 Index, bool bLooping) { return StateMachine.SetStateLooping(Index, bLooping); }
    float GetStateTime(int32 Index) const { return StateMachine.GetStateTime(Index); }
    bool SetStateTime(int32 Index, float TimeSeconds) { return StateMachine.SetStateTime(Index, TimeSeconds); }

    // ==== Lua-facing API (moved from SkeletalMeshComponent) ====
    UFUNCTION(LuaBind, DisplayName="Clear")
    void Clear();

    UFUNCTION(LuaBind, DisplayName="IsActive")
    bool Lua_IsActive() const { return IsPlaying(); }

    UFUNCTION(LuaBind, DisplayName="AddState")
    int32 Lua_AddState(const FString& Name, const FString& AssetPath, float Rate, bool bLooping);

    UFUNCTION(LuaBind, DisplayName="AddTransitionByName")
    void Lua_AddTransitionByName(const FString& FromName, const FString& ToName, float BlendTime);

    UFUNCTION(LuaBind, DisplayName="SetState")
    void Lua_SetState(const FString& Name, float BlendTime);

    UFUNCTION(LuaBind, DisplayName="GetCurrentStateName")
    FString Lua_GetCurrentStateName() const;

    UFUNCTION(LuaBind, DisplayName="GetStateIndex")
    int32 Lua_GetStateIndex(const FString& Name) const;

    UFUNCTION(LuaBind, DisplayName="SetStatePlayRate")
    void Lua_SetStatePlayRate(const FString& Name, float Rate);

    UFUNCTION(LuaBind, DisplayName="SetStateLooping")
    void Lua_SetStateLooping(const FString& Name, bool bLooping);

    UFUNCTION(LuaBind, DisplayName="GetStateTime")
    float Lua_GetStateTime(const FString& Name) const;

    UFUNCTION(LuaBind, DisplayName="SetStateTime")
    void Lua_SetStateTime(const FString& Name, float TimeSeconds);

    // UAnimInstance overrides
    void NativeUpdateAnimation(float DeltaSeconds) override;
    void EvaluateAnimation(FPoseContext& Output) override;
    bool IsPlaying() const override { return StateMachine.IsActive(); }

private:
    FAnimNode_StateMachine StateMachine;
};
