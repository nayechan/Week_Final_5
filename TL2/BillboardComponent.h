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
    void SetTextureName( FString TexturePath);
    void SetSize(float InWidth, float InHeight) { Width = InWidth; Height = InHeight; }

    float GetWidth() const { return Width; }
    float GetHeight() const { return Height; }
    UQuad* GetStaticMesh() const { return Quad; }
    FString& GetTextureName() { return TextureName; }

private:
    FString TextureName;

    // 버텍스랑 인덱스 정보 
    UQuad* Quad = nullptr;

    // 텍스쳐 & 셰이더 

    float Width = 100.f;
    float Height = 100.f;
};

