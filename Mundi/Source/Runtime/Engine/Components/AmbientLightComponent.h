#pragma once

#include "LightComponent.h"
#include "LightManager.h"
#include "UAmbientLightComponent.generated.h"

// 환경광 (전역 조명)
class UAmbientLightComponent : public ULightComponent
{
public:

	GENERATED_REFLECTION_BODY()

	UAmbientLightComponent();
	virtual ~UAmbientLightComponent() override;

public:
	// Light Info
	FAmbientLightInfo GetLightInfo() const;

	// Virtual Interface
	virtual void UpdateLightData() override;
	void OnTransformUpdated() override;
	void OnRegister(UWorld* InWorld) override;
	void OnUnregister() override;

	// Serialization & Duplication
	void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;
	virtual void DuplicateSubObjects() override;

};
