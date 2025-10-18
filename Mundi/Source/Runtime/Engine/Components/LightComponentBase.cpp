#include "pch.h"
#include "LightComponentBase.h"

IMPLEMENT_CLASS(ULightComponentBase)

BEGIN_PROPERTIES(ULightComponentBase)
	MARK_AS_COMPONENT("라이트 베이스", "라이트 컴포넌트의 베이스 클래스입니다.")
	ADD_PROPERTY(bool, bIsEnabled, "Light", true, "라이트 활성화 여부입니다.")
	ADD_PROPERTY_RANGE(float, Intensity, "Light", 0.0f, 100.0f, true, "라이트의 강도입니다.")
	ADD_PROPERTY(FLinearColor, LightColor, "Light", true, "라이트의 색상입니다.")
END_PROPERTIES()

ULightComponentBase::ULightComponentBase()
{
	bIsEnabled = true;
	Intensity = 1.0f;
	LightColor = FLinearColor(1.0f, 1.0f, 1.0f, 1.0f);
}

ULightComponentBase::~ULightComponentBase()
{
}

void ULightComponentBase::UpdateLightData()
{
	// 자식 클래스에서 오버라이드
}

void ULightComponentBase::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	Super::Serialize(bInIsLoading, InOutHandle);

	// 리플렉션 기반 자동 직렬화
	AutoSerialize(bInIsLoading, InOutHandle, ULightComponentBase::StaticClass());
}

void ULightComponentBase::DuplicateSubObjects()
{
	Super::DuplicateSubObjects();
}
