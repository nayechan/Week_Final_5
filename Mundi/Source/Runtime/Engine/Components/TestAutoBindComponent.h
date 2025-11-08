#pragma once

#include "Object.h"
#include "SceneComponent.h"
#include "UTestAutoBindComponent.generated.h"

/**
 * 자동 바인딩 테스트용 컴포넌트
 * UPROPERTY와 UFUNCTION 매크로 테스트
 */
class UTestAutoBindComponent : public USceneComponent
{

	GENERATED_REFLECTION_BODY()

public:
	UTestAutoBindComponent() = default;
	virtual ~UTestAutoBindComponent() = default;

	// ===== Auto-bound Properties =====

	UPROPERTY(EditAnywhere, Category="Test", Tooltip="Test intensity value")
	float Intensity = 1.0f;

	UPROPERTY(EditAnywhere, Category="Test", Range="0,100", Tooltip="Test percentage")
	float Percentage = 50.0f;

	UPROPERTY(EditAnywhere, Category="Test")
	bool bEnabled = true;

	UPROPERTY(EditAnywhere, Category="Test")
	FVector Position = FVector(0, 0, 0);

	// ===== Auto-bound Methods =====

	UFUNCTION(LuaBind, DisplayName="SetIntensity")
	void SetTestIntensity(float Value)
	{
		Intensity = Value;
	}

	UFUNCTION(LuaBind, DisplayName="GetIntensity")
	float GetTestIntensity() const
	{
		return Intensity;
	}

	UFUNCTION(LuaBind, DisplayName="Enable")
	void EnableComponent()
	{
		bEnabled = true;
	}

	UFUNCTION(LuaBind, DisplayName="Disable")
	void DisableComponent()
	{
		bEnabled = false;
	}
};
