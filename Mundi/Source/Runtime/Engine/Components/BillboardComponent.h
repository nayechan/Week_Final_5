#pragma once

#include "PrimitiveComponent.h"
#include "Object.h"
#include "UBillboardComponent.generated.h"

class UQuad;
class UTexture;
class UMaterial;
class URenderer;

UCLASS(DisplayName="빌보드 컴포넌트", Description="항상 카메라를 향하는 스프라이트 컴포넌트입니다")
class UBillboardComponent : public UPrimitiveComponent
{
public:

    GENERATED_REFLECTION_BODY()

public:

    // ===== Lua-Bindable Properties (Auto-moved from protected/private) =====

    UPROPERTY(EditAnywhere, Category="Billboard")
    UTexture* Texture = nullptr;  // 리플렉션 시스템용 Texture 포인터

    UBillboardComponent();
    ~UBillboardComponent() override = default;

    void CollectMeshBatches(TArray<FMeshBatchElement>& OutMeshBatchElements, const FSceneView* View) override;

    // Setup
    UFUNCTION(LuaBind, DisplayName="SetTexture")
    void SetTexture(FString TexturePath);

    UQuad* GetStaticMesh() const { return Quad; }
    FString& GetFilePath() { return TexturePath; }

    UMaterialInterface* GetMaterial(uint32 InSectionIndex) const override;
    void SetMaterial(uint32 InElementIndex, UMaterialInterface* InNewMaterial) override;

    // Serialize
    void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;

    // Duplication
    void DuplicateSubObjects() override;

private:
    FString TexturePath;


    UMaterialInterface* Material = nullptr;
    UQuad* Quad = nullptr;
};

