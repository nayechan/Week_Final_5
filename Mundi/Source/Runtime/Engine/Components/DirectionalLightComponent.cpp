#include "pch.h"
#include "DirectionalLightComponent.h"

UDirectionalLightComponent::UDirectionalLightComponent()
{
}

UDirectionalLightComponent::~UDirectionalLightComponent()
{
}

IMPLEMENT_CLASS(UDirectionalLightComponent)

BEGIN_PROPERTIES(UDirectionalLightComponent)
	MARK_AS_COMPONENT("디렉셔널 라이트", "방향성 라이트 (태양광 같은 평행광) 컴포넌트입니다.")
END_PROPERTIES()

FVector UDirectionalLightComponent::GetLightDirection() const
{
	// Z-Up Left-handed 좌표계에서 Forward는 X축
	FQuat Rotation = GetWorldRotation();
	return Rotation.RotateVector(FVector(1.0f, 0.0f, 0.0f));
}

FDirectionalLightInfo UDirectionalLightComponent::GetLightInfo() const
{
	FDirectionalLightInfo Info;
	// Use GetLightColorWithIntensity() to include Temperature + Intensity
	Info.Color = GetLightColorWithIntensity();
	Info.Direction = GetLightDirection();
	Info.Padding = 0.0f; // 패딩 초기화

	return Info;
}

void UDirectionalLightComponent::UpdateLightData()
{
	Super::UpdateLightData();
	// 방향성 라이트 특화 업데이트 로직
}

void UDirectionalLightComponent::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	Super::Serialize(bInIsLoading, InOutHandle);

	// 리플렉션 기반 자동 직렬화 (추가 프로퍼티 없음)
	AutoSerialize(bInIsLoading, InOutHandle, UDirectionalLightComponent::StaticClass());
}

void UDirectionalLightComponent::DuplicateSubObjects()
{
	Super::DuplicateSubObjects();
}
