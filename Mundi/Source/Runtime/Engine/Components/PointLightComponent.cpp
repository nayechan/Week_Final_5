#include "pch.h"
#include "PointLightComponent.h"

IMPLEMENT_CLASS(UPointLightComponent)

BEGIN_PROPERTIES(UPointLightComponent)
	MARK_AS_COMPONENT("포인트 라이트", "포인트 라이트 컴포넌트를 추가합니다.")
	ADD_PROPERTY_RANGE(float, SourceRadius, "Light", 0.0f, 1000.0f, true, "광원의 반경입니다.")
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
	Info.AttenuationRadius = GetAttenuationRadius();  // Moved up for optimal packing
	Info.Attenuation = IsUsingAttenuationCoefficients() ? GetAttenuation() : FVector(1.0f, 0.0f, 0.0f);
	Info.FalloffExponent = IsUsingAttenuationCoefficients() ? 0.0f : GetFalloffExponent();
	Info.bUseAttenuationCoefficients = IsUsingAttenuationCoefficients() ? 1u : 0u;
	Info.Padding = FVector(0.0f, 0.0f, 0.0f); // 패딩 초기화

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

	// 리플렉션 기반 자동 직렬화 (이 클래스의 프로퍼티만)
	AutoSerialize(bInIsLoading, InOutHandle, UPointLightComponent::StaticClass());
}

void UPointLightComponent::DuplicateSubObjects()
{
	Super::DuplicateSubObjects();
}
