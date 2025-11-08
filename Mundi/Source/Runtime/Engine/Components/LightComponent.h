#pragma once

#include "LightComponentBase.h"
#include "ULightComponent.generated.h"

class FSceneView;

struct FShadowRenderRequest;

// 실제 조명 효과를 가진 라이트들의 공통 베이스
class ULightComponent : public ULightComponentBase
{
public:

	GENERATED_REFLECTION_BODY()

	ULightComponent();
	virtual ~ULightComponent() override;

public:
	// Temperature
	void SetTemperature(float InTemperature) { Temperature = InTemperature;}
	float GetTemperature() const { return Temperature; }

	// 색상과 강도를 합쳐서 반환
	virtual FLinearColor GetLightColorWithIntensity() const;
	void OnRegister(UWorld* InWorld) override;

	virtual void GetShadowRenderRequests(FSceneView* View, TArray<FShadowRenderRequest>& OutRequests) {};

	// Serialization & Duplication
	void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;
	virtual void DuplicateSubObjects() override;

	float GetShadowResolutionScale() const { return ShadowResolutionScale; }
	float GetShadowBias() const { return ShadowBias; }
	float GetShadowSlopeBias() const { return ShadowSlopeBias; }
	float GetShadowSharpen() const { return ShadowSharpen; }

protected:
	UPROPERTY(EditAnywhere, Category="Light", Range="1000.0, 15000.0", Tooltip="조명의 색온도를 켈빈(K) 단위로 설정합니다\n(1000K: 주황색, 6500K: 주광색, 15000K: 푸른색)")
	float Temperature = 6500.0f; // 색온도 (K)

	UPROPERTY(EditAnywhere, Category="Light", Range="512, 8192", Tooltip="Shadow Resolution Scale")
	int ShadowResolutionScale = 1024;

	UPROPERTY(EditAnywhere, Category="Light", Range="0.0, 0.01", Tooltip="Shadow Bias")
	float ShadowBias = 0.00001f;

	UPROPERTY(EditAnywhere, Category="Light", Range="0.0, 0.01", Tooltip="Shadow Slope Bias")
	float ShadowSlopeBias = 0.00001f;

	UPROPERTY(EditAnywhere, Category="Light", Range="0.0, 1.0", Tooltip="Shadow Sharpen - 0.0f(Soft) ~ 1.0f(Sharp)")
	float ShadowSharpen = 0.5f;	// 0.0f(Soft) ~ 1.0f(Sharp)
};
