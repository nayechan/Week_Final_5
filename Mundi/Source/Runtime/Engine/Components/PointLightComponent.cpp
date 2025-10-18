#include "pch.h"
#include "PointLightComponent.h"
#include "BillboardComponent.h"

IMPLEMENT_CLASS(UPointLightComponent)

BEGIN_PROPERTIES(UPointLightComponent)
	MARK_AS_COMPONENT("포인트 라이트", "포인트 라이트 컴포넌트를 추가합니다.")
	ADD_PROPERTY_RANGE(float, SourceRadius, "Light", 0.0f, 1000.0f, false, "광원의 반경입니다.")
END_PROPERTIES()

UPointLightComponent::UPointLightComponent()
{
	SourceRadius = 0.0f;
}

UPointLightComponent::~UPointLightComponent()
{
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
	Info.Padding = FVector2D(0.0f, 0.0f); // 패딩 초기화

	return Info;
}

void UPointLightComponent::UpdateLightData()
{
	Super::UpdateLightData();
	// 점광원 특화 업데이트 로직
}


void UPointLightComponent::OnRegister()
{
	Super_t::OnRegister();
	SpriteComponent->SetTextureName("Data/UI/Icons/PointLight_64x.png");
}

void UPointLightComponent::RenderDebugVolume(URenderer* Renderer, const FMatrix& View, const FMatrix& Proj) const
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

	// 리플렉션 기반 자동 직렬화 (이 클래스의 프로퍼티만)
	AutoSerialize(bInIsLoading, InOutHandle, UPointLightComponent::StaticClass());
}

void UPointLightComponent::DuplicateSubObjects()
{
	Super::DuplicateSubObjects();
}
