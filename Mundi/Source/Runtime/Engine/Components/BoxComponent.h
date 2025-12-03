// ────────────────────────────────────────────────────────────────────────────
// BoxComponent.h
// Box 형태의 충돌 컴포넌트 (Week09 기반, Week12 적응)
// ────────────────────────────────────────────────────────────────────────────
#pragma once
#include "ShapeComponent.h"
#include "AABB.h"
#include "BoxSphereBounds.h"
#include "UBoxComponent.generated.h"

class UShader;
class USphereComponent;

/**
 * UBoxComponent
 *
 * Box(직육면체) 형태의 충돌 컴포넌트입니다.
 * AABB(Axis-Aligned Bounding Box)를 사용하여 충돌을 감지합니다.
 */
UCLASS(DisplayName="박스 컴포넌트", Description="박스 모양 충돌 컴포넌트입니다")
class UBoxComponent : public UShapeComponent
{
public:
	GENERATED_REFLECTION_BODY()

	UBoxComponent();

protected:
	~UBoxComponent() override;

public:
	// ────────────────────────────────────────────────
	// 복사 관련
	// ────────────────────────────────────────────────
	void DuplicateSubObjects() override;

public:
	// ────────────────────────────────────────────────
	// Box 속성
	// ────────────────────────────────────────────────

	/** Box의 반 크기 (로컬 스페이스, 중심에서 각 축으로의 거리) */
	UPROPERTY(EditAnywhere, Category="BoxExtent")
	FVector BoxExtent = FVector(0.50f, 0.50f, 0.50f);

	void OnPropertyChanged(const FProperty& Prop) override;
	/**
	 * Box 크기를 설정합니다 (로컬 스페이스).
	 */
	void SetBoxExtent(const FVector& InExtent, bool bUpdateBounds = true);

	/**
	 * Box 크기를 반환합니다 (로컬 스페이스).
	 */
	FVector GetBoxExtent() const { return BoxExtent; }

	/**
	 * 스케일이 적용된 Box 크기를 반환합니다 (월드 스페이스).
	 */
	FVector GetScaledBoxExtent() const;

	/**
	 * 월드 스페이스의 Box 중심을 반환합니다.
	 */
	FVector GetBoxCenter() const;

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
	 * Renderer를 통해 Box를 디버그 렌더링합니다.
	 */
	void RenderDebugVolume(class URenderer* Renderer) const override;

	// ────────────────────────────────────────────────
	// Box 전용 충돌 감지 함수
	// ────────────────────────────────────────────────

	/**
	 * 다른 Box 컴포넌트와 겹쳐있는지 확인합니다.
	 */
	bool IsOverlappingBox(const UBoxComponent* OtherBox) const;

	/**
	 * Sphere 컴포넌트와 겹쳐있는지 확인합니다.
	 */
	bool IsOverlappingSphere(const USphereComponent* OtherSphere) const;

	/**
	 * 특정 점이 Box 내부에 있는지 확인합니다.
	 */
	bool ContainsPoint(const FVector& Point) const;

	UBodySetup* GetBodySetup() override;

private:
	/** 현재 Bounds (캐시됨) */
	FBoxSphereBounds CachedBounds;
};
