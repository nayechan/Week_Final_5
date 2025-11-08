#include "pch.h"
#include "LightComponentBase.h"
// IMPLEMENT_CLASS is now auto-generated in .generated.cpp
ULightComponentBase::ULightComponentBase()
{
	bWantsOnUpdateTransform = true;
	Intensity = 1.0f;
	bCastShadows = true;
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

}

void ULightComponentBase::DuplicateSubObjects()
{
	Super::DuplicateSubObjects();
}
