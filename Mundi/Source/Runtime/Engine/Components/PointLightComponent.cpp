#include "pch.h"
#include "PointLightComponent.h"
#include "BillboardComponent.h"
// IMPLEMENT_CLASS is now auto-generated in .generated.cpp
UPointLightComponent::UPointLightComponent()
{
	SourceRadius = 0.0f;
}

UPointLightComponent::~UPointLightComponent()
{
}

void UPointLightComponent::GetShadowRenderRequests(FSceneView* View, TArray<FShadowRenderRequest>& OutRequests)
{
	// 라이트의 월드 위치와 영향 반경 가져오기
	FVector LightPosition = this->GetWorldLocation();
	float LightRadius = this->GetAttenuationRadius();

	// 큐브맵 각 면에 대한 뷰 행렬 생성 (X=Forward, Y=Right, Z=Up)
	FMatrix LightViews[6];

	// Face 0: +X
	LightViews[0] = FMatrix::LookAtLH(LightPosition, LightPosition + FVector(0, 1, 0), FVector(0, 0, 1));
	// Face 1: -X 
	LightViews[1] = FMatrix::LookAtLH(LightPosition, LightPosition + FVector(0, -1, 0), FVector(0, 0, 1));
	// Face 2: +Y 
	LightViews[2] = FMatrix::LookAtLH(LightPosition, LightPosition + FVector(0, 0, 1), FVector(-1, 0, 0));
	// Face 3: -Y
	LightViews[3] = FMatrix::LookAtLH(LightPosition, LightPosition + FVector(0, 0, -1), FVector(1, 0, 0));
	// Face 4: +Z
	LightViews[4] = FMatrix::LookAtLH(LightPosition, LightPosition + FVector(1, 0, 0), FVector(0, 0, 1));
	// Face 5: -Z
	LightViews[5] = FMatrix::LookAtLH(LightPosition, LightPosition + FVector(-1, 0, 0), FVector(0, 0, 1));

	// 큐브맵 프로젝션 행렬 생성 (90도 FOV)
	const float FovRadians = HALF_PI;  // 90 degrees in radians
	FMatrix LightProjection = FMatrix::PerspectiveFovLH(FovRadians, 1.0f, 0.1f, LightRadius);

	for (uint32 i = 0; i < 6; i++)
	{
		FShadowRenderRequest ShadowRenderRequest;
		ShadowRenderRequest.LightOwner = this;
		ShadowRenderRequest.ViewMatrix = LightViews[i];
		ShadowRenderRequest.ProjectionMatrix = LightProjection;
		ShadowRenderRequest.Size = ShadowResolutionScale;
		ShadowRenderRequest.SubViewIndex = i;
		ShadowRenderRequest.AtlasScaleOffset = 0;
		OutRequests.Add(ShadowRenderRequest);
	}
}

FPointLightInfo UPointLightComponent::GetLightInfo() const
{
	FPointLightInfo Info;
	// Use GetLightColorWithIntensity() to include Temperature + Intensity
	Info.Color = GetLightColorWithIntensity();
	Info.Position = GetWorldLocation();
	Info.AttenuationRadius = GetAttenuationRadius();
	Info.FalloffExponent = GetFalloffExponent(); // Always pass FalloffExponent (used when bUseInverseSquareFalloff = false)
	Info.bUseInverseSquareFalloff = IsUsingInverseSquareFalloff() ? 1u : 0u;
	Info.bCastShadows = 0u;		// UpdateLightBuffer 에서 초기화 해줌
	Info.ShadowArrayIndex = -1; // UpdateLightBuffer 에서 초기화 해줌

	Info.ShadowBias = GetShadowBias();
	Info.ShadowSlopeBias = GetShadowSlopeBias();
	Info.ShadowSharpen = GetShadowSharpen();

	return Info;
}

void UPointLightComponent::UpdateLightData()
{
	Super::UpdateLightData();
	// 점광원 특화 업데이트 로직
	GWorld->GetLightManager()->UpdateLight(this);
}

