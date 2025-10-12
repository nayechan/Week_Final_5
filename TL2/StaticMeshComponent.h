#pragma once
#include "MeshComponent.h"
#include "Enums.h"
#include "AABB.h"

class UStaticMesh;
class UShader;
class UTexture;
struct FPrimitiveData;

struct FMaterialSlot
{
    FName MaterialName;
    bool bChangedByUser = false; // user에 의해 직접 Material이 바뀐 적이 있는지.
};

class UStaticMeshComponent : public UMeshComponent
{
public:
    DECLARE_CLASS(UStaticMeshComponent, UMeshComponent)
    UStaticMeshComponent();

protected:
    ~UStaticMeshComponent() override;

public:
    void Render(URenderer* Renderer, const FMatrix& View, const FMatrix& Proj) override;

    void SetStaticMesh(const FString& PathFileName);
    UStaticMesh* GetStaticMesh() const { return StaticMesh; }

    // 씬 포맷(FPrimitiveData)을 이용한 컴포넌트 직렬화/역직렬화
    // - bIsLoading == true  : InOut로부터 읽어서 컴포넌트 상태(메시) 설정
    // - bIsLoading == false : 컴포넌트 상태를 InOut에 기록
    void Serialize(bool bIsLoading, FPrimitiveData& InOut);

    void SetMaterialByUser(const uint32 InMaterialSlotIndex, const FString& InMaterialName);

    const TArray<FMaterialSlot>& GetMaterialSlots() const { return MaterialSlots; }

    bool IsChangedMaterialByUser() const
    {
        return bChangedMaterialByUser;
    }

    FAABB GetWorldAABB() const;
    
    // ───── 복사 관련 ────────────────────────────
    void DuplicateSubObjects() override;
    DECLARE_DUPLICATE(UStaticMeshComponent)
    
protected:
    void OnTransformUpdatedChildImpl() override;
    void MarkWorldPartitionDirty();

protected:
    UStaticMesh* StaticMesh = nullptr;
    TArray<FMaterialSlot> MaterialSlots;

    bool bChangedMaterialByUser = false;
};

