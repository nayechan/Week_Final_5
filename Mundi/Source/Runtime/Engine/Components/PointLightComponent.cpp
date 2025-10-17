#include "pch.h"
#include "PointLightComponent.h"

IMPLEMENT_CLASS(UPointLightComponent)

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
	Info.Color = GetLightColor();
	Info.Position = GetWorldLocation();
	Info.FalloffExponent = IsUsingAttenuationCoefficients() ? 0.0f : GetFalloffExponent();
	Info.Attenuation = IsUsingAttenuationCoefficients() ? GetAttenuation() : FVector(1.0f, 0.0f, 0.0f);
	Info.AttenuationRadius = GetAttenuationRadius();
	Info.Intensity = GetIntensity();
	Info.bUseAttenuationCoefficients = IsUsingAttenuationCoefficients() ? 1u : 0u;
	Info.Padding = FVector2D(0.0f, 0.0f);
	return Info;
}

void UPointLightComponent::UpdateLightData()
{
	Super::UpdateLightData();
	// 점광원 특화 업데이트 로직
}

void UPointLightComponent::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	Super::Serialize(bInIsLoading, InOutHandle);

	if (bInIsLoading)
	{
		FJsonSerializer::ReadFloat(InOutHandle, "SourceRadius", SourceRadius, 0.0f);
	}
	else
	{
		InOutHandle["SourceRadius"] = SourceRadius;
	}
}

void UPointLightComponent::DuplicateSubObjects()
{
	Super::DuplicateSubObjects();
}
