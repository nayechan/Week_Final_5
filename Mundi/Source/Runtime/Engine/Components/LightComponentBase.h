#pragma once
#include "SceneComponent.h"
#include "Color.h"
#include "ObjectMacros.h"

// 모든 라이트 컴포넌트의 베이스 클래스
class ULightComponentBase : public USceneComponent
{
public:
	DECLARE_CLASS(ULightComponentBase, USceneComponent)
	GENERATED_REFLECTION_BODY()

	ULightComponentBase();
	virtual ~ULightComponentBase() override;

public:
	// Light Properties
	void SetEnabled(bool bInEnabled) { bIsEnabled = bInEnabled; }
	bool IsEnabled() const { return bIsEnabled; }

	void SetIntensity(float InIntensity) { Intensity = InIntensity; }
	float GetIntensity() const { return Intensity; }

	void SetLightColor(const FLinearColor& InColor) { LightColor = InColor; }
	const FLinearColor& GetLightColor() const { return LightColor; }

	// Virtual Interface
	virtual void UpdateLightData();

	// Serialization & Duplication
	virtual void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;
	virtual void DuplicateSubObjects() override;
	DECLARE_DUPLICATE(ULightComponentBase)

protected:
	bool bIsEnabled = true;
	float Intensity = 1.0f;
	FLinearColor LightColor = FLinearColor(1.0f, 1.0f, 1.0f, 1.0f);
};
