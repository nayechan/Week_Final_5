#include "pch.h"
#include "DirectionalLightComponent.h"
#include "BillboardComponent.h"
#include "Gizmo/GizmoArrowComponent.h"

#include "RenderManager.h"
#include "D3D11RHI.h"

UDirectionalLightComponent::UDirectionalLightComponent()
{
	CreateShadowResource();
}

UDirectionalLightComponent::~UDirectionalLightComponent()
{
	ReleaseShadowResource();
}

IMPLEMENT_CLASS(UDirectionalLightComponent)

BEGIN_PROPERTIES(UDirectionalLightComponent)
	MARK_AS_COMPONENT("디렉셔널 라이트", "방향성 라이트 (태양광 같은 평행광) 컴포넌트입니다.")
	ADD_PROPERTY_RANGE(int, ShadowMapWidth, "ShadowMap", 32, 2048, true, "쉐도우 맵 Width")
	ADD_PROPERTY_RANGE(int, ShadowMapHeight, "ShadowMap", 32, 2048, true, "쉐도우 맵 Height")
	ADD_PROPERTY_RANGE(float, Near, "ShadowMap", 0.01f, 10.0f, true, "쉐도우 맵 Near Plane")
	ADD_PROPERTY_RANGE(float, Far, "ShadowMap", 11.0f, 1000.0f, true, "쉐도우 맵 Far Plane")
	ADD_PROPERTY_SRV(ID3D11ShaderResourceView*, ShadowMapSRV, "ShadowMap", true, "쉐도우 맵 Far Plane")
END_PROPERTIES()
void UDirectionalLightComponent::ReleaseShadowResource()
{
	ShadowMapSRV->Release();
	ShadowMapDSV->Release();
}
void UDirectionalLightComponent::CreateShadowResource()
{
	ID3D11Device* Device = URenderManager::GetInstance().GetRenderer()->GetRHIDevice()->GetDevice();

	ShadowMapViewport.Width = (float)ShadowMapWidth;
	ShadowMapViewport.Height = (float)ShadowMapHeight;
	D3D11_TEXTURE2D_DESC TexDesc = {};
	TexDesc.Width = ShadowMapWidth;
	TexDesc.Height = ShadowMapHeight;
	TexDesc.MipLevels = 1;
	TexDesc.ArraySize = 1;
	TexDesc.Format = DXGI_FORMAT_R24G8_TYPELESS;
	TexDesc.SampleDesc.Count = 1;
	TexDesc.SampleDesc.Quality = 0;
	TexDesc.Usage = D3D11_USAGE_DEFAULT;
	TexDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;

	ID3D11Texture2D* DepthMap = {};
	Device->CreateTexture2D(&TexDesc, nullptr, &DepthMap);

	D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
	SRVDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
	SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	SRVDesc.Texture2D.MipLevels = TexDesc.MipLevels;
	SRVDesc.Texture2D.MostDetailedMip = 0;
	Device->CreateShaderResourceView(DepthMap, &SRVDesc, &ShadowMapSRV);

	D3D11_DEPTH_STENCIL_VIEW_DESC DSVDesc = {};
	DSVDesc.Flags = 0;
	DSVDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	DSVDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	Device->CreateDepthStencilView(DepthMap, &DSVDesc, &ShadowMapDSV);

	DepthMap->Release();
}
FVector UDirectionalLightComponent::GetLightDirection() const
{
	// Z-Up Left-handed 좌표계에서 Forward는 X축
	FQuat Rotation = GetWorldRotation();
	return Rotation.RotateVector(FVector(1.0f, 0.0f, 0.0f));
}

FDirectionalLightInfo UDirectionalLightComponent::GetLightInfo() const
{
	FDirectionalLightInfo Info;
	// Use GetLightColorWithIntensity() to include Temperature + Intensity
	Info.Color = GetLightColorWithIntensity();
	Info.Direction = GetLightDirection();
	Info.Padding = 0.0f; // 패딩 초기화

	return Info;
}

void UDirectionalLightComponent::OnRegister(UWorld* InWorld)
{
	Super::OnRegister(InWorld);

	UE_LOG("DirectionalLightComponent::OnRegister called");

	if (SpriteComponent)
	{
		SpriteComponent->SetTextureName(GDataDir + "/UI/Icons/S_LightDirectional.dds");
	}

	// Create Direction Gizmo if not already created
	if (!DirectionGizmo)
	{
		UE_LOG("Creating DirectionGizmo...");
		CREATE_EDITOR_COMPONENT(DirectionGizmo, UGizmoArrowComponent);

		// Set gizmo mesh (using the same mesh as GizmoActor's arrow)
		DirectionGizmo->SetStaticMesh(GDataDir + "/Gizmo/TranslationHandle.obj");
		DirectionGizmo->SetMaterialByName(0, "Shaders/UI/Gizmo.hlsl");

		// Use world-space scale (not screen-constant scale like typical gizmos)
		DirectionGizmo->SetUseScreenConstantScale(false);

		// Set default scale
		DirectionGizmo->SetDefaultScale(FVector(0.5f, 0.3f, 0.3f));

		// Update gizmo properties to match light
		UpdateDirectionGizmo();
		UE_LOG("DirectionGizmo created successfully");
	}
	else
	{
		UE_LOG("DirectionGizmo already exists");
	}
	InWorld->GetLightManager()->RegisterLight(this);
}

void UDirectionalLightComponent::OnUnregister()
{
	GWorld->GetLightManager()->DeRegisterLight(this);
}

void UDirectionalLightComponent::UpdateLightData()
{
	Super::UpdateLightData();
	// 방향성 라이트 특화 업데이트 로직
	GWorld->GetLightManager()->UpdateLight(this);
	// Update direction gizmo to reflect any changes
	UpdateDirectionGizmo();
}

void UDirectionalLightComponent::OnTransformUpdated()
{
	Super::OnTransformUpdated();
	GWorld->GetLightManager()->UpdateLight(this);
}

void UDirectionalLightComponent::OnSerialized()
{
	Super::OnSerialized();

}

void UDirectionalLightComponent::DuplicateSubObjects()
{
	Super::DuplicateSubObjects();
	DirectionGizmo = nullptr;
}

void UDirectionalLightComponent::UpdateDirectionGizmo()
{
	if (!DirectionGizmo)
		return;

	// Set direction to match light direction (used for hovering/picking, not for rendering)
	FVector LightDir = GetLightDirection();
	DirectionGizmo->SetDirection(LightDir);

	// Set color to match base light color (without intensity or temperature multipliers)
	const FLinearColor& BaseColor = GetLightColor();
	DirectionGizmo->SetColor(FVector(BaseColor.R, BaseColor.G, BaseColor.B));

	// DirectionGizmo is a child of DirectionalLightComponent, so it inherits the parent's rotation automatically.
	// Arrow mesh points along +X axis by default, which matches the DirectionalLight's forward direction.
	// No need to set RelativeRotation - it should remain identity (0, 0, 0)
}
