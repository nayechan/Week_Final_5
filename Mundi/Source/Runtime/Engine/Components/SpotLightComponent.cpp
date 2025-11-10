#include "pch.h"
#include "SpotLightComponent.h"
#include "BillboardComponent.h"
#include "CameraComponent.h"
#include "FViewport.h"
#include "Gizmo/GizmoArrowComponent.h"
#include "LightManager.h"
#include "SceneView.h"
// IMPLEMENT_CLASS is now auto-generated in .generated.cpp
USpotLightComponent::USpotLightComponent()
{
	InnerConeAngle = 30.0f;
	OuterConeAngle = 45.0f;
}

void USpotLightComponent::ValidateConeAngles()
{
	// OuterCone과 InnerCone의 일관성 유지
	// 어느 값이 변경되었는지 추적하여 다른 값을 조정

	// InnerCone이 변경됨
	bool bInnerChanged = (InnerConeAngle != PreviousInnerConeAngle);
	// OuterCone이 변경됨
	bool bOuterChanged = (OuterConeAngle != PreviousOuterConeAngle);

	if (bInnerChanged)
	{
		// InnerCone이 증가하여 OuterCone보다 커진 경우
		if (InnerConeAngle > OuterConeAngle)
		{
			OuterConeAngle = InnerConeAngle;
		}
	}
	else if (bOuterChanged)
	{
		// OuterCone이 감소하여 InnerCone보다 작아진 경우
		if (OuterConeAngle < InnerConeAngle)
		{
			InnerConeAngle = OuterConeAngle;
		}
	}

	// 이전 값 업데이트
	PreviousInnerConeAngle = InnerConeAngle;
	PreviousOuterConeAngle = OuterConeAngle;
}

USpotLightComponent::~USpotLightComponent()
{
}

void USpotLightComponent::GetShadowRenderRequests(FSceneView* View, TArray<FShadowRenderRequest>& OutRequests)
{
	FShadowRenderRequest ShadowRenderRequest;
	ShadowRenderRequest.LightOwner = this;
	ShadowRenderRequest.ViewMatrix = GetViewMatrix();
	ShadowRenderRequest.ProjectionMatrix = GetProjectionMatrix();
	ShadowRenderRequest.WorldLocation = GetWorldLocation();
	ShadowRenderRequest.Radius = GetAttenuationRadius();
	ShadowRenderRequest.Size = ShadowResolutionScale;
	// ShadowRenderRequest.ViewMatrix = GetViewMatrix() * GetProjectionMatrix();
	// ShadowRenderRequest.ProjectionMatrix = WarpMatrix;
	ShadowRenderRequest.SubViewIndex = 0;
	ShadowRenderRequest.AtlasScaleOffset = 0;
	OutRequests.Add(ShadowRenderRequest);
}

FVector USpotLightComponent::GetDirection() const
{
	// Z-Up Left-handed 좌표계에서 Forward는 X축
	FQuat Rotation = GetWorldRotation();
	return Rotation.RotateVector(FVector(1.0f, 0.0f, 0.0f));
}

float USpotLightComponent::GetConeAttenuation(const FVector& WorldPosition) const
{
	FVector LightPosition = GetWorldLocation();
	FVector LightDirection = GetDirection();
	FVector ToPosition = (WorldPosition - LightPosition).GetNormalized();

	// 방향 벡터와의 각도 계산
	float CosAngle = FVector::Dot(LightDirection, ToPosition);
	float Angle = acos(CosAngle) * (180.0f / 3.14159265f); // Radians to Degrees

	// 내부 원뿔 안: 감쇠 없음
	if (Angle <= InnerConeAngle)
	{
		return 1.0f;
	}

	// 외부 원뿔 밖: 빛 없음
	if (Angle >= OuterConeAngle)
	{
		return 0.0f;
	}

	// 내부와 외부 사이: 부드러운 감쇠
	float Range = OuterConeAngle - InnerConeAngle;
	float T = (Angle - InnerConeAngle) / Range;
	return 1.0f - T;
}

