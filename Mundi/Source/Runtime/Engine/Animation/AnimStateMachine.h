#pragma once
#include "AnimNodeBase.h"
#include "AnimSequencePlayer.h"

class UAnimSequenceBase;

// Simple runtime animation state machine types
struct FAnimState
{
    // Authoring inputs (descriptor only)
    FString Name;
    float PlayRate = 1.f;
    bool bLooping = true;

    // Internal player node (owns the sequence and playback state)
    FAnimNode_SequencePlayer Player;
};

struct FAnimTransition
{
    int32 FromStateIndex = -1;
    int32 ToStateIndex = -1;
    float BlendTime = 0.2f;
    bool bAutomatic = false;
};

struct FAnimExtractContext; // fwd decl

struct FAnimSMRuntime
{
    int32 CurrentState = -1;
    int32 NextState = -1;
    float BlendAlpha = 0.f;
    float BlendDuration = 0.f;
};

// Graph node: state machine
struct FAnimNode_StateMachine : public FAnimNode_Base
{
    // Maintenance
    void Reset();
    // Authoring API
    int32 AddState(const FAnimState& State, UAnimSequenceBase* Sequence);
    void AddTransition(const FAnimTransition& Transition);
    void SetCurrentState(int32 StateIndex, float BlendTime = -1.f);
    int32 FindStateByName(const FString& Name) const;

    // Introspection / controls
    int32 GetCurrentStateIndex() const { return Runtime.CurrentState; }
    int32 GetNumStates() const { return States.Num(); }
    const FString& GetStateName(int32 Index) const;
    bool SetStatePlayRate(int32 Index, float Rate);
    bool SetStateLooping(int32 Index, bool bInLooping);
    float GetStateTime(int32 Index) const;
    bool SetStateTime(int32 Index, float TimeSeconds);

    // FAnimNode_Base overrides
    void Update(FAnimationBaseContext& Context) override;
    void Evaluate(FPoseContext& Output) override;

    bool IsActive() const { return (Runtime.CurrentState != -1) || (Runtime.NextState != -1); }

private:
    const FAnimState* GetStateChecked(int32 Index) const;
    const FAnimTransition* FindTransition(int32 FromIndex, int32 ToIndex) const;
    void EnsureTransitionBuckets();

private:
    TArray<FAnimState> States;
    // Flat list kept for tooling/debug; lookups use adjacency buckets
    TArray<FAnimTransition> Transitions;
    // Adjacency: per-from-state list of outgoing transitions
    TArray<TArray<FAnimTransition>> TransitionsByFrom;
    FAnimSMRuntime Runtime;
};
