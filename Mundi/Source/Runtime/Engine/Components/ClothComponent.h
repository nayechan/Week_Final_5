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

	void BeginPlay() override;
	void TickComponent(float DeltaSeconds) override;
	void EndPlay() override;
	void CollectMeshBatches(TArray<FMeshBatchElement>& OutMeshBatchElements, const FSceneView* View) override;

	void DuplicateSubObjects() override;

	void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;

	UClothMeshInstance* ClothInstance = nullptr;
	
	UPROPERTY(EditAnywhere, DisplayName = "Cloth")
	float Gravity = 9.8f;

	UPROPERTY(EditAnywhere, DisplayName = "Cloth")
	TArray<FVector> Winds;

};