FSpotLightInfo USpotLightComponent::GetLightInfo() const
{
	FSpotLightInfo Info;
	// Use GetLightColorWithIntensity() to include Temperature + Intensity
	Info.Color = GetLightColorWithIntensity();
	Info.Position = GetWorldLocation();
	Info.InnerConeAngle = GetInnerConeAngle();
	Info.Direction = GetDirection();
	Info.OuterConeAngle = GetOuterConeAngle();
	Info.AttenuationRadius = GetAttenuationRadius();
	Info.FalloffExponent = GetFalloffExponent(); // Always pass FalloffExponent (used when bUseInverseSquareFalloff = false)
	Info.bUseInverseSquareFalloff = IsUsingInverseSquareFalloff() ? 1u : 0u;
	// Info.ShadowData.ShadowViewProjMatrix = GetViewMatrix() * GetProjectionMatrix() * GetWarpMatrix();
	Info.ShadowData.ShadowViewProjMatrix = GetViewMatrix() * GetProjectionMatrix();
	Info.bCastShadows = 0u;		// UpdateLightBuffer 에서 초기화 해줌
	Info.ShadowData.WorldPosition = GetWorldLocation();
	return Info;
}

void USpotLightComponent::UpdateLightData()
{
    Super::UpdateLightData();
    // 스포트라이트 특화 업데이트 로직

	// Cone 각도 유효성 검사 (UI에서 변경된 경우를 대비)
	ValidateConeAngles();

    if (UWorld* World = GetWorld())
    {
        if (World->GetLightManager())
        {
            World->GetLightManager()->UpdateLight(this);
        }
    }

	// Update direction gizmo to reflect any changes
	UpdateDirectionGizmo();
}

void USpotLightComponent::OnTransformUpdated()
{
    Super::OnTransformUpdated();
    if (UWorld* World = GetWorld())
    {
        if (World->GetLightManager())
        {
            World->GetLightManager()->UpdateLight(this);
        }
    }
}

void USpotLightComponent::OnRegister(UWorld* InWorld)
{
	Super::OnRegister(InWorld);
	
	if (SpriteComponent)
	{
		SpriteComponent->SetTexture(GDataDir + "/UI/Icons/SpotLight_64x.png");
	}

	// Create Direction Gizmo if not already created
	if (!DirectionGizmo && !InWorld->bPie)
	{
		CREATE_EDITOR_COMPONENT(DirectionGizmo, UGizmoArrowComponent);

		// DirectionGizmo->SetCanEverPick(false);

		// Set gizmo mesh (using the same mesh as GizmoActor's arrow)
		DirectionGizmo->SetStaticMesh(GDataDir + "/Gizmo/TranslationHandle.obj");
		DirectionGizmo->SetMaterialByName(0, "Shaders/UI/Gizmo.hlsl");

		// Use world-space scale (not screen-constant scale like typical gizmos)
		DirectionGizmo->SetUseScreenConstantScale(false);

		// Set default scale
		DirectionGizmo->SetDefaultScale(FVector(0.5f, 0.3f, 0.3f));

		// Update gizmo properties to match light
		UpdateDirectionGizmo();
	}
    InWorld->GetLightManager()->RegisterLight(this);
}

void USpotLightComponent::OnUnregister()
{
    if (UWorld* World = GetWorld())
    {
        if (World->GetLightManager())
        {
            World->GetLightManager()->DeRegisterLight(this);
        }
    }

    Super::OnUnregister();
}

void USpotLightComponent::UpdateDirectionGizmo()
{
	if (!DirectionGizmo)
		return;

	// Set direction to match light direction (used for hovering/picking, not for rendering)
	FVector LightDir = GetDirection();
	DirectionGizmo->SetDirection(LightDir);

	// Set color to match base light color (without intensity or temperature multipliers)
	const FLinearColor& BaseColor = GetLightColor();
	DirectionGizmo->SetColor(FVector(BaseColor.R, BaseColor.G, BaseColor.B));
}

FMatrix USpotLightComponent::GetViewMatrix() const
{
	static const FMatrix YUpToZUp(
		0, 1, 0, 0,
		0, 0, 1, 0,
		1, 0, 0, 0,
		0, 0, 0, 1
	);

	const FMatrix World = GetWorldTransform().ToMatrix();

	return (YUpToZUp * World).InverseAffine();
}

FMatrix USpotLightComponent::GetProjectionMatrix() const
{
	float Fovy = DegreesToRadians(OuterConeAngle * 2.0f);
	return FMatrix::PerspectiveFovLH(Fovy, 1.0f, 0.5f, GetAttenuationRadius());
}

void USpotLightComponent::RenderDebugFrustum(TArray<FVector>& StartPoints, TArray<FVector>& EndPoints, TArray<FVector4>& Colors) const
{
	FMatrix VP_Matrix = (GetViewMatrix() * GetProjectionMatrix());
	
	FMatrix InverseViewProj = VP_Matrix.Inverse();
	
	FVector NDC[8] = {
		// near
		FVector(-1.0f, -1.0f, 0.0f), // bottom left
		FVector(1.0f, -1.0f, 0.0f), // bottom right
		FVector(1.0f, 1.0f, 0.0f), // top right
		FVector(-1.0f, 1.0f, 0.0f), // top left
		// far
		FVector(-1.0f, -1.0f, 1.0f), // bottom left 
		FVector(1.0f, -1.0f, 1.0f), // bottom right
		FVector(1.0f, 1.0f, 1.0f), // top right
		FVector(-1.0f, 1.0f, 1.0f) // top left
	};

	// 0, 1 / 1, 2 / 2, 3 / 3, 0
	FVector WorldFrustum[8];
	for (int i =0; i<8; i++)
	{
		FVector4 NDCCorner = FVector4(NDC[i].X, NDC[i].Y, NDC[i].Z, 1.0f);
		FVector4 WorldCorner = NDCCorner * InverseViewProj;

		if (FMath::Abs(WorldCorner.W) > KINDA_SMALL_NUMBER)
		{
			WorldFrustum[i] = FVector(WorldCorner.X, WorldCorner.Y, WorldCorner.Z) / WorldCorner.W; 
		}
		else
		{
			WorldFrustum[i] = FVector(WorldCorner.X, WorldCorner.Y, WorldCorner.Z);
		}
	}

	const FVector4 FrustumColor = FVector4(1.0f, 0.0f, 1.0f, 1.0f);

	// near
	StartPoints.Add(WorldFrustum[0]);
	EndPoints.Add(WorldFrustum[1]);
	Colors.Add(FrustumColor);
	StartPoints.Add(WorldFrustum[1]);
	EndPoints.Add(WorldFrustum[2]);
	Colors.Add(FrustumColor);
	StartPoints.Add(WorldFrustum[2]);
	EndPoints.Add(WorldFrustum[3]);
	Colors.Add(FrustumColor);
	StartPoints.Add(WorldFrustum[3]);
	EndPoints.Add(WorldFrustum[0]);
	Colors.Add(FrustumColor);
	
	StartPoints.Add(WorldFrustum[4]);
	EndPoints.Add(WorldFrustum[5]);
	Colors.Add(FrustumColor);
	StartPoints.Add(WorldFrustum[5]);
	EndPoints.Add(WorldFrustum[6]);
	Colors.Add(FrustumColor);
	StartPoints.Add(WorldFrustum[6]);
	EndPoints.Add(WorldFrustum[7]);
	Colors.Add(FrustumColor);
	StartPoints.Add(WorldFrustum[7]);
	EndPoints.Add(WorldFrustum[4]);
	Colors.Add(FrustumColor);

	StartPoints.Add(WorldFrustum[0]);
	EndPoints.Add(WorldFrustum[4]);
	Colors.Add(FrustumColor);
	StartPoints.Add(WorldFrustum[1]);
	EndPoints.Add(WorldFrustum[5]);
	Colors.Add(FrustumColor);
	StartPoints.Add(WorldFrustum[2]);
	EndPoints.Add(WorldFrustum[6]);
	Colors.Add(FrustumColor);
	StartPoints.Add(WorldFrustum[3]);
	EndPoints.Add(WorldFrustum[7]);
	Colors.Add(FrustumColor);
}

