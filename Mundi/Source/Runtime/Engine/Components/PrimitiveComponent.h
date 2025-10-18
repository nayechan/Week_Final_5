#pragma once
#include "SceneComponent.h"
#include "Material.h"

// 전방 선언
struct FSceneCompData;

class URenderer;
class FMeshBatchElement;
class FSceneView;

class UPrimitiveComponent :public USceneComponent
{
public:
    DECLARE_CLASS(UPrimitiveComponent, USceneComponent)

    UPrimitiveComponent() = default;
    virtual ~UPrimitiveComponent() = default;

    virtual void SetMaterial(const FString& FilePath);
    virtual UMaterial* GetMaterial() { return Material; }

    // 이 프리미티브를 렌더링하는 데 필요한 FMeshBatchElement를 수집합니다.
    virtual void CollectMeshBatches(TArray<FMeshBatchElement>& OutMeshBatchElements, const FSceneView* View) {}

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

    // ───── 직렬화 ────────────────────────────
    virtual void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;

protected:
    UMaterial* Material = nullptr;
    bool bIsCulled = false;
};