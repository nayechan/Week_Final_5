#pragma once
#include "SceneComponent.h"
#include "Material.h"

// 전방 선언
struct FSceneCompData;

class URenderer;

class UPrimitiveComponent :public USceneComponent
{
public:
    DECLARE_CLASS(UPrimitiveComponent, USceneComponent)

    UPrimitiveComponent() = default;
    virtual ~UPrimitiveComponent() = default;

    virtual void SetMaterial(const FString& FilePath, EVertexLayoutType layoutType);
    virtual UMaterial* GetMaterial() { return Material; }

    //// 트랜스폼 직렬화/역직렬화 (월드 트랜스폼 기준)
    //virtual void Serialize(bool bIsLoading, FSceneCompData& InOut);

    virtual void Render(URenderer* Renderer, const FMatrix& View, const FMatrix& Proj) {}

    void SetCulled(bool InCulled)
    {
        bIsCulled = InCulled;
    }

    bool GetCulled() const
    {
        return bIsCulled;
    }

    // ───── 복사 관련 ────────────────────────────
    void DuplicateSubObjects() override;
    DECLARE_DUPLICATE(UPrimitiveComponent)

protected:
    UMaterial* Material = nullptr;
    bool bIsCulled = false;

};