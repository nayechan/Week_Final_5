#pragma once
#include "PointLightComponent.h"
#include "LightInfo.h"

// 스포트라이트 (원뿔 형태로 빛 방출)
class USpotLightComponent : public UPointLightComponent
{
public:
	DECLARE_CLASS(USpotLightComponent, UPointLightComponent)

	USpotLightComponent();
	virtual ~USpotLightComponent() override;

public:
	// Cone Angles
	void SetInnerConeAngle(float InAngle) { InnerConeAngle = InAngle; }
	float GetInnerConeAngle() const { return InnerConeAngle; }

	void SetOuterConeAngle(float InAngle) { OuterConeAngle = InAngle; }
	float GetOuterConeAngle() const { return OuterConeAngle; }

	// 스포트라이트 방향 (Transform의 Forward 벡터 사용)
	FVector GetDirection() const;

	// 원뿔 영역 감쇠 계산
	// 반환값: 0.0 (영향 없음) ~ 1.0 (최대 영향)
	// - 0.0: OuterConeAngle 밖, 원뿔 밖으로 빛이 닿지 않음
	// - 0.0 < x < 1.0: InnerConeAngle과 OuterConeAngle 사이, 부드러운 감쇠
	// - 1.0: InnerConeAngle 안, 원뿔 중심으로 최대 영향
	float GetConeAttenuation(const FVector& WorldPosition) const;

	// Light Info
	FSpotLightInfo GetLightInfo() const;

	// Virtual Interface
	virtual void UpdateLightData() override;

	// Serialization & Duplication
	virtual void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;
	virtual void DuplicateSubObjects() override;
	DECLARE_DUPLICATE(USpotLightComponent)

protected:
	float InnerConeAngle = 30.0f; // 내부 원뿔 각도
	float OuterConeAngle = 45.0f; // 외부 원뿔 각도
};
