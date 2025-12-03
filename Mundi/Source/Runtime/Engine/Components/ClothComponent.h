#pragma once
#include "MeshComponent.h"
#include "UClothComponent.generated.h"

class UClothMeshInstance;

UCLASS(DisplayName = "Cloth", Description = "Cloth")
class UClothComponent : public UMeshComponent
{
	GENERATED_REFLECTION_BODY()
public:
	UClothComponent();
	~UClothComponent();

	void TickComponent(float DeltaTime) override;       // 매 프레임
	void CollectMeshBatches(TArray<FMeshBatchElement>& OutMeshBatchElements, const FSceneView* View) override;

	void DuplicateSubObjects() override;

	void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;

	
	UClothMeshInstance* ClothInstance = nullptr;
};