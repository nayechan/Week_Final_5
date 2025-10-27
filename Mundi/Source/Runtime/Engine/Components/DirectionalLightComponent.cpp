#include "pch.h"
#include "DirectionalLightComponent.h"
#include "BillboardComponent.h"
#include "Gizmo/GizmoArrowComponent.h"
#include "SceneView.h"
#include "FViewport.h"

#include "RenderManager.h"
#include "D3D11RHI.h"

UDirectionalLightComponent::UDirectionalLightComponent()
{
}

UDirectionalLightComponent::~UDirectionalLightComponent()
{
}


// 익명 네임스페이스 (Anonymous Namespace) 사용
namespace
{
	/**
	 * @brief Perspective Shadow Mapping (PSM)을 위한 최종 투영 행렬을 계산합니다.
	 * 라이트 공간 AABB를 기반으로 X, Y는 직교 투영하고, Z는 원근 왜곡합니다. (D3D NDC [0, 1] 기준)
	 * @param LightSpaceAABB 카메라 절두체 8개 꼭짓점을 라이트 뷰 공간으로 변환한 후 계산된 AABB.
	 * @return 계산된 PSM 투영 행렬 (4x4).
	 */
	FMatrix CalculatePSMProjectionMatrix(const FAABB& LightSpaceAABB)
	{
		// 1. 라이트 공간 AABB에서 X, Y, Z 범위 추출
		//    OrthoMatrix 함수 파라미터 순서(R, L, T, B, F, N)에 맞게 변수 사용
		float L = LightSpaceAABB.Min.X;
		float R = LightSpaceAABB.Max.X;
		float B = LightSpaceAABB.Min.Y;
		float T = LightSpaceAABB.Max.Y;
		float N = LightSpaceAABB.Min.Z; // 라이트 공간에서의 Near
		float F = LightSpaceAABB.Max.Z; // 라이트 공간에서의 Far

		// Z 범위 유효성 검사 (0으로 나누기 방지)
		if (F <= N)
		{
			// 유효하지 않은 범위면 기본 직교 행렬 반환
			UE_LOG("CalculatePSMProjectionMatrix: Invalid Z range (NearZ >= FarZ).");
			return FMatrix::OrthoMatrix(R, L, T, B, F, N); // 원래 함수 호출
		}
		// 추가: XY 범위 유효성 검사
		if (R <= L || T <= B)
		{
			UE_LOG("CalculatePSMProjectionMatrix: Invalid XY range.");
			return FMatrix::OrthoMatrix(R, L, T, B, F, N); // 원래 함수 호출
		}

		// 2. 직교 투영 행렬의 X, Y 관련 요소 계산 (제공된 OrthoMatrix 함수 로직 사용)
		const float M_A = 2.0f / (R - L); // X Scale
		const float M_B = 2.0f / (T - B); // Y Scale
		const float M_D = -(R + L) / (R - L); // X Offset
		const float M_E = -(T + B) / (T - B); // Y Offset

		// 3. PSM 왜곡을 위한 Z 요소 계산 (D3D NDC [0, 1] 기준)
		const float A_psm = F / (F - N); // Z Scale (PSM)
		const float B_psm = -N * F / (F - N); // Z Offset (PSM)

		// 4. 최종 PSM 행렬 구성 (제공된 OrthoMatrix 함수의 생성자 방식 사용, Row-Major 가정)
		return FMatrix(
			M_A, 0.0f, 0.0f, 0.0f,  // Row 0
			0.0f, M_B, 0.0f, 0.0f,  // Row 1
			0.0f, 0.0f, A_psm, 1.0f,  // Row 2: PSM Z scale, Copy Z to W
			M_D, M_E, B_psm, 0.0f   // Row 3: Ortho XY offsets, PSM Z offset, W' = 0
		);
	}
} // end anonymous namespace

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
	ShadowRenderRequest.Size = 8192;
	ShadowRenderRequest.SubViewIndex = 0;
	ShadowRenderRequest.AtlasScaleOffset = 0;
	OutRequests.Add(ShadowRenderRequest);
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