#pragma once
#include "MeshComponent.h"
#include "Enums.h"
#include "AABB.h"

class UStaticMesh;
class UShader;
class UTexture;
struct FSceneCompData;

struct FMaterialSlot
{
	FName MaterialName;
	bool bChangedByUser = false; // user에 의해 직접 Material이 바뀐 적이 있는지.
};

class UStaticMeshComponent : public UMeshComponent
{
public:
	DECLARE_CLASS(UStaticMeshComponent, UMeshComponent)
	GENERATED_REFLECTION_BODY()

		UStaticMeshComponent();

protected:
	~UStaticMeshComponent() override;

public:
	void SetViewModeShader(UShader* InShader) override;

	void Render(URenderer* Renderer, const FMatrix& View, const FMatrix& Proj) override;
	void CollectMeshBatches(TArray<FMeshBatchElement>& OutMeshBatchElements, const FSceneView* View) override;

	void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;


	void SetStaticMesh(const FString& PathFileName);

	UStaticMesh* GetStaticMesh() const { return StaticMesh; }
	
	UMaterial* GetMaterial(uint32 InSectionIndex) const override;
	void SetMaterial(uint32 InElementIndex, const FString& InMaterialName) override;

	void SetMaterialByUser(const uint32 InMaterialSlotIndex, const FString& InMaterialName);
	const TArray<UMaterial*> GetMaterialSlots() const { return MaterialSlots; }

	bool IsChangedMaterialByUser() const
	{
		return bChangedMaterialByUser;
	}

	FAABB GetWorldAABB() const;

	void DuplicateSubObjects() override;
	DECLARE_DUPLICATE(UStaticMeshComponent)

protected:
	void OnTransformUpdatedChildImpl() override;
	void MarkWorldPartitionDirty();

protected:
	UStaticMesh* StaticMesh = nullptr;
	TArray<UMaterial*> MaterialSlots;

	bool bChangedMaterialByUser = false;
};
