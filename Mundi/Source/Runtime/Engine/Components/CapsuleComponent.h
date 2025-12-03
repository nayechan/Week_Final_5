// ────────────────────────────────────────────────────────────────────────────
// CapsuleComponent.h
// Capsule 형태의 충돌 컴포넌트 (Week09 기반, Week12 적응)
// ────────────────────────────────────────────────────────────────────────────
#pragma once
#include "ShapeComponent.h"
#include "BoxSphereBounds.h"
#include "UCapsuleComponent.generated.h"

class UShader;
class USphereComponent;
class UBoxComponent;

/**
 * UCapsuleComponent
 *
 * Capsule(캡슐) 형태의 충돌 컴포넌트입니다.
 * 중심 선분(Line Segment) + 반지름으로 표현됩니다.
 * 캐릭터 충돌에 주로 사용됩니다.
 */
UCLASS(DisplayName="캡슐 컴포넌트", Description="캡슐 모양 충돌 컴포넌트입니다")
class UCapsuleComponent : public UShapeComponent
{
public:
	GENERATED_REFLECTION_BODY()

	UCapsuleComponent();

protected:
	~UCapsuleComponent() override;

public:
	// ────────────────────────────────────────────────
	// 복사 관련
	// ────────────────────────────────────────────────
	void DuplicateSubObjects() override;

public:
	// ────────────────────────────────────────────────
	// Capsule 속성
	// ────────────────────────────────────────────────

	/** Capsule의 반지름 (로컬 스페이스) */
	UPROPERTY(EditAnywhere, Category="CapsuleRadius")
	float CapsuleRadius = 1.0f;

	/** Capsule의 반 높이 (로컬 스페이스, 중심에서 끝까지, 반지름 제외) */
	UPROPERTY(EditAnywhere, Category="CapsuleHalfHeight")
	float CapsuleHalfHeight = 2.0f;

	void OnPropertyChanged(const FProperty& Prop) override;

	/**
	 * Capsule 크기를 설정합니다.
	 */
	void SetCapsuleSize(float InRadius, float InHalfHeight, bool bUpdateBounds = true);

	/**
	 * Capsule 반지름을 반환합니다 (로컬 스페이스).
	 */
	float GetCapsuleRadius() const { return CapsuleRadius; }

	/**
	 * Capsule 반 높이를 반환합니다 (로컬 스페이스).
	 */
	float GetCapsuleHalfHeight() const { return CapsuleHalfHeight; }

	/**
	 * 스케일이 적용된 Capsule 반지름을 반환합니다 (월드 스페이스).
	 */
	float GetScaledCapsuleRadius() const;

	/**
	 * 스케일이 적용된 Capsule 반 높이를 반환합니다 (월드 스페이스).
	 */
	float GetScaledCapsuleHalfHeight() const;

	/**
	 * 월드 스페이스의 Capsule 중심을 반환합니다.
	 */
	FVector GetCapsuleCenter() const;

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
	 * Renderer를 통해 Capsule을 디버그 렌더링합니다.
	 */
	void RenderDebugVolume(class URenderer* Renderer) const override;

	// ────────────────────────────────────────────────
	// Capsule 전용 충돌 감지 함수
	// ────────────────────────────────────────────────

	/**
	 * 다른 Capsule 컴포넌트와 겹쳐있는지 확인합니다.
	 */
	bool IsOverlappingCapsule(const UCapsuleComponent* OtherCapsule) const;

	/**
	 * Sphere 컴포넌트와 겹쳐있는지 확인합니다.
	 */
	bool IsOverlappingSphere(const USphereComponent* OtherSphere) const;

	/**
	 * Box 컴포넌트와 겹쳐있는지 확인합니다.
	 */
	bool IsOverlappingBox(const UBoxComponent* OtherBox) const;

	/**
	 * 특정 점이 Capsule 내부에 있는지 확인합니다.
	 */
	bool ContainsPoint(const FVector& Point) const;

	UBodySetup* GetBodySetup() override;

private:
	/** 현재 Bounds (캐시됨) */
	FBoxSphereBounds CachedBounds;

	/**
	 * Capsule의 선분 끝점을 계산합니다 (월드 스페이스).
	 */
	void GetCapsuleSegment(FVector& OutStart, FVector& OutEnd) const;

	/**
	 * 점과 선분 사이의 최단 거리 제곱을 계산합니다.
	 */
	static float PointToSegmentDistanceSquared(
		const FVector& Point,
		const FVector& SegmentStart,
		const FVector& SegmentEnd
	);

	/**
	 * 두 선분 사이의 최단 거리 제곱을 계산합니다.
	 */
	static float SegmentToSegmentDistanceSquared(
		const FVector& Seg1Start,
		const FVector& Seg1End,
		const FVector& Seg2Start,
		const FVector& Seg2End
	);
};
