#include "pch.h"
#include "BillboardComponent.h"

#include "Quad.h"
#include "Material.h"
#include "Shader.h"
#include "Texture.h"
#include "Renderer.h"
#include "ResourceManager.h"
#include "CameraActor.h"
#include "JsonSerializer.h"

IMPLEMENT_CLASS(UBillboardComponent)

UBillboardComponent::UBillboardComponent()
{
	auto& RM = UResourceManager::GetInstance();
	Quad = RM.Get<UQuad>("BillboardQuad");
	if (Quad == nullptr)
	{
		UE_LOG("Quad is nullptr");
		return;
	}

	// HSLS 설정 
	SetMaterial("Shaders/UI/Billboard.hlsl", EVertexLayoutType::PositionBillBoard);
	//Material->SetShader(".hlsl", EVertexLayoutType::PositionColorTexturNormal);

	// 일단 디폴트 텍스쳐로 설정하기 .
	SetTextureName("Data/UI/Icons/Pawn_64x.png");
}


// 기존 작업에서 , Renderer 단계에서 리소스 매니저에 요청해서 찾아오는 작업으로 되어있기에, 
// 일단 텍스쳐 이름만 저장하는 방안으로 두어  랜더링 단게에서 Texture를 로드 하는 방법 선택 ...
// 리팩토링 필요 
void UBillboardComponent::SetTextureName( FString TexturePath)
{

    //if (!Material)
    //    return;
	TextureName = TexturePath;
    // Ensure Material has a texture object to carry the name.
  /*  if (!Material->GetTexture())
    {
        UTexture* TempTex = NewObject<UTexture>();
        Material->SetTexture(TempTex);
    }*/
    //Material->SetTextName(TexturePath);
}

void UBillboardComponent::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	Super::Serialize(bInIsLoading, InOutHandle);

	if (bInIsLoading)
	{
		FString TextureNameTemp;
		float WidthTemp;
		float HeightTemp;

		FJsonSerializer::ReadString(InOutHandle, "TextureName", TextureNameTemp);
		FJsonSerializer::ReadFloat(InOutHandle, "Width", WidthTemp);
		FJsonSerializer::ReadFloat(InOutHandle, "Height", HeightTemp);

		SetTextureName(TextureNameTemp);
		SetSize(WidthTemp, HeightTemp);
	}
	else
	{
		InOutHandle["Width"] = Width;
		InOutHandle["Height"] = Height;
		InOutHandle["TextureName"] = TextureName;
	}
}

// 여기서만 Cull_Back을 꺼야함. 
void UBillboardComponent::Render(URenderer* Renderer, const FMatrix& View, const FMatrix& Proj)
{
	// 빌보드를 위한 업데이트 ! 
	ACameraActor* CameraActor = GetOwner()->GetWorld()->GetCameraActor();
	FVector CamRight = CameraActor->GetActorRight();
	FVector CamUp = CameraActor->GetActorUp();
	FVector cameraPosition = CameraActor->GetActorLocation();
    Renderer->GetRHIDevice()->UpdateBillboardConstantBuffers(Owner->GetActorLocation() + GetRelativeLocation() + FVector(0.f, 0.f, 1.f) * Owner->GetActorScale().Z, View, Proj, CamRight, CamUp);

    Renderer->GetRHIDevice()->PrepareShader(Material->GetShader());
    Renderer->GetRHIDevice()->OMSetDepthStencilState(EComparisonFunc::LessEqual);
	Renderer->GetRHIDevice()->RSSetState(ERasterizerMode::Solid_NoCull);
    Renderer->DrawIndexedPrimitiveComponent(this, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    //Renderer->GetRHIDevice()->RSSetState(EViewModeIndex::VMI_Unlit);
}
