#include "pch.h"
#include "AnimBlendSpaceInstance.h"
#include "SkeletalMeshComponent.h"
#include "AnimSequence.h"
#include "ResourceManager.h"

void UAnimBlendSpaceInstance::NativeUpdateAnimation(float DeltaSeconds)
{
    USkeletalMeshComponent* Comp = GetOwningComponent();
    if (!Comp) return;
    const USkeletalMesh* Mesh = Comp->GetSkeletalMesh();
    const FSkeleton* Skeleton = Mesh ? Mesh->GetSkeleton() : nullptr;
    if (!Skeleton) return;

    FAnimationBaseContext Ctx;
    Ctx.Initialize(Comp, Skeleton, DeltaSeconds);
    BlendSpace.Update(Ctx);
}

void UAnimBlendSpaceInstance::EvaluateAnimation(FPoseContext& Output)
{
    BlendSpace.Evaluate(Output);
}

// ==== Lua-facing API implementations ====
void UAnimBlendSpaceInstance::Lua_Clear()
{
    Clear();
}

int32 UAnimBlendSpaceInstance::Lua_AddSample(float X, float Y, const FString& AssetPath, float Rate, bool bLooping)
{
    UAnimSequence* Seq = UResourceManager::GetInstance().Get<UAnimSequence>(AssetPath);
    if (!Seq)
    {
        const auto& AllAnims = UResourceManager::GetInstance().GetAnimations();
        for (const UAnimSequence* Anim : AllAnims)
        {
            if (Anim)
            {
                UE_LOG("[BlendSpace2D]   - %s\n", Anim->GetFilePath().c_str());
            }
        }
        return -1;
    }
    return AddSample(FVector2D(X, Y), Seq, Rate, bLooping);
}

void UAnimBlendSpaceInstance::Lua_AddTriangle(int32 I0, int32 I1, int32 I2)
{
    AddTriangle(I0, I1, I2);
}

void UAnimBlendSpaceInstance::Lua_SetBlendPosition(float X, float Y)
{
    SetBlendPosition(FVector2D(X, Y));
}

FVector2D UAnimBlendSpaceInstance::Lua_GetBlendPosition() const
{
    return GetBlendPosition();
}

void UAnimBlendSpaceInstance::Lua_SetPlayRate(float Rate)
{
    SetPlayRate(Rate);
}

void UAnimBlendSpaceInstance::Lua_SetLooping(bool bLooping)
{
    SetLooping(bLooping);
}

void UAnimBlendSpaceInstance::Lua_SetDriverIndex(int32 Idx)
{
    SetDriverIndex(Idx);
}