void USpotLightComponent::CalculateWarpMatrix(URenderer* Renderer, UCameraComponent* Camera, FViewport* Viewport)
{
	FVector CameraNDCCorners[8]= {
		// near
		FVector(-1.0f, -1.0f, 0.0f), // bottom left
		FVector(1.0f, -1.0f, 0.0f), // bottom right
		FVector(1.0f, 1.0f, 0.0f), // top right
		FVector(-1.0f, 1.0f, 0.0f), // top left
		// far
		FVector(-1.0f, -1.0f, 1.0f), // bottom left 
		FVector(1.0f, -1.0f, 1.0f), // bottom right
		FVector(1.0f, 1.0f, 1.0f), // top right
		FVector(-1.0f, 1.0f, 1.0f) // top left
	};

	// === [1단계: 카메라 절두체의 월드 공간 꼭짓점 8개 구하기] ===
    // (이 로직은 CSM과 PSM이 동일함. 네 코드 그대로 사용)
    FMatrix CameraView = Camera->GetViewMatrix();
    FMatrix CameraProj = Camera->GetProjectionMatrix(Viewport->GetAspectRatio(), Viewport);
    FMatrix InverseCameraViewProj = (CameraView * CameraProj).Inverse();
    
    FVector WorldFrustum[8] = {};
    for (int i = 0; i < 8; i++)
    {
       FVector4 NDC = FVector4(CameraNDCCorners[i].X, CameraNDCCorners[i].Y, CameraNDCCorners[i].Z, 1.0f) * InverseCameraViewProj;
       if (FMath::Abs(NDC.W) > KINDA_SMALL_NUMBER)
       {
          WorldFrustum[i] = FVector(NDC.X, NDC.Y, NDC.Z) / NDC.W;          
       }
       else
       {
          WorldFrustum[i] = FVector(NDC.X, NDC.Y, NDC.Z);
       }
    }

    // === [2단계: 월드 공간 꼭짓점을 '라이트의 NDC 공간'으로 변환] ===
    // (CSM의 'LightView'만 곱하는 것과 달리, 'LightViewProj'를 모두 곱함)
    
    FMatrix LightView = GetViewMatrix();
    FMatrix LightProj = GetProjectionMatrix(); // 스포트라이트의 '원본' 원근 투영
    FMatrix LightViewProj = LightView * LightProj;

    TArray<FVector> LightNDCCorners;
    LightNDCCorners.Reserve(8); // 8개 공간 예약

    for (int i = 0; i < 8; i++)
    {
       FVector4 WorldCorner = FVector4(WorldFrustum[i].X, WorldFrustum[i].Y, WorldFrustum[i].Z, 1.0f);
       FVector4 LightClipCorner = WorldCorner * LightViewProj; // 라이트의 클립 공간으로

       // [중요] 라이트의 Near Plane(W) 뒤에 있는 점은 클리핑 (AABB를 망가뜨림)
       if (LightClipCorner.W > KINDA_SMALL_NUMBER)
       {
           // Perspective Divide (퍼스펙티브 나누기) 수행
           FVector LightNDC = FVector(LightClipCorner.X, LightClipCorner.Y, LightClipCorner.Z) / LightClipCorner.W;

           // D3D11의 Z는 [0, 1] 범위임. 이 범위를 벗어나는 점들도 AABB 계산에 포함하되,
           // Z좌표는 클리핑해서 AABB가 [0, 1] 범위를 유지하게 함.
           LightNDC.Z = FMath::Clamp(LightNDC.Z, 0.0f, 1.0f);
           LightNDCCorners.Add(LightNDC);
       }
       // else: W가 0이거나 음수(라이트 뒤)면 AABB 계산에서 제외
    }

    // [중요] 만약 8개의 점이 모두 라이트 뒤에 있다면(예: 라이트가 벽을 보고 있음),
    // AABB를 만들 수 없으므로 디버그를 중단해야 함.
    if (LightNDCCorners.Num() == 0)
    {
    	this->WarpMatrix = FMatrix::Identity();
        return; // 그릴 수 있는 절두체가 없음
    }

    // === [3단계: '라이트 NDC 공간'에서 AABB(경계 상자) 찾기] ===
    FAABB Bounds(LightNDCCorners);
    
    // [중요] PSM에서는 라이트의 원점(NDC 0,0,0)을 포함해야 원근 효과가 유지됨 (LiSPSM 기법)
    // 여기서는 Z=0 (Near Plane)을 강제로 포함시켜 안정성을 높임
    Bounds.Min.Z = 0.0f; // D3D11 NDC Z축은 0에서 시작
    Bounds.Max.Z = FMath::Max(Bounds.Max.Z, 0.0f + KINDA_SMALL_NUMBER); // Z Max가 0이 되는 것 방지

    // === [4단계: 'Warp Matrix' 생성] ===
    // 'LightNDC_Bounds'를 새로운 [-1, 1] (X,Y), [0, 1] (Z) 공간으로 
    // 매핑하는 '직교' 행렬을 생성. 이것이 바로 'Warp Matrix'임.
    
    // (이전 코드의 AABB 패딩 로직은 여전히 유효함! NaN 방지)
    float Min = 1.0f; // 최소 크기
    float ExtentX = Bounds.Max.X - Bounds.Min.X;
    float ExtentY = Bounds.Max.Y - Bounds.Min.Y;
    if (ExtentX < Min)
    {
       float Mid = (Bounds.Max.X + Bounds.Min.X) * 0.5f;
       Bounds.Min.X = Mid - (Min * 0.5f);
       Bounds.Max.X = Mid + (Min * 0.5f);
    }
    if (ExtentY < Min)
    {
       float Mid = (Bounds.Max.Y + Bounds.Min.Y) * 0.5f;
       Bounds.Min.Y = Mid - (Min * 0.5f);
       Bounds.Max.Y = Mid + (Min * 0.5f);
    }
    
    FMatrix NewWarpMatrix = FMatrix::OrthoMatrix(Bounds);

    // === [5단계: 최종 'Warped VP' 행렬 및 그 역행렬 계산] ===
    // PSM의 핵심: M_WarpedVP = M_Original_VP * M_Warp
    FMatrix WarpedLightVP = LightViewProj * NewWarpMatrix; 
    FMatrix InverseWarpedLightVP = WarpedLightVP.Inverse();
	WarpMatrix = NewWarpMatrix;
 
    // === [6단계: 'Warped Frustum'을 월드 공간에 그리기] ===
    // (이 로직은 CSM 코드와 동일, 사용하는 행렬만 InverseWarpedLightVP로 변경)

	FVector TightCorners[8] = {};
	for (int i = 0; i < 8; i++)
	{
		FVector4 NDCCorner = FVector4(CameraNDCCorners[i].X, CameraNDCCorners[i].Y, CameraNDCCorners[i].Z, 1.0f);
		FVector4 WorldCorner = NDCCorner * InverseWarpedLightVP; // 최종 왜곡 행렬의 역행렬 사용
	
		if (FMath::Abs(WorldCorner.W) > KINDA_SMALL_NUMBER)
		{
			TightCorners[i] = FVector(WorldCorner.X, WorldCorner.Y, WorldCorner.Z) / WorldCorner.W;
		}
		else
		{
			TightCorners[i] = FVector(WorldCorner.X, WorldCorner.Y, WorldCorner.Z);
		}
	}
    
    // ... (이후 AddLines 호출 코드는 동일) ...
    FVector4 FrustumColor(1.0f, 0.f, 0.0f, 1.0f);
    TArray<FVector> StartPoints = {};
    TArray<FVector> EndPoints = {};
    TArray<FVector4> Colors = {};
    
    // (이하 StartPoints.Add ... 로직은 동일)
	StartPoints.Add(TightCorners[0]);
	EndPoints.Add(TightCorners[1]);
	Colors.Add(FrustumColor);
	StartPoints.Add(TightCorners[1]);
	EndPoints.Add(TightCorners[2]);
	Colors.Add(FrustumColor);
	StartPoints.Add(TightCorners[2]);
	EndPoints.Add(TightCorners[3]);
	Colors.Add(FrustumColor);
	StartPoints.Add(TightCorners[3]);
	EndPoints.Add(TightCorners[0]);
	Colors.Add(FrustumColor);
	
	StartPoints.Add(TightCorners[4]);
	EndPoints.Add(TightCorners[5]);
	Colors.Add(FrustumColor);
	StartPoints.Add(TightCorners[5]);
	EndPoints.Add(TightCorners[6]);
	Colors.Add(FrustumColor);
	StartPoints.Add(TightCorners[6]);
	EndPoints.Add(TightCorners[7]);
	Colors.Add(FrustumColor);
	StartPoints.Add(TightCorners[7]);
	EndPoints.Add(TightCorners[4]);
	Colors.Add(FrustumColor);
 
	StartPoints.Add(TightCorners[0]);
	EndPoints.Add(TightCorners[4]);
	Colors.Add(FrustumColor);
	StartPoints.Add(TightCorners[1]);
	EndPoints.Add(TightCorners[5]);
	Colors.Add(FrustumColor);
	StartPoints.Add(TightCorners[2]);
	EndPoints.Add(TightCorners[6]);
	Colors.Add(FrustumColor);
	StartPoints.Add(TightCorners[3]);
	EndPoints.Add(TightCorners[7]);
	Colors.Add(FrustumColor);
    Renderer->AddLines(StartPoints, EndPoints, Colors);
}

