#include "pch.h"
#include "LocalLightComponent.h"
// IMPLEMENT_CLASS is now auto-generated in .generated.cpp
ULocalLightComponent::ULocalLightComponent()
{
}

ULocalLightComponent::~ULocalLightComponent()
{
}

float ULocalLightComponent::GetAttenuationFactor(const FVector& WorldPosition) const
{
	FVector LightPosition = GetWorldLocation();
	float Distance = FVector::Distance(LightPosition, WorldPosition);

	if (Distance >= AttenuationRadius)
	{
		return 0.0f;
	}

	// 거리 기반 감쇠 계산
	float NormalizedDistance = Distance / AttenuationRadius;
	float Attenuation = 1.0f - pow(NormalizedDistance, FalloffExponent);

	return FMath::Max(0.0f, Attenuation);
}

void ULocalLightComponent::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	Super::Serialize(bInIsLoading, InOutHandle);

}

void ULocalLightComponent::DuplicateSubObjects()
{
	Super::DuplicateSubObjects();
}
