#pragma once

#include "LocalLightComponent.h"
#include "LightManager.h"
#include "UPointLightComponent.generated.h"

// 점광원 (모든 방향으로 균등하게 빛 방출)
class UPointLightComponent : public ULocalLightComponent
{
public:

	GENERATED_REFLECTION_BODY()

	UPointLightComponent();
	virtual ~UPointLightComponent() override;

public:
	void GetShadowRenderRequests(FSceneView* View, TArray<FShadowRenderRequest>& OutRequests) override;

	// Source Radius
	void SetSourceRadius(float InRadius) { SourceRadius = InRadius; }
	float GetSourceRadius() const { return SourceRadius; }

	// Light Info
	FPointLightInfo GetLightInfo() const;

	// Virtual Interface
	virtual void UpdateLightData() override;
	void OnTransformUpdated() override;
	void OnRegister(UWorld* InWorld) override;
	void OnUnregister()	override;

	// Debug Rendering
	virtual void RenderDebugVolume(class URenderer* Renderer) const override;

	// Serialization & Duplication
	void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;
	virtual void DuplicateSubObjects() override;

	bool IsOverrideCameraLightPerspective() { return bOverrideCameraLightPerspective; }
	uint32 GetOverrideCameraLightNum() { return OverrideCameraLightNum; }

protected:
	UPROPERTY(EditAnywhere, Category="Light", Range="0.0, 1000.0")
	float SourceRadius = 0.0f; // 광원 반경

	// NOTE: 실제로는 사용하지 않지만 프로퍼티 양식 때문에 어쩔 수 없이 임시로 선언
	UPROPERTY(EditAnywhere, Category="ShadowMap")
	ID3D11ShaderResourceView* ShadowMapSRV = nullptr;

	UPROPERTY(EditAnywhere, Category="ShadowMap")
	bool bOverrideCameraLightPerspective = false;

	UPROPERTY(EditAnywhere, Category="ShadowMap", Range="0, 5")
	uint32 OverrideCameraLightNum = 0;
};
