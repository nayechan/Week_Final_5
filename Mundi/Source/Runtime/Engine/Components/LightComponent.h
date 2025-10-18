#pragma once
#include "LightComponentBase.h"

// 실제 조명 효과를 가진 라이트들의 공통 베이스
class ULightComponent : public ULightComponentBase
{
public:
	DECLARE_CLASS(ULightComponent, ULightComponentBase)
	GENERATED_REFLECTION_BODY()

	ULightComponent();
	virtual ~ULightComponent() override;

public:
	// Temperature
	void SetTemperature(float InTemperature) { Temperature = InTemperature; }
	float GetTemperature() const { return Temperature; }

	// 색상과 강도를 합쳐서 반환
	virtual FLinearColor GetLightColorWithIntensity() const;

	// Serialization & Duplication
	virtual void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;
	virtual void DuplicateSubObjects() override;
	DECLARE_DUPLICATE(ULightComponent)

protected:
	float Temperature = 6500.0f; // 색온도 (K)
};
