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

	/** StaticMesh의 BodySetup을 반환합니다 */
	virtual UBodySetup* GetBodySetup() override;

	/** 디버그 충돌체 렌더링 */
	void RenderDebugVolume(class URenderer* Renderer) const override;

	void DuplicateSubObjects() override;

	// ====================================================================

	/** 임시 디버그용 */
	// UBodySetup* BodySetup;

	/** 임시 디버그용 */
	// virtual UBodySetup* GetBodySetup() override { return BodySetup;} 

protected:
	void OnUpdateTransform(EUpdateTransformFlags UpdateTransformFlags, ETeleportType Teleport = ETeleportType::None) override;

protected:
};
