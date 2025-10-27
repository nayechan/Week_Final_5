#include "pch.h"
#include "DirectionalLightComponent.h"
#include "BillboardComponent.h"
#include "Gizmo/GizmoArrowComponent.h"
#include "SceneView.h"
#include "FViewport.h"

#include "RenderManager.h"
#include "D3D11RHI.h"

IMPLEMENT_CLASS(UDirectionalLightComponent)

BEGIN_PROPERTIES(UDirectionalLightComponent)
	MARK_AS_COMPONENT("디렉셔널 라이트", "방향성 라이트 (태양광 같은 평행광) 컴포넌트입니다.")
	ADD_PROPERTY_RANGE(int, ShadowMapWidth, "ShadowMap", 32, 2048, true, "쉐도우 맵 Width")
	ADD_PROPERTY_RANGE(int, ShadowMapHeight, "ShadowMap", 32, 2048, true, "쉐도우 맵 Height")
	ADD_PROPERTY_RANGE(float, Near, "ShadowMap", 0.01f, 10.0f, true, "쉐도우 맵 Near Plane")
	ADD_PROPERTY_RANGE(float, Far, "ShadowMap", 11.0f, 1000.0f, true, "쉐도우 맵 Far Plane")
	ADD_PROPERTY_SRV(ID3D11ShaderResourceView*, ShadowMapSRV, "ShadowMap", true, "쉐도우 맵 Far Plane")
END_PROPERTIES()

UDirectionalLightComponent::UDirectionalLightComponent()
{
}

UDirectionalLightComponent::~UDirectionalLightComponent()
{
}

void UDirectionalLightComponent::GetShadowRenderRequests(FSceneView* View, TArray<FShadowRenderRequest>& OutRequests)
{
	FMatrix ShadowMapView = GetWorldRotation().Inverse().ToMatrix() * FMatrix::ZUpToYUp;
	FMatrix RotInv = GetWorldRotation().Inverse().ToMatrix();

	TArray<FVector> CameraFrustum = View->Camera->GetViewAreaVerticesWS(View->Viewport);

	for (FVector& CameraFrustumPoint : CameraFrustum)
	{
		CameraFrustumPoint = CameraFrustumPoint * ShadowMapView;
	}

	FAABB CameraFrustumAABB = FAABB(CameraFrustum);

	FMatrix ShadowMapOrtho = FMatrix::OrthoMatrix(CameraFrustumAABB);

	// [권장] 최종 PSM 행렬을 직접 계산하는 함수 사용
	FMatrix FinalProjectionMatrix = ShadowMapOrtho;

	FShadowRenderRequest ShadowRenderRequest;
	ShadowRenderRequest.LightOwner = this;
	ShadowRenderRequest.ViewMatrix = ShadowMapView;
	ShadowRenderRequest.ProjectionMatrix = ShadowMapOrtho;
	ShadowRenderRequest.Size = 8196;
	ShadowRenderRequest.SubViewIndex = 0;
	ShadowRenderRequest.AtlasScaleOffset = 0;
	OutRequests.Add(ShadowRenderRequest);
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