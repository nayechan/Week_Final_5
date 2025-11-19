#pragma once
#include "AnimInstance.h"
#include "AnimBlendSpace2D.h"
#include "UAnimBlendSpaceInstance.generated.h"

// Thin UAnimInstance wrapper hosting a BlendSpace2D node
UCLASS(DisplayName="블렌드 스페이스 2D 인스턴스", Description="2D 파라미터 기반 삼각 보간 블렌드")
class UAnimBlendSpaceInstance : public UAnimInstance
{
public:
    GENERATED_REFLECTION_BODY()

    UAnimBlendSpaceInstance() = default;

    // Authoring/control API (C++)
    int32 AddSample(const FVector2D& Pos, UAnimSequenceBase* Sequence, float RateScale = 1.f, bool bLooping = true) { return BlendSpace.AddSample(Pos, Sequence, RateScale, bLooping); }
    void AddTriangle(int32 I0, int32 I1, int32 I2) { BlendSpace.AddTriangle(I0, I1, I2); }
    void Clear() { BlendSpace.Clear(); }

    void SetBlendPosition(const FVector2D& Pos) { BlendSpace.SetBlendPosition(Pos); }
    FVector2D GetBlendPosition() const { return BlendSpace.GetBlendPosition(); }
    void SetPlayRate(float Rate) { BlendSpace.SetPlayRate(Rate); }
    void SetLooping(bool bInLooping) { BlendSpace.SetLooping(bInLooping); }
    void SetDriverIndex(int32 Idx) { BlendSpace.SetDriverIndex(Idx); }

    const TArray<FBlendSample2D>& GetSamples() const { return BlendSpace.GetSamples(); }
    const TArray<FBlendTriangle>& GetTriangles() const { return BlendSpace.GetTriangles(); }
    bool SetSamplePosition(int32 Index, const FVector2D& Pos) { return BlendSpace.SetSamplePosition(Index, Pos); }
    bool SetSampleParams(int32 Index, float RateScale, bool bLooping) { return BlendSpace.SetSampleParams(Index, RateScale, bLooping); }
    void ClearTriangles() { BlendSpace.ClearTriangles(); }

    // Lua-facing API
    UFUNCTION(LuaBind, DisplayName="Clear")
    void Lua_Clear();

    UFUNCTION(LuaBind, DisplayName="AddSample")
    int32 Lua_AddSample(float X, float Y, const FString& AssetPath, float Rate, bool bLooping);

    UFUNCTION(LuaBind, DisplayName="AddTriangle")
    void Lua_AddTriangle(int32 I0, int32 I1, int32 I2);

    UFUNCTION(LuaBind, DisplayName="SetBlendPosition")
    void Lua_SetBlendPosition(float X, float Y);

    UFUNCTION(LuaBind, DisplayName="GetBlendPosition")
    FVector2D Lua_GetBlendPosition() const;

    UFUNCTION(LuaBind, DisplayName="SetPlayRate")
    void Lua_SetPlayRate(float Rate);

    UFUNCTION(LuaBind, DisplayName="SetLooping")
    void Lua_SetLooping(bool bLooping);

    UFUNCTION(LuaBind, DisplayName="SetDriverIndex")
    void Lua_SetDriverIndex(int32 Idx);

    // UAnimInstance overrides
    void NativeUpdateAnimation(float DeltaSeconds) override;
    void EvaluateAnimation(FPoseContext& Output) override;
    bool IsPlaying() const override { return BlendSpace.GetSamples().Num() > 0; }

private:
    FAnimNode_BlendSpace2D BlendSpace;
};
