#pragma once
#include "Color.h"
#include "PrimitiveComponent.h"
#include "BoundingSphere.h"

class UShader;

/**
 * @brief 구 형태의 발광체를 표현하는 컴포넌트.
 * - 본체는 구 메시로 그려지고, 별도의 광원 렌더 패스에서 StaticMeshComponent 표면을 대상으로 색을 블렌딩한다.
 * - 추후 뎁스 큐브맵을 사용해 차폐된 픽셀을 제외할 수 있도록 관련 리소스 관리 훅을 미리 정의해 둔다.
 */
class UFireBallComponent : public UPrimitiveComponent
{
public:
	DECLARE_CLASS(UFireBallComponent, UPrimitiveComponent)
	UFireBallComponent();
	~UFireBallComponent() override;

	// ─────────────── Lifecycle ───────────────
	void InitializeComponent() override;
	void BeginPlay() override;
	void TickComponent(float DeltaTime) override;
	void EndPlay(EEndPlayReason Reason) override;

	// Registration은 ActorComponent의 base 메소드 그대로 사용

	// ─────────────── Rendering ───────────────
	void RenderAffectedPrimitives(URenderer* Renderer, UPrimitiveComponent* Target, const FMatrix& View, const FMatrix& Proj);
	void RenderDebugVolume(URenderer* Renderer, const FMatrix& View, const FMatrix& Proj) const;

	// ─────────────── Parameters ───────────────
	void SetIntensity(float InIntensity);
	float GetIntensity() const { return Intensity; }

	void SetRadius(float InRadius);
	float GetRadius() const { return Radius; }

	void SetRadiusFallOff(float InRadiusFallOff);
	float GetRadiusFallOff() const { return RadiusFallOff; }

	void SetColor(const FLinearColor& InColor);
	FLinearColor GetColor() const { return Color; }

	FBoundingSphere GetBoundingSphere() const;

	// ─────────────── Shadow Capture Stub ───────────────
	void SetShadowCaptureEnabled(bool bEnabled);
	bool IsShadowCaptureEnabled() const { return bShadowCaptureEnabled; }

	void SetShadowMapResolution(uint32 InResolution);
	uint32 GetShadowMapResolution() const { return ShadowMapResolution; }

	// ─────────────── Serialization ───────────────
	void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;

	// ───── 복사 관련 ────────────────────────────
	void DuplicateSubObjects() override;
	DECLARE_DUPLICATE(UFireBallComponent);

protected:
	void OnTransformUpdatedChildImpl() override;

private:
	struct FShadowResources;

	float Intensity = 1.0f;                    // Peak emissive strength applied during the lighting pass.
	float Radius = 10.0f;                     // Reach of the fireball effect measured from the component origin.
	float RadiusFallOff = 1.5f;                // Falloff exponent controlling intensity drop over distance.
	FLinearColor Color = FLinearColor(1.0f, 0.45f, 0.15f, 1.0f); // RGBA tint used for blended lighting.

	FString LightingShaderPath;                // Shader path for the lighting pass material.
	UShader* LightingShader = nullptr;          // Cached shader instance for the lighting pass.

	bool bShadowCaptureEnabled = false;        // Enables future depth cube-map capture for occlusion.
	uint32 ShadowMapResolution = 512;          // Planned cube-map resolution for shadow capture.

	FShadowResources* ShadowResources = nullptr; // Opaque handle to depth cube-map GPU resources.
};
