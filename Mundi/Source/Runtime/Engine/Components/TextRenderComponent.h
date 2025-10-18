#pragma once
#include "MeshComponent.h"
class UTextRenderComponent : public UPrimitiveComponent
{
public:
	DECLARE_CLASS(UTextRenderComponent, UPrimitiveComponent)
	GENERATED_REFLECTION_BODY()

	UTextRenderComponent();

protected:
	~UTextRenderComponent() override;

public:
	void InitCharInfoMap();
	TArray<FBillboardVertexInfo_GPU> CreateVerticesForString(const FString& text,const FVector& StartPos);
	//FResourceData* GetResourceData() { return ResourceData; }
	//FTextureData* GetTextureData() { return TextureData; }
	virtual void Render(URenderer* Renderer, const FMatrix& View, const FMatrix& Proj) override;
	// void SetText(FString Txt);

	UQuad* GetStaticMesh() const { return TextQuad; }

	// Serialize
	void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;

	UMaterial* GetMaterial(uint32 InSectionIndex) const override;
	void SetMaterial(uint32 InElementIndex, const FString& InMaterialName) override;

	// ───── 복사 관련 ────────────────────────────
	void DuplicateSubObjects() override;
	DECLARE_DUPLICATE(UTextRenderComponent)



private:
	FString Text;
	static TMap<char, FBillboardVertexInfo> CharInfoMap; // shared per-process, built once
	FString TextureFilePath;
	UMaterial* Material;
	UQuad* TextQuad = nullptr;
};