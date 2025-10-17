#pragma once
#include "LightComponent.h"
#include "LightInfo.h"

// 방향성 라이트 (태양광 같은 평행광)
class UDirectionalLightComponent : public ULightComponent
{
public:
	DECLARE_CLASS(UDirectionalLightComponent, ULightComponent)

	UDirectionalLightComponent();
	virtual ~UDirectionalLightComponent() override;

public:
	// 월드 회전을 반영한 라이트 방향 반환 (Transform의 Forward 벡터)
	FVector GetLightDirection() const;

	// Light Info
	FDirectionalLightInfo GetLightInfo() const;

	// Virtual Interface
	virtual void UpdateLightData() override;

	// Serialization & Duplication
	virtual void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;
	virtual void DuplicateSubObjects() override;
	DECLARE_DUPLICATE(UDirectionalLightComponent)
};
