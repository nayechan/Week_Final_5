#pragma once

#include "MeshComponent.h"
#include "AABB.h"
#include "UStaticMeshComponent.generated.h"

class UStaticMesh;
class UShader;
class UTexture;
class UMaterialInterface;
class UMaterialInstanceDynamic;
struct FSceneCompData;

UCLASS(DisplayName="스태틱 메시 컴포넌트", Description="정적 메시를 렌더링하는 컴포넌트입니다")
class UStaticMeshComponent : public UMeshComponent
{
public:

	GENERATED_REFLECTION_BODY()

	UStaticMeshComponent();

protected:
	~UStaticMeshComponent() override;

public:

    // ===== Lua-Bindable Properties (Auto-moved from protected/private) =====

	UPROPERTY(EditAnywhere, Category="Static Mesh", Tooltip="Static mesh asset to render")
	UStaticMesh* StaticMesh = nullptr;
	void OnStaticMeshReleased(UStaticMesh* ReleasedMesh);

	void CollectMeshBatches(TArray<FMeshBatchElement>& OutMeshBatchElements, const FSceneView* View) override;

	void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;

	void SetStaticMesh(const FString& PathFileName);

	UStaticMesh* GetStaticMesh() const { return StaticMesh; }
	
	FAABB GetWorldAABB() const override;

	void DuplicateSubObjects() override;

protected:
	void OnTransformUpdated() override;

public:
	UPROPERTY(EditAnywhere, Category="Physics", DisplayName="충돌 모드")
	ECollisionShapeMode ShapeMode = ECollisionShapeMode::Simple;
	
protected:
	void CreatePhysicsState() override;
	UBodySetup* GetBodySetup() override;
};
