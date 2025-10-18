#include "pch.h"
#include "LocalLightComponent.h"

IMPLEMENT_CLASS(ULocalLightComponent)

BEGIN_PROPERTIES(ULocalLightComponent)
	MARK_AS_COMPONENT("로컬 라이트", "위치 기반 로컬 라이트 컴포넌트입니다.")
	ADD_PROPERTY_RANGE(float, AttenuationRadius, "Light", 0.0f, 10000.0f, true, "감쇠 반경입니다.")
	ADD_PROPERTY(bool, bUseAttenuationCoefficients, "Light", true, "감쇠 방식 선택: true = Attenuation 사용, false = FalloffExponent 사용.")
	ADD_PROPERTY_RANGE(float, FalloffExponent, "Light", 0.1f, 10.0f, true, "감쇠 지수입니다 (bUseAttenuationCoefficients = false일 때 사용).")
	ADD_PROPERTY(FVector, Attenuation, "Light", true, "감쇠 계수입니다: 상수, 일차항, 이차항 (bUseAttenuationCoefficients = true일 때 사용).")
END_PROPERTIES()

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

	// 리플렉션 기반 자동 직렬화
	AutoSerialize(bInIsLoading, InOutHandle, ULocalLightComponent::StaticClass());
}

void ULocalLightComponent::DuplicateSubObjects()
{
	Super::DuplicateSubObjects();
}