void USpotLightComponent::RenderDebugVolume(URenderer* Renderer) const
{
	// Cone 각도 유효성 검사 (const 메서드이므로 const_cast 사용)
	const_cast<USpotLightComponent*>(this)->ValidateConeAngles();

	// Unreal Engine 스타일 SpotLight 볼륨 렌더링
	const FVector4 InnerConeColor(1.0f, 1.0f, 0.0f, 1.0f); // 노란색 (InnerCone)
	const FVector4 OuterConeColor(1.0f, 0.5f, 0.0f, 1.0f); // 주황색 (OuterCone)

	TArray<FVector> StartPoints;
	TArray<FVector> EndPoints;
	TArray<FVector4> Colors;

	// 원뿔의 정점(apex)은 라이트의 위치
	const FVector ApexWorld = GetWorldLocation();
	const FVector LightDirection = GetDirection();

	// 볼록한 아크의 가장 먼 지점(LightDirection 방향)까지의 거리가 AttenuationRadius
	const float AttenuationRadius = GetAttenuationRadius();

	// 세그먼트 수
	const int NumRays = 24;      // 방사형 라인 개수
	const int NumArcSegments = 32; // 아크 세그먼트 개수

	// LightDirection에 수직인 두 벡터 구하기
	FVector Up = FVector(0.0f, 0.0f, 1.0f);
	if (std::abs(FVector::Dot(LightDirection, Up)) > 0.99f)
	{
		Up = FVector(0.0f, 1.0f, 0.0f);
	}
	FVector Right = FVector::Cross(LightDirection, Up).GetNormalized();
	FVector ActualUp = FVector::Cross(Right, LightDirection).GetNormalized();

	// --- OuterCone 그리기 ---
	const float OuterAngleRad = DegreesToRadians(OuterConeAngle);

	// 방사형 라인 길이는 AttenuationRadius로 고정
	const float OuterRayLength = AttenuationRadius;

	// 아크의 가장 먼 지점(LightDirection 방향)이 AttenuationRadius가 되도록 ArcRadius 계산
	// ArcRadius × cos(angle) = AttenuationRadius
	// ArcRadius = AttenuationRadius / cos(angle)
	const float OuterArcRadius = (OuterAngleRad < HALF_PI && std::cos(OuterAngleRad) > 0.0001f)
		? AttenuationRadius / std::cos(OuterAngleRad)
		: AttenuationRadius;

	// 밑면 원을 위한 끝점들 저장
	TArray<FVector> OuterEndPoints;
	OuterEndPoints.Reserve(NumRays);

	// 1. 방사형 라인 (Apex에서 원뿔 가장자리로) - 길이 고정
	for (int i = 0; i < NumRays; ++i)
	{
		const float Angle = (static_cast<float>(i) / NumRays) * TWO_PI;
		const FVector RadialDir = (Right * std::cos(Angle) + ActualUp * std::sin(Angle)).GetNormalized();

		// 원뿔 방향으로 회전
		const FVector RayDir = (LightDirection * std::cos(OuterAngleRad) + RadialDir * std::sin(OuterAngleRad)).GetNormalized();
		const FVector RayEnd = ApexWorld + RayDir * OuterRayLength;

		StartPoints.Add(ApexWorld);
		EndPoints.Add(RayEnd);
		Colors.Add(OuterConeColor);

		OuterEndPoints.Add(RayEnd);
	}

	// 밑면 원 그리기 (방사형 라인의 끝점들을 연결)
	for (int i = 0; i < NumRays; ++i)
	{
		StartPoints.Add(OuterEndPoints[i]);
		EndPoints.Add(OuterEndPoints[(i + 1) % NumRays]);
		Colors.Add(OuterConeColor);
	}

	// 2. 언리얼 엔진 스타일 아크 (5+1+5 정점 방식)
	// ActualUp축 기준 아크
	{
		// 가운데 정점 (가장 볼록한 부분) - LightDirection 방향으로 AttenuationRadius 거리
		const FVector CenterPoint = ApexWorld + LightDirection * AttenuationRadius;

		// 양쪽 끝점 (방사형 라인의 끝점과 동일)
		const FVector RayDirLeft = (LightDirection * std::cos(OuterAngleRad) + ActualUp * std::sin(OuterAngleRad)).GetNormalized();
		const FVector RayDirRight = (LightDirection * std::cos(OuterAngleRad) - ActualUp * std::sin(OuterAngleRad)).GetNormalized();
		const FVector LeftEndPoint = ApexWorld + RayDirLeft * OuterRayLength;
		const FVector RightEndPoint = ApexWorld + RayDirRight * OuterRayLength;

		// 11개 정점 생성 (왼쪽 5개 + 중앙 1개 + 오른쪽 5개)
		TArray<FVector> ArcPoints;
		const int NumArcPoints = 11;

		for (int i = 0; i < NumArcPoints; ++i)
		{
			float t = static_cast<float>(i) / (NumArcPoints - 1); // 0.0 ~ 1.0

			FVector Point;
			if (t <= 0.5f)
			{
				// 왼쪽 끝점 -> 중앙 정점 (구면 보간)
				float localT = t * 2.0f; // 0.0 ~ 1.0
				FVector DirLeft = (LeftEndPoint - ApexWorld).GetNormalized();
				FVector DirCenter = (CenterPoint - ApexWorld).GetNormalized();

				// Slerp (구면 선형 보간)
				float cosAngle = FVector::Dot(DirLeft, DirCenter);
				float angle = std::acos(FMath::Clamp(cosAngle, -1.0f, 1.0f));

				if (angle < 0.001f)
				{
					// 각도가 매우 작으면 선형 보간 사용
					Point = FVector::Lerp(LeftEndPoint, CenterPoint, localT);
				}
				else
				{
					float sinAngle = std::sin(angle);
					float ratio1 = std::sin((1.0f - localT) * angle) / sinAngle;
					float ratio2 = std::sin(localT * angle) / sinAngle;
					FVector Dir = (DirLeft * ratio1 + DirCenter * ratio2).GetNormalized();

					// 거리도 보간
					float distance = FMath::Lerp(OuterRayLength, AttenuationRadius, localT);
					Point = ApexWorld + Dir * distance;
				}
			}
			else
			{
				// 중앙 정점 -> 오른쪽 끝점 (구면 보간)
				float localT = (t - 0.5f) * 2.0f; // 0.0 ~ 1.0
				FVector DirCenter = (CenterPoint - ApexWorld).GetNormalized();
				FVector DirRight = (RightEndPoint - ApexWorld).GetNormalized();

				// Slerp (구면 선형 보간)
				float cosAngle = FVector::Dot(DirCenter, DirRight);
				float angle = std::acos(FMath::Clamp(cosAngle, -1.0f, 1.0f));

				if (angle < 0.001f)
				{
					// 각도가 매우 작으면 선형 보간 사용
					Point = FVector::Lerp(CenterPoint, RightEndPoint, localT);
				}
				else
				{
					float sinAngle = std::sin(angle);
					float ratio1 = std::sin((1.0f - localT) * angle) / sinAngle;
					float ratio2 = std::sin(localT * angle) / sinAngle;
					FVector Dir = (DirCenter * ratio1 + DirRight * ratio2).GetNormalized();

					// 거리도 보간
					float distance = FMath::Lerp(AttenuationRadius, OuterRayLength, localT);
					Point = ApexWorld + Dir * distance;
				}
			}

			ArcPoints.Add(Point);
		}

		// 정점들을 선으로 연결
		for (int i = 0; i < NumArcPoints - 1; ++i)
		{
			StartPoints.Add(ArcPoints[i]);
			EndPoints.Add(ArcPoints[i + 1]);
			Colors.Add(OuterConeColor);
		}
	}

	// Right축 기준 아크
	{
		// 가운데 정점 (가장 볼록한 부분) - LightDirection 방향으로 AttenuationRadius 거리
		const FVector CenterPoint = ApexWorld + LightDirection * AttenuationRadius;

		// 양쪽 끝점 (방사형 라인의 끝점과 동일)
		const FVector RayDirLeft = (LightDirection * std::cos(OuterAngleRad) + Right * std::sin(OuterAngleRad)).GetNormalized();
		const FVector RayDirRight = (LightDirection * std::cos(OuterAngleRad) - Right * std::sin(OuterAngleRad)).GetNormalized();
		const FVector LeftEndPoint = ApexWorld + RayDirLeft * OuterRayLength;
		const FVector RightEndPoint = ApexWorld + RayDirRight * OuterRayLength;

		// 11개 정점 생성 (왼쪽 5개 + 중앙 1개 + 오른쪽 5개)
		TArray<FVector> ArcPoints;
		const int NumArcPoints = 11;

		for (int i = 0; i < NumArcPoints; ++i)
		{
			float t = static_cast<float>(i) / (NumArcPoints - 1); // 0.0 ~ 1.0

			FVector Point;
			if (t <= 0.5f)
			{
				// 왼쪽 끝점 -> 중앙 정점 (구면 보간)
				float localT = t * 2.0f; // 0.0 ~ 1.0
				FVector DirLeft = (LeftEndPoint - ApexWorld).GetNormalized();
				FVector DirCenter = (CenterPoint - ApexWorld).GetNormalized();

				// Slerp (구면 선형 보간)
				float cosAngle = FVector::Dot(DirLeft, DirCenter);
				float angle = std::acos(FMath::Clamp(cosAngle, -1.0f, 1.0f));

				if (angle < 0.001f)
				{
					// 각도가 매우 작으면 선형 보간 사용
					Point = FVector::Lerp(LeftEndPoint, CenterPoint, localT);
				}
				else
				{
					float sinAngle = std::sin(angle);
					float ratio1 = std::sin((1.0f - localT) * angle) / sinAngle;
					float ratio2 = std::sin(localT * angle) / sinAngle;
					FVector Dir = (DirLeft * ratio1 + DirCenter * ratio2).GetNormalized();

					// 거리도 보간
					float distance = FMath::Lerp(OuterRayLength, AttenuationRadius, localT);
					Point = ApexWorld + Dir * distance;
				}
			}
			else
			{
				// 중앙 정점 -> 오른쪽 끝점 (구면 보간)
				float localT = (t - 0.5f) * 2.0f; // 0.0 ~ 1.0
				FVector DirCenter = (CenterPoint - ApexWorld).GetNormalized();
				FVector DirRight = (RightEndPoint - ApexWorld).GetNormalized();

				// Slerp (구면 선형 보간)
				float cosAngle = FVector::Dot(DirCenter, DirRight);
				float angle = std::acos(FMath::Clamp(cosAngle, -1.0f, 1.0f));

				if (angle < 0.001f)
				{
					// 각도가 매우 작으면 선형 보간 사용
					Point = FVector::Lerp(CenterPoint, RightEndPoint, localT);
				}
				else
				{
					float sinAngle = std::sin(angle);
					float ratio1 = std::sin((1.0f - localT) * angle) / sinAngle;
					float ratio2 = std::sin(localT * angle) / sinAngle;
					FVector Dir = (DirCenter * ratio1 + DirRight * ratio2).GetNormalized();

					// 거리도 보간
					float distance = FMath::Lerp(AttenuationRadius, OuterRayLength, localT);
					Point = ApexWorld + Dir * distance;
				}
			}

			ArcPoints.Add(Point);
		}

		// 정점들을 선으로 연결
		for (int i = 0; i < NumArcPoints - 1; ++i)
		{
			StartPoints.Add(ArcPoints[i]);
			EndPoints.Add(ArcPoints[i + 1]);
			Colors.Add(OuterConeColor);
		}
	}

	// --- InnerCone 그리기 (선택적) ---
	if (InnerConeAngle > 0.0f && InnerConeAngle <= OuterConeAngle)
	{
		const float InnerAngleRad = DegreesToRadians(InnerConeAngle);

		// 방사형 라인 길이는 AttenuationRadius로 고정
		const float InnerRayLength = AttenuationRadius;

		// 아크의 가장 먼 지점이 AttenuationRadius가 되도록 ArcRadius 계산
		const float InnerArcRadius = (InnerAngleRad < HALF_PI && std::cos(InnerAngleRad) > 0.0001f)
			? AttenuationRadius / std::cos(InnerAngleRad)
			: AttenuationRadius;

		// 밑면 원을 위한 끝점들 저장
		TArray<FVector> InnerEndPoints;
		InnerEndPoints.Reserve(NumRays);

		// 1. 방사형 라인 (Apex에서 원뿔 가장자리로) - 길이 고정
		for (int i = 0; i < NumRays; ++i)
		{
			const float Angle = (static_cast<float>(i) / NumRays) * TWO_PI;
			const FVector RadialDir = (Right * std::cos(Angle) + ActualUp * std::sin(Angle)).GetNormalized();

			const FVector RayDir = (LightDirection * std::cos(InnerAngleRad) + RadialDir * std::sin(InnerAngleRad)).GetNormalized();
			const FVector RayEnd = ApexWorld + RayDir * InnerRayLength;

			StartPoints.Add(ApexWorld);
			EndPoints.Add(RayEnd);
			Colors.Add(InnerConeColor);

			InnerEndPoints.Add(RayEnd);
		}

		// 밑면 원 그리기 (방사형 라인의 끝점들을 연결)
		for (int i = 0; i < NumRays; ++i)
		{
			StartPoints.Add(InnerEndPoints[i]);
			EndPoints.Add(InnerEndPoints[(i + 1) % NumRays]);
			Colors.Add(InnerConeColor);
		}

		// InnerCone은 아크를 그리지 않음
	}		

	RenderDebugFrustum(StartPoints, EndPoints, Colors);
	
	// 렌더러에 라인 데이터 전달
	Renderer->AddLines(StartPoints, EndPoints, Colors);
}

void USpotLightComponent::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	Super::Serialize(bInIsLoading, InOutHandle);

	if (bInIsLoading)
	{
		ValidateConeAngles();
	}
}

void USpotLightComponent::DuplicateSubObjects()
{
	Super::DuplicateSubObjects();
	DirectionGizmo = nullptr;
}
