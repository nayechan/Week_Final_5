#include "pch.h"
#include "BillboardComponent.h"

#include "Quad.h"
#include "Material.h"
#include "Shader.h"
#include "Texture.h"
#include "Renderer.h"
#include "ResourceManager.h"
#include "CameraActor.h"

UBillboardComponent::UBillboardComponent()
{
	auto& RM = UResourceManager::GetInstance();
	Quad = RM.Get<UQuad>("BillboardQuad");

    if (auto* M = RM.Get<UMaterial>("BillboardQuad"))
    {
        Material = M;
    }
    else
    {

        Material = NewObject<UMaterial>();
        RM.Add<UMaterial>("BillboardQuad", Material);
    }
}

void UBillboardComponent::SetTexture(const FString& TexturePath)
{
  
}

void UBillboardComponent::Render(URenderer* Renderer, const FMatrix& View, const FMatrix& Proj)
{
  
}

