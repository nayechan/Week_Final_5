#pragma once
#include "Object.h"

class UQuad;
class UTexture;
class UMaterial;
class URenderer;

class UBillboardComponent : public UPrimitiveComponent
{
public:
    DECLARE_CLASS(UBillboardComponent, UPrimitiveComponent)

    UBillboardComponent();
    ~UBillboardComponent() override = default;

    // Render override
    void Render(URenderer* Renderer, const FMatrix& View, const FMatrix& Proj) override;

    // Setup
    void SetTexture(const FString& TexturePath);
    void SetSize(float InWidth, float InHeight) { Width = InWidth; Height = InHeight; }

    float GetWidth() const { return Width; }
    float GetHeight() const { return Height; }
    UQuad* GetStaticMesh() const { return Quad; }


private:
    UQuad* Quad = nullptr;
    UMaterial* BillboardMaterial = nullptr;

    float Width = 100.f;
    float Height = 100.f;
};

