#include "pch.h"
#include "SpotLightComponent.h"

IMPLEMENT_CLASS(USpotLightComponent)

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
	Info.Color = GetLightColor();
	Info.Position = GetWorldLocation();
	Info.InnerConeAngle = GetInnerConeAngle();
	Info.Direction = GetDirection();
	Info.OuterConeAngle = GetOuterConeAngle();
	Info.Attenuation = IsUsingAttenuationCoefficients() ? GetAttenuation() : FVector(1.0f, 0.0f, 0.0f);
	Info.AttenuationRadius = GetAttenuationRadius();
	Info.FalloffExponent = IsUsingAttenuationCoefficients() ? 0.0f : GetFalloffExponent();
	Info.Intensity = GetIntensity();
	Info.bUseAttenuationCoefficients = IsUsingAttenuationCoefficients() ? 1u : 0u;
	Info.Padding = 0.0f;
	return Info;
}

void USpotLightComponent::UpdateLightData()
{
	Super::UpdateLightData();
	// 스포트라이트 특화 업데이트 로직
}

void USpotLightComponent::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	Super::Serialize(bInIsLoading, InOutHandle);

	if (bInIsLoading)
	{
		FJsonSerializer::ReadFloat(InOutHandle, "InnerConeAngle", InnerConeAngle, 30.0f);
		FJsonSerializer::ReadFloat(InOutHandle, "OuterConeAngle", OuterConeAngle, 45.0f);
	}
	else
	{
		InOutHandle["InnerConeAngle"] = InnerConeAngle;
		InOutHandle["OuterConeAngle"] = OuterConeAngle;
	}
}

void USpotLightComponent::DuplicateSubObjects()
{
	Super::DuplicateSubObjects();
}
