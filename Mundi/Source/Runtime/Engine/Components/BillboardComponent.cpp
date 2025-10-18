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
#include "LightComponentBase.h"

IMPLEMENT_CLASS(UBillboardComponent)

BEGIN_PROPERTIES(UBillboardComponent)
	MARK_AS_COMPONENT("빌보드 컴포넌트", "항상 카메라를 향하는 2D 아이콘을 표시합니다.")
	ADD_PROPERTY_TEXTURE(UTexture*, Texture, "Billboard", true, "빌보드 텍스처입니다.")
	ADD_PROPERTY_RANGE(float, Width, "Billboard", 1.0f, 1000.0f, true, "빌보드 너비입니다.")
	ADD_PROPERTY_RANGE(float, Height, "Billboard", 1.0f, 1000.0f, true, "빌보드 높이입니다.")
END_PROPERTIES()

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
	SetMaterial(0, "Shaders/UI/Billboard.hlsl");

	// 일단 디폴트 텍스쳐로 설정하기 .
	SetTextureName("Data/UI/Icons/Pawn_64x.png");
}

void UBillboardComponent::SetTextureName( FString TexturePath)
{
	TextureName = TexturePath;
	Texture = UResourceManager::GetInstance().Load<UTexture>(TexturePath);
	if (Texture)
	{
		Texture->SetTextureName(TexturePath);
	}
}

UMaterial* UBillboardComponent::GetMaterial(uint32 InSectionIndex) const
{
	return Material;
}

void UBillboardComponent::SetMaterial(uint32 InElementIndex, const FString& InMaterialName)
{
	Material = UResourceManager::GetInstance().Load<UMaterial>(InMaterialName);
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

	// 리플렉션 기반 자동 직렬화
	AutoSerialize(bInIsLoading, InOutHandle, UBillboardComponent::StaticClass());
}

// 여기서만 Cull_Back을 꺼야함. 
void UBillboardComponent::Render(URenderer* Renderer, const FMatrix& View, const FMatrix& Proj)
{
	// 빌보드를 위한 업데이트
	ACameraActor* CameraActor = GetOwner()->GetWorld()->GetCameraActor();
	FVector CamRight = CameraActor->GetActorRight();
	FVector CamUp = CameraActor->GetActorUp();
	FVector cameraPosition = CameraActor->GetActorLocation();

    //Renderer->GetRHIDevice()->UpdateBillboardConstantBuffers(Owner->GetActorLocation() + GetRelativeLocation() + FVector(0.f, 0.f, 1.f) * Owner->GetActorScale().Z, View, Proj, CamRight, CamUp);
	//정작 location, view proj만 사용하고 있길래 그냥 Identity넘김
	Renderer->GetRHIDevice()->SetAndUpdateConstantBuffer(BillboardBufferType(
		GetWorldLocation(),
		View,
		Proj,
		View.InverseAffineFast()));
	FLinearColor Color{1,1,1,1};
	if (ULightComponentBase* LightBase = Cast<ULightComponentBase>(this->GetAttachParent()))
	{
		Color = LightBase->GetLightColor();
	}
	Renderer->GetRHIDevice()->SetAndUpdateConstantBuffer(ColorBufferType(Color, this->InternalIndex));


	Renderer->GetRHIDevice()->SetAndUpdateConstantBuffer(BillboardBufferType(GetWorldLocation(), View, Proj, View.InverseAffineFast()));
    Renderer->GetRHIDevice()->PrepareShader(Material->GetShader());
    Renderer->GetRHIDevice()->OMSetDepthStencilState(EComparisonFunc::LessEqual);
	Renderer->GetRHIDevice()->RSSetState(ERasterizerMode::Solid_NoCull);
    Renderer->DrawIndexedPrimitiveComponent(this, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}
