#pragma once

#include "PointLightComponent.h"
#include "LightManager.h"
#include "USpotLightComponent.generated.h"

class UCameraComponent;

// 스포트라이트 (원뿔 형태로 빛 방출)
class USpotLightComponent : public UPointLightComponent
{
public:

	GENERATED_REFLECTION_BODY()

	USpotLightComponent();
	virtual ~USpotLightComponent() override;

public:
	void GetShadowRenderRequests(FSceneView* View, TArray<FShadowRenderRequest>& OutRequests) override;

	// Cone Angles
	void SetInnerConeAngle(float InAngle)
	{
		InnerConeAngle = InAngle;
		// InnerCone은 절대 OuterCone보다 클 수 없음
		if (InnerConeAngle > OuterConeAngle)
		{
			OuterConeAngle = InnerConeAngle;
		}
	}
	float GetInnerConeAngle() const { return InnerConeAngle; }

	void SetOuterConeAngle(float InAngle)
	{
		OuterConeAngle = InAngle;
		// OuterCone이 InnerCone보다 작아지면 InnerCone도 함께 줄임
		if (OuterConeAngle < InnerConeAngle)
		{
			InnerConeAngle = OuterConeAngle;
		}
	}
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
	void OnTransformUpdated() override;
	void OnRegister(UWorld* InWorld) override;
	void OnUnregister() override;

	// Cone Angle Validation
	void ValidateConeAngles();

	// Debug Rendering
	virtual void RenderDebugVolume(class URenderer* Renderer) const override;

	// Serialization & Duplication
	void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;
	virtual void DuplicateSubObjects() override;

	// Update Gizmo to match light properties
	void UpdateDirectionGizmo();

	FMatrix GetViewMatrix() const;
	FMatrix GetProjectionMatrix() const;
	FMatrix GetWarpMatrix() const { return WarpMatrix; }
	void RenderDebugFrustum(TArray<FVector>& StartPoints, TArray<FVector>& EndPoints, TArray<FVector4>& Colors) const;
	void CalculateWarpMatrix(URenderer* Renderer, UCameraComponent* Camera, FViewport* Viewport);

protected:
	UPROPERTY(EditAnywhere, Category="Light", Range="0.0, 90.0")
	float InnerConeAngle = 30.0f; // 내부 원뿔 각도

	UPROPERTY(EditAnywhere, Category="Light", Range="0.0, 90.0")
	float OuterConeAngle = 45.0f; // 외부 원뿔 각도

	// 이전 값 추적 (UI 변경 감지용)
	float PreviousInnerConeAngle = 30.0f;
	float PreviousOuterConeAngle = 45.0f;

	FMatrix WarpMatrix = FMatrix::Identity();

	// Direction Gizmo (shows light direction)
	class UGizmoArrowComponent* DirectionGizmo = nullptr;

};
