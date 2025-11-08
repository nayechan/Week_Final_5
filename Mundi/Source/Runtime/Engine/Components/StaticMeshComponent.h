#pragma once

#include "MeshComponent.h"
#include "Enums.h"
#include "AABB.h"
#include "UStaticMeshComponent.generated.h"

class UStaticMesh;
class UShader;
class UTexture;
class UMaterialInterface;
class UMaterialInstanceDynamic;
struct FSceneCompData;

class UStaticMeshComponent : public UMeshComponent
{
public:

	GENERATED_REFLECTION_BODY()

	UStaticMeshComponent();

protected:
	~UStaticMeshComponent() override;
	void ClearDynamicMaterials();

public:
	void OnStaticMeshReleased(UStaticMesh* ReleasedMesh);

	void CollectMeshBatches(TArray<FMeshBatchElement>& OutMeshBatchElements, const FSceneView* View) override;

	void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;

	void SetStaticMesh(const FString& PathFileName);

	UStaticMesh* GetStaticMesh() const { return StaticMesh; }
	
	UMaterialInterface* GetMaterial(uint32 InSectionIndex) const override;
	void SetMaterial(uint32 InElementIndex, UMaterialInterface* InNewMaterial) override;

	UMaterialInstanceDynamic* CreateAndSetMaterialInstanceDynamic(uint32 ElementIndex);

	const TArray<UMaterialInterface*> GetMaterialSlots() const { return MaterialSlots; }

	void SetMaterialTextureByUser(const uint32 InMaterialSlotIndex, EMaterialTextureSlot Slot, UTexture* Texture);

	UFUNCTION(LuaBind, DisplayName="SetColor", Tooltip="Set material color parameter")
	void SetMaterialColorByUser(const uint32 InMaterialSlotIndex, const FString& ParameterName, const FLinearColor& Value);

	UFUNCTION(LuaBind, DisplayName="SetScalar", Tooltip="Set material scalar parameter")
	void SetMaterialScalarByUser(const uint32 InMaterialSlotIndex, const FString& ParameterName, float Value);

	FAABB GetWorldAABB() const override;

	void DuplicateSubObjects() override;

protected:
	void OnTransformUpdated() override;
	void MarkWorldPartitionDirty();

protected:
	UPROPERTY(EditAnywhere, Category="Static Mesh", Tooltip="Static mesh asset to render")
	UStaticMesh* StaticMesh = nullptr;

	UPROPERTY(EditAnywhere, Category="Materials", Tooltip="Material slots for the mesh")
	TArray<UMaterialInterface*> MaterialSlots;

	TArray<UMaterialInstanceDynamic*> DynamicMaterialInstances;
};
