#pragma once
#include "MeshComponent.h"
class UTextRenderComponent : public UPrimitiveComponent
{
public:
	DECLARE_CLASS(UTextRenderComponent, UPrimitiveComponent)
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

	// ───── 복사 관련 ────────────────────────────
	void DuplicateSubObjects() override;
	DECLARE_DUPLICATE(UTextRenderComponent)

private:
	FString Text;
	static TMap<char, FBillboardVertexInfo> CharInfoMap; // shared per-process, built once
	FString TextureFilePath;

	// TODO: UStaticMesh는 UStaticMeshComponent만 사용하도록 바꿔야 한다
	UQuad* TextQuad = nullptr;
};