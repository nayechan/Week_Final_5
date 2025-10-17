#include "pch.h"
#include "AmbientLightComponent.h"

IMPLEMENT_CLASS(UAmbientLightComponent)

UAmbientLightComponent::UAmbientLightComponent()
{
}

UAmbientLightComponent::~UAmbientLightComponent()
{
}

FAmbientLightInfo UAmbientLightComponent::GetLightInfo() const
{
	FAmbientLightInfo Info;
	Info.Color = GetLightColor();
	Info.Intensity = GetIntensity();
	Info.Padding = FVector(0.0f, 0.0f, 0.0f);
	return Info;
}

void UAmbientLightComponent::UpdateLightData()
{
	Super::UpdateLightData();
	// 환경광 특화 업데이트 로직
}

void UAmbientLightComponent::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	Super::Serialize(bInIsLoading, InOutHandle);
	// 추가 속성 없음 (베이스 클래스의 Intensity와 LightColor 사용)
}

void UAmbientLightComponent::DuplicateSubObjects()
{
	Super::DuplicateSubObjects();
}
