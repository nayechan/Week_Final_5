#pragma once
#include "PrimitiveComponent.h"
#include "UMeshComponent.generated.h"

class UShader;

UCLASS(Abstract)
class UMeshComponent : public UPrimitiveComponent
{
public:
    GENERATED_REFLECTION_BODY()

    UMeshComponent();

protected:
    ~UMeshComponent() override;

public:

    // ===== Lua-Bindable Properties (Auto-moved from protected/private) =====

    UPROPERTY(EditAnywhere, Category="Materials", Tooltip="Material slots for the mesh")
    TArray<UMaterialInterface*> MaterialSlots;

    UPROPERTY(EditAnywhere, Category="Rendering", Tooltip="그림자를 드리울지 여부입니다")
    bool bCastShadows = true;
    void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;
    
    void DuplicateSubObjects() override;

protected:
    void MarkWorldPartitionDirty();

// Material Section
public:
    UMaterialInterface* GetMaterial(uint32 InSectionIndex) const override;
    void SetMaterial(uint32 InElementIndex, UMaterialInterface* InNewMaterial) override;

    UMaterialInstanceDynamic* CreateAndSetMaterialInstanceDynamic(uint32 ElementIndex);

    const TArray<UMaterialInterface*>& GetMaterialSlots() const { return MaterialSlots; }

    void SetMaterialTextureByUser(const uint32 InMaterialSlotIndex, EMaterialTextureSlot Slot, UTexture* Texture);
   	UFUNCTION(LuaBind, DisplayName="SetColor", Tooltip="Set material color parameter")
    void SetMaterialColorByUser(const uint32 InMaterialSlotIndex, const FString& ParameterName, const FLinearColor& Value);
    UFUNCTION(LuaBind, DisplayName="SetScalar", Tooltip="Set material scalar parameter")
    void SetMaterialScalarByUser(const uint32 InMaterialSlotIndex, const FString& ParameterName, float Value);
    
protected:
    void ClearDynamicMaterials();
    
    TArray<UMaterialInstanceDynamic*> DynamicMaterialInstances;

// Shadow Section
public:
    bool IsCastShadows() const { return bCastShadows; }

private:
};