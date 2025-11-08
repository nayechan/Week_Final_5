#pragma once

#include "MeshComponent.h"
#include "UTextRenderComponent.generated.h"
class UTextRenderComponent : public UPrimitiveComponent
{
public:

	GENERATED_REFLECTION_BODY()

	UTextRenderComponent();

protected:
	~UTextRenderComponent() override;

public:
	void InitCharInfoMap();
	TArray<FBillboardVertexInfo_GPU> CreateVerticesForString(const FString& text,const FVector& StartPos);

	UQuad* GetStaticMesh() const { return TextQuad; }

	// Serialize
	void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;

	UMaterialInterface* GetMaterial(uint32 InSectionIndex) const override;
	void SetMaterial(uint32 InElementIndex, UMaterialInterface* InNewMaterial) override;

	// ───── 복사 관련 ────────────────────────────
	void DuplicateSubObjects() override;

private:
	FString Text;
	static TMap<char, FBillboardVertexInfo> CharInfoMap; // shared per-process, built once
	FString TextureFilePath;
	UMaterialInterface* Material;
	UQuad* TextQuad = nullptr;
};