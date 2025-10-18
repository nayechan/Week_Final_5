#pragma once
#include "LightComponent.h"

// 위치 기반 로컬 라이트의 공통 베이스 (Point, Spot)
class ULocalLightComponent : public ULightComponent
{
public:
	DECLARE_CLASS(ULocalLightComponent, ULightComponent)
	GENERATED_REFLECTION_BODY()

	ULocalLightComponent();
	virtual ~ULocalLightComponent() override;

public:
	// Attenuation Properties
	void SetAttenuationRadius(float InRadius) { AttenuationRadius = InRadius; }
	float GetAttenuationRadius() const { return AttenuationRadius; }

	void SetFalloffExponent(float InExponent) { FalloffExponent = InExponent; }
	float GetFalloffExponent() const { return FalloffExponent; }

	// Attenuation 계수 설정 (상수, 일차항, 이차항)
	void SetAttenuation(const FVector& InAttenuation) { Attenuation = InAttenuation; }
	const FVector& GetAttenuation() const { return Attenuation; }

	// 감쇠 방식 선택
	void SetUseAttenuationCoefficients(bool bInUse) { bUseAttenuationCoefficients = bInUse; }
	bool IsUsingAttenuationCoefficients() const { return bUseAttenuationCoefficients; }

	// 거리 기반 감쇠 계산
	// 반환값: 0.0 (영향 없음) ~ 1.0 (최대 영향)
	// - 0.0: AttenuationRadius 밖, 라이트 영향 없음
	// - 0.0 < x < 1.0: 부분적 영향 (거리에 따라 감쇠)
	// - 1.0: 라이트 중심, 최대 영향
	virtual float GetAttenuationFactor(const FVector& WorldPosition) const;

	// Serialization & Duplication
	virtual void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;
	virtual void DuplicateSubObjects() override;
	DECLARE_DUPLICATE(ULocalLightComponent)

protected:
	float AttenuationRadius = 30.0f;				 // 감쇠 반경
	float FalloffExponent = 1.0f;					 // 감쇠 지수 (bUseAttenuationCoefficients = false일 때 사용)
	FVector Attenuation = FVector(0.0f, 0.0f, 1.0f); // 감쇠 계수: 상수, 일차항, 이차항 (bUseAttenuationCoefficients = true일 때 사용)
	bool bUseAttenuationCoefficients = true;		 // true: Attenuation 사용, false: FalloffExponent 사용 (기본값)
};
