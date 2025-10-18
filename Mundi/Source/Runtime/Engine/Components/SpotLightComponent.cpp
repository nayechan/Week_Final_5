#include "pch.h"
#include "SpotLightComponent.h"

IMPLEMENT_CLASS(USpotLightComponent)

BEGIN_PROPERTIES(USpotLightComponent)
	MARK_AS_COMPONENT("스포트 라이트", "스포트 라이트 컴포넌트를 추가합니다.")
	ADD_PROPERTY_RANGE(float, InnerConeAngle, "Light", 0.0f, 90.0f, true, "원뿔 내부 각도입니다. 이 각도 안에서는 빛이 최대 밝기로 표시됩니다.")
	ADD_PROPERTY_RANGE(float, OuterConeAngle, "Light", 0.0f, 90.0f, true, "원뿔 외부 각도입니다. 이 각도 안에서는 빛이 보이지 않습니다..")
END_PROPERTIES()

USpotLightComponent::USpotLightComponent()
{
	InnerConeAngle = 30.0f;
	OuterConeAngle = 45.0f;
}

USpotLightComponent::~USpotLightComponent()
{
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
	Info.Attenuation = IsUsingAttenuationCoefficients() ? GetAttenuation() : FVector(1.0f, 0.0f, 0.0f);
	Info.AttenuationRadius = GetAttenuationRadius();
	Info.FalloffExponent = IsUsingAttenuationCoefficients() ? 0.0f : GetFalloffExponent();
	Info.bUseAttenuationCoefficients = IsUsingAttenuationCoefficients() ? 1u : 0u;
	Info.Padding = FVector2D(0.0f, 0.0f); // 패딩 초기화

	return Info;
}

void USpotLightComponent::UpdateLightData()
{
	Super::UpdateLightData();
	// 스포트라이트 특화 업데이트 로직
}

void USpotLightComponent::RenderDebugVolume(URenderer* Renderer, const FMatrix& View, const FMatrix& Proj) const
{
	// SpotLight의 원뿔 볼륨을 라인으로 렌더링
	const FVector4 InnerConeColor(1.0f, 1.0f, 0.0f, 1.0f); // 노란색 (InnerCone)
	const FVector4 OuterConeColor(1.0f, 0.5f, 0.0f, 1.0f); // 주황색 (OuterCone)

	TArray<FVector> StartPoints;
	TArray<FVector> EndPoints;
	TArray<FVector4> Colors;

	// 원뿔의 정점(apex)은 라이트의 위치
	const FVector ApexWorld = GetWorldLocation();
	const FVector LightDirection = GetDirection();

	// 원뿔의 길이는 AttenuationRadius
	const float ConeLength = GetAttenuationRadius();

	// 원뿔 밑면의 중심점
	const FVector BaseCenterWorld = ApexWorld + LightDirection * ConeLength;

	// 원뿔 밑면을 그리기 위한 세그먼트 수
	const int NumBaseSegments = 32;

	// --- OuterCone 그리기 ---
	const float OuterAngleRad = DegreesToRadians(OuterConeAngle);
	const float OuterBaseRadius = ConeLength * std::tan(OuterAngleRad);

	// LightDirection에 수직인 두 벡터 구하기 (원뿔 밑면을 그리기 위한 basis)
	FVector Up = FVector(0.0f, 0.0f, 1.0f);
	if (std::abs(FVector::Dot(LightDirection, Up)) > 0.99f)
	{
		Up = FVector(0.0f, 1.0f, 0.0f); // LightDirection이 거의 Z축과 평행하면 Y축 사용
	}
	FVector Right = FVector::Cross(LightDirection, Up).GetNormalized();
	FVector ActualUp = FVector::Cross(Right, LightDirection).GetNormalized();

	// OuterCone 밑면의 점들
	TArray<FVector> OuterBasePoints;
	OuterBasePoints.Reserve(NumBaseSegments);
	for (int i = 0; i < NumBaseSegments; ++i)
	{
		const float Angle = (static_cast<float>(i) / NumBaseSegments) * TWO_PI;
		const FVector Offset = Right * (OuterBaseRadius * std::cos(Angle)) +
		                       ActualUp * (OuterBaseRadius * std::sin(Angle));
		OuterBasePoints.Add(BaseCenterWorld + Offset);
	}

	// Apex에서 밑면으로 가는 라인들 (OuterCone)
	for (int i = 0; i < NumBaseSegments; ++i)
	{
		StartPoints.Add(ApexWorld);
		EndPoints.Add(OuterBasePoints[i]);
		Colors.Add(OuterConeColor);
	}

	// 밑면 원 그리기 (OuterCone)
	for (int i = 0; i < NumBaseSegments; ++i)
	{
		StartPoints.Add(OuterBasePoints[i]);
		EndPoints.Add(OuterBasePoints[(i + 1) % NumBaseSegments]);
		Colors.Add(OuterConeColor);
	}

	// --- InnerCone 그리기 (선택적) ---
	if (InnerConeAngle > 0.0f && InnerConeAngle < OuterConeAngle)
	{
		const float InnerAngleRad = DegreesToRadians(InnerConeAngle);
		const float InnerBaseRadius = ConeLength * std::tan(InnerAngleRad);

		// InnerCone 밑면의 점들
		TArray<FVector> InnerBasePoints;
		InnerBasePoints.Reserve(NumBaseSegments);
		for (int i = 0; i < NumBaseSegments; ++i)
		{
			const float Angle = (static_cast<float>(i) / NumBaseSegments) * TWO_PI;
			const FVector Offset = Right * (InnerBaseRadius * std::cos(Angle)) +
			                       ActualUp * (InnerBaseRadius * std::sin(Angle));
			InnerBasePoints.Add(BaseCenterWorld + Offset);
		}

		// 밑면 원 그리기만 (InnerCone - Apex 라인은 생략하여 깔끔하게)
		for (int i = 0; i < NumBaseSegments; ++i)
		{
			StartPoints.Add(InnerBasePoints[i]);
			EndPoints.Add(InnerBasePoints[(i + 1) % NumBaseSegments]);
			Colors.Add(InnerConeColor);
		}
	}

	// 렌더러에 라인 데이터 전달
	Renderer->AddLines(StartPoints, EndPoints, Colors);
}

void USpotLightComponent::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	Super::Serialize(bInIsLoading, InOutHandle);

	// 리플렉션 기반 자동 직렬화 (이 클래스의 프로퍼티만)
	AutoSerialize(bInIsLoading, InOutHandle, USpotLightComponent::StaticClass());
}

void USpotLightComponent::DuplicateSubObjects()
{
	Super::DuplicateSubObjects();
}
