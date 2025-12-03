// ────────────────────────────────────────────────────────────────────────────
// SphereComponent.h
// Sphere 형태의 충돌 컴포넌트 (Week09 기반, Week12 적응)
// ────────────────────────────────────────────────────────────────────────────
#pragma once
#include "ShapeComponent.h"
#include "Sphere.h"
#include "BoxSphereBounds.h"
#include "USphereComponent.generated.h"

class UShader;
class UBoxComponent;

/**
 * USphereComponent
 *
 * Sphere(구) 형태의 충돌 컴포넌트입니다.
 * FSphere를 사용하여 충돌을 감지합니다.
 */
UCLASS(DisplayName="구체 컴포넌트", Description="구체 모양 충돌 컴포넌트입니다")
class USphereComponent : public UShapeComponent
{
public:
	GENERATED_REFLECTION_BODY()

	USphereComponent();

protected:
	~USphereComponent() override;

public:
	// ────────────────────────────────────────────────
	// 복사 관련
	// ────────────────────────────────────────────────
	void DuplicateSubObjects() override;

public:
	// ────────────────────────────────────────────────
	// Sphere 속성
	// ────────────────────────────────────────────────

	/** Sphere의 반지름 (로컬 스페이스) */
	UPROPERTY(EditAnywhere, Category="SphereRadius")
	float SphereRadius = 1.0f;

	void OnPropertyChanged(const FProperty& Prop) override;
	/**
	 * Sphere 반지름을 설정합니다.
	 */
	void SetSphereRadius(float InRadius, bool bUpdateBounds = true);

	/**
	 * Sphere 반지름을 반환합니다 (로컬 스페이스).
	 */
	float GetSphereRadius() const { return SphereRadius; }

	/**
	 * 스케일이 적용된 Sphere 반지름을 반환합니다 (월드 스페이스).
	 */
	float GetScaledSphereRadius() const;

	/**
	 * 월드 스페이스의 Sphere 중심을 반환합니다.
	 */
	FVector GetSphereCenter() const;

	// ────────────────────────────────────────────────
	// UShapeComponent 인터페이스 구현
	// ────────────────────────────────────────────────

	void OnRegister(UWorld* InWorld) override;

	/** Week12 호환용: Shape 반환 */
	void GetShape(FShape& Out) const override;

	/** Week12 호환용: WorldAABB 반환 */
	FAABB GetWorldAABB() const override;

	/**
	 * Bounds를 업데이트합니다.
	 */
	void UpdateBounds() override;

	/**
	 * 스케일이 적용된 Bounds를 반환합니다.
	 */
	FBoxSphereBounds GetScaledBounds() const;

	/**
	 * Renderer를 통해 Sphere를 디버그 렌더링합니다.
	 */
	void RenderDebugVolume(class URenderer* Renderer) const override;

	// ────────────────────────────────────────────────
	// Sphere 전용 충돌 감지 함수
	// ────────────────────────────────────────────────

	/**
	 * 다른 Sphere 컴포넌트와 겹쳐있는지 확인합니다.
	 */
	bool IsOverlappingSphere(const USphereComponent* OtherSphere) const;

	/**
	 * Box 컴포넌트와 겹쳐있는지 확인합니다.
	 */
	bool IsOverlappingBox(const UBoxComponent* OtherBox) const;

	/**
	 * 특정 점이 Sphere 내부에 있는지 확인합니다.
	 */
	bool ContainsPoint(const FVector& Point) const;

	UBodySetup* GetBodySetup() override;

private:
	/** 현재 Bounds (캐시됨) */
	FBoxSphereBounds CachedBounds;
};
