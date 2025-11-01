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
	ADD_PROPERTY(bool, bCascaded, "ShadowMap", true, "Cascaded 사용 여부")
	ADD_PROPERTY_RANGE(int, CascadedCount, "ShadowMap", 1, 8, true, "Cascaded 갯수")
	ADD_PROPERTY_RANGE(float, CascadedLinearBlendingValue, "ShadowMap", 0, 1, true, "Cascaded Log~Linear 가중치 : 0~1")
	ADD_PROPERTY_RANGE(float, CascadedOverlapValue, "ShadowMap", 0, 0.5f, true, "Cascaded 확장 범위 크기")
	ADD_PROPERTY_RANGE(float, CascadedAreaColorDebugValue, "ShadowMap", 0, 1.0f, true, "Cascaded 범위 시각화")
	ADD_PROPERTY_RANGE(int, CascadedAreaShadowDebugValue, "ShadowMap", -1, 8, true, "Cascaded 쉐도우 구역 설정 (-1 : 전체 쉐도우)")
	ADD_PROPERTY_SRV(ID3D11ShaderResourceView*, ShadowMapSRV, "ShadowMap", true, "쉐도우 맵 Far Plane")
	ADD_PROPERTY(bool, bOverrideCameraLightPerspective, "ShadowMap", true, "Override Camera Light Perspective")
END_PROPERTIES()

UDirectionalLightComponent::UDirectionalLightComponent()
{
	ShadowResolutionScale = 2048;
}

UDirectionalLightComponent::~UDirectionalLightComponent()
{
}

void UDirectionalLightComponent::GetShadowRenderRequests(FSceneView* View, TArray<FShadowRenderRequest>& OutRequests)
{
	FMatrix ShadowMapView = GetWorldRotation().Inverse().ToMatrix() * FMatrix::ZUpToYUp;
	FMatrix ViewInv = View->Camera->GetViewMatrix().InverseAffine();
	if (bCascaded == false)
	{
		TArray<FVector> CameraFrustum = View->Camera->GetFrustumVertices(View->Viewport);
		float Near = View->Camera->GetNearClip();
		float Far = View->Camera->GetFarClip();
		float CenterDepth = (Far + Near) / 2;
		FVector Center = FVector(0, 0, CenterDepth);
		float MaxDis = FVector::Distance(Center, CameraFrustum[7]) * 2;
		float WorldSizePerTexel = MaxDis / ShadowResolutionScale;
		
		CameraFrustum *= ViewInv;
		CameraFrustum *= ShadowMapView;
		FAABB CameraFrustumAABB = FAABB(CameraFrustum);
		CameraFrustumAABB.Min = CameraFrustumAABB.Min.SnapToGrid(FVector(WorldSizePerTexel, WorldSizePerTexel,0), true);
		CameraFrustumAABB.Max = CameraFrustumAABB.Min + FVector(MaxDis, MaxDis, MaxDis);
		CameraFrustumAABB.Min.Z -= View->Camera->GetFarClip();
		FMatrix ShadowMapOrtho = FMatrix::OrthoMatrix(CameraFrustumAABB);
		FShadowRenderRequest ShadowRenderRequest;
		ShadowRenderRequest.LightOwner = this;
		ShadowRenderRequest.ViewMatrix = ShadowMapView;
		ShadowRenderRequest.ProjectionMatrix = ShadowMapOrtho;
		ShadowRenderRequest.Size = ShadowResolutionScale;
		ShadowRenderRequest.SubViewIndex = 0;
		ShadowRenderRequest.AtlasScaleOffset = 0;
		OutRequests.Add(ShadowRenderRequest);
	}
	else 
	{
		CascadedSliceDepth = View->Camera->GetCascadedSliceDepth(CascadedCount, CascadedLinearBlendingValue);
		for (int i = 0; i < CascadedCount; i++)
		{
			float Near = CascadedSliceDepth[i];
			float Far = CascadedSliceDepth[i + 1];
			//Near -= Near * CascadedOverlapValue;
			Far += Far * CascadedOverlapValue;
			TArray<FVector> CameraFrustum = View->Camera->GetFrustumVerticesCascaded(View->Viewport, Near, Far);
			float CenterDepth = (Far + Near) / 2;
			FVector Center = FVector(0, 0, CenterDepth);
			float MaxDis = FVector::Distance(Center, CameraFrustum[7]) * 2;
			float WorldSizePerTexel = MaxDis / ShadowResolutionScale;

			CameraFrustum *= ViewInv;
			CameraFrustum *= ShadowMapView;
			FAABB CameraFrustumAABB = FAABB(CameraFrustum);
			CameraFrustumAABB.Min = CameraFrustumAABB.Min.SnapToGrid(FVector(WorldSizePerTexel, WorldSizePerTexel, 0), true);
			CameraFrustumAABB.Max = CameraFrustumAABB.Min + FVector(MaxDis, MaxDis, MaxDis);
			CameraFrustumAABB.Min.Z -= View->Camera->GetFarClip();
			FMatrix ShadowMapOrtho = FMatrix::OrthoMatrix(CameraFrustumAABB);
			FShadowRenderRequest ShadowRenderRequest;
			ShadowRenderRequest.LightOwner = this;
			ShadowRenderRequest.ViewMatrix = ShadowMapView;
			ShadowRenderRequest.ProjectionMatrix = ShadowMapOrtho;
			ShadowRenderRequest.Size = ShadowResolutionScale;
			ShadowRenderRequest.SubViewIndex = i;
			ShadowRenderRequest.AtlasScaleOffset = 0;
			OutRequests.Add(ShadowRenderRequest);
		}
	}
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
	Info.bCastShadows = bCastShadows ? 1 : 0;

	Info.bCascaded = bCascaded ? 1 : 0;
	Info.CascadeCount = CascadedCount;
	Info.CascadedOverlapValue = CascadedOverlapValue;
	Info.CascadedAreaColorDebugValue = CascadedAreaColorDebugValue;
	Info.CascadedAreaShadowDebugValue = CascadedAreaShadowDebugValue;
	for (int i = 0; i < CascadedSliceDepth.Num(); i++)
	{
		Info.CascadedSliceDepth[i] = CascadedSliceDepth[i];
	}
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
	if (!DirectionGizmo && !InWorld->bPie)
	{
		UE_LOG("Creating DirectionGizmo...");
		CREATE_EDITOR_COMPONENT(DirectionGizmo, UGizmoArrowComponent);

		// Set gizmo mesh (using the same mesh as GizmoActor's arrow)
		DirectionGizmo->SetStaticMesh(GDataDir + "/Gizmo/TranslationHandle.obj");
		DirectionGizmo->SetMaterialByName(0, "Shaders/UI/Gizmo.hlsl");

		// Use world-space scale (not screen-constant scale like typical gizmos)
		DirectionGizmo->SetUseScreenConstantScale(false);

		// Set default scale
		DirectionGizmo->SetDefaultScale(FVector(2.5f, 2.5f, 2.5f));

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

void UDirectionalLightComponent::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	Super::Serialize(bInIsLoading, InOutHandle);

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