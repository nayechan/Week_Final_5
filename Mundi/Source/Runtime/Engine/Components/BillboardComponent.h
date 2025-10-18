#pragma once
#include "PrimitiveComponent.h"
#include "Object.h"

class UQuad;
class UTexture;
class UMaterial;
class URenderer;

class UBillboardComponent : public UPrimitiveComponent
{
public:
    DECLARE_CLASS(UBillboardComponent, UPrimitiveComponent)
    GENERATED_REFLECTION_BODY()

    UBillboardComponent();
    ~UBillboardComponent() override = default;

    // Render override
    void Render(URenderer* Renderer, const FMatrix& View, const FMatrix& Proj) override;

    // Setup
    void SetTextureName( FString TexturePath);
    void SetSize(float InWidth, float InHeight) { Width = InWidth; Height = InHeight; }

    float GetWidth() const { return Width; }
    float GetHeight() const { return Height; }
    UQuad* GetStaticMesh() const { return Quad; }
    FString& GetTextureName() { return TextureName; }

    UMaterial* GetMaterial(uint32 InSectionIndex) const override;
    void SetMaterial(uint32 InElementIndex, const FString& InMaterialName) override;

    // Serialize
    void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;

private:
    FString TextureName;
    UTexture* Texture = nullptr;  // 리플렉션 시스템용 Texture 포인터
    UMaterial* Material = nullptr;
    UQuad* Quad = nullptr;
    float Width = 100.f;
    float Height = 100.f;
};

