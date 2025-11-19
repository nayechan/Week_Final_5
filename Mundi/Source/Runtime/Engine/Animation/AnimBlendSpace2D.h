#pragma once
#include "AnimNodeBase.h"
#include "AnimBlendMath.h"

class UAnimSequenceBase;

// 2D sample placed in blend space parameter plane
struct FBlendSample2D
{
    FVector2D Position = FVector2D(0.f, 0.f);
    UAnimSequenceBase* Sequence = nullptr;
    float RateScale = 1.f;
    bool bLooping = true;
};

// Triangle referencing 3 sample indices
struct FBlendTriangle
{
    int32 I0 = -1;
    int32 I1 = -1;
    int32 I2 = -1;
};

// Graph node: evaluates a 2D triangular blend among up to 3 samples using barycentric weights
struct FAnimNode_BlendSpace2D : public FAnimNode_Base
{
    // Authoring / setup API
    int32 AddSample(const FVector2D& Pos, UAnimSequenceBase* Seq, float RateScale = 1.f, bool bLooping = true);
    void AddTriangle(int32 I0, int32 I1, int32 I2);
    void Clear();

    // Controls
    void SetBlendPosition(const FVector2D& InPos) { BlendPosition = InPos; }
    const FVector2D& GetBlendPosition() const { return BlendPosition; }

    void SetPlayRate(float InRate) { PlayRate = InRate; }
    float GetPlayRate() const { return PlayRate; }

    void SetLooping(bool bInLooping) { bLooping = bInLooping; }
    bool IsLooping() const { return bLooping; }

    void SetDriverIndex(int32 Idx) { DriverSampleIndex = Idx; }
    int32 GetDriverIndex() const { return DriverSampleIndex; }

    const TArray<FBlendSample2D>& GetSamples() const { return Samples; }
    const TArray<FBlendTriangle>& GetTriangles() const { return Triangles; }

    // Edits (for editor tooling)
    bool SetSamplePosition(int32 Index, const FVector2D& NewPos);
    bool SetSampleParams(int32 Index, float NewRateScale, bool bNewLooping);
    void ClearTriangles() { Triangles.Empty(); }

    // FAnimNode_Base overrides
    void Update(FAnimationBaseContext& Context) override;
    void Evaluate(FPoseContext& Output) override;

private:
    struct FTrianglePick
    {
        int32 TriangleIndex = -1;
        float U = 0.f, V = 0.f, W = 0.f; // barycentric weights
        bool bInside = false;
    };

    bool FindBestTriangle(const FVector2D& P, FTrianglePick& OutPick) const;

private:
    // Authoring data
    TArray<FBlendSample2D> Samples;
    TArray<FBlendTriangle> Triangles;

    // Runtime parameters
    FVector2D BlendPosition = FVector2D(0.f, 0.f);
    float NormalizedTime = 0.f; // 0..1 shared clock
    float PlayRate = 1.f;
    bool bLooping = true;
    int32 DriverSampleIndex = 0; // source of normalized time (phase)
};