void UPointLightComponent::OnTransformUpdated()
{
	Super::OnTransformUpdated();
	GWorld->GetLightManager()->UpdateLight(this);
}


void UPointLightComponent::OnRegister(UWorld* InWorld)
{
	Super::OnRegister(InWorld);
	if (SpriteComponent)
	{
		SpriteComponent->SetTexture(GDataDir + "/UI/Icons/PointLight_64x.png");
	}
	InWorld->GetLightManager()->RegisterLight(this);
}

void UPointLightComponent::OnUnregister()
{
	GWorld->GetLightManager()->DeRegisterLight(this);

	Super::OnUnregister();
}

void UPointLightComponent::RenderDebugVolume(URenderer* Renderer) const
{
	// PointLight의 구형 볼륨을 3개의 원으로 렌더링 (XY, XZ, YZ 평면)
	const FVector4 CircleColor(1.0f, 0.8f, 0.2f, 1.0f); // 노란색-주황색

	TArray<FVector> StartPoints;
	TArray<FVector> EndPoints;
	TArray<FVector4> Colors;

	// 점광원의 중심은 라이트의 위치
	const FVector CenterWorld = GetWorldLocation();

	// 원의 반지름은 AttenuationRadius
	const float Radius = GetAttenuationRadius();

	// 원을 그리기 위한 세그먼트 수
	const int NumSegments = 32;

	// --- XY 평면 원 (Z축 기준) ---
	for (int i = 0; i < NumSegments; ++i)
	{
		const float Angle1 = (static_cast<float>(i) / NumSegments) * TWO_PI;
		const float Angle2 = (static_cast<float>((i + 1) % NumSegments) / NumSegments) * TWO_PI;

		const FVector Point1 = CenterWorld + FVector(
			Radius * std::cos(Angle1),
			Radius * std::sin(Angle1),
			0.0f
		);

		const FVector Point2 = CenterWorld + FVector(
			Radius * std::cos(Angle2),
			Radius * std::sin(Angle2),
			0.0f
		);

		StartPoints.Add(Point1);
		EndPoints.Add(Point2);
		Colors.Add(CircleColor);
	}

	// --- XZ 평면 원 (Y축 기준) ---
	for (int i = 0; i < NumSegments; ++i)
	{
		const float Angle1 = (static_cast<float>(i) / NumSegments) * TWO_PI;
		const float Angle2 = (static_cast<float>((i + 1) % NumSegments) / NumSegments) * TWO_PI;

		const FVector Point1 = CenterWorld + FVector(
			Radius * std::cos(Angle1),
			0.0f,
			Radius * std::sin(Angle1)
		);

		const FVector Point2 = CenterWorld + FVector(
			Radius * std::cos(Angle2),
			0.0f,
			Radius * std::sin(Angle2)
		);

		StartPoints.Add(Point1);
		EndPoints.Add(Point2);
		Colors.Add(CircleColor);
	}

	// --- YZ 평면 원 (X축 기준) ---
	for (int i = 0; i < NumSegments; ++i)
	{
		const float Angle1 = (static_cast<float>(i) / NumSegments) * TWO_PI;
		const float Angle2 = (static_cast<float>((i + 1) % NumSegments) / NumSegments) * TWO_PI;

		const FVector Point1 = CenterWorld + FVector(
			0.0f,
			Radius * std::cos(Angle1),
			Radius * std::sin(Angle1)
		);

		const FVector Point2 = CenterWorld + FVector(
			0.0f,
			Radius * std::cos(Angle2),
			Radius * std::sin(Angle2)
		);

		StartPoints.Add(Point1);
		EndPoints.Add(Point2);
		Colors.Add(CircleColor);
	}

	// 렌더러에 라인 데이터 전달
	Renderer->AddLines(StartPoints, EndPoints, Colors);
}

void UPointLightComponent::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	Super::Serialize(bInIsLoading, InOutHandle);

}

void UPointLightComponent::DuplicateSubObjects()
{
	Super::DuplicateSubObjects();
}
