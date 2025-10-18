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

    // Serialize
    void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;

private:
    FString TextureName;
    UQuad* Quad = nullptr;
    float Width = 100.f;
    float Height = 100.f;
};

