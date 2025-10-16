#pragma once
#include "ShapeComponent.h"
#include "AABB.h"

class ULine;
class UAABoundingBoxComponent :public UShapeComponent
{
	DECLARE_CLASS(UAABoundingBoxComponent, UShapeComponent)
public:
	UAABoundingBoxComponent();

	// 주어진 로컬 버텍스들로부터 Min/Max 계산
	void SetFromVertices(const TArray<FVector>& Verts);
	void SetFromVertices(const TArray<FNormalVertex>& Verts);

	void Render(URenderer* Renderer, const FMatrix& View, const FMatrix& Proj) override;

	// Arvo 기반 월드 AABB
	// 코너 8개 변환 방식에 비해 8배 이상 빠릅니다.
	FAABB GetWorldBound() const;

	// 월드 좌표계에서의 AABB 반환
	FAABB GetWorldBoundFromCube();
	//FBound GetWorldBoundFromSphere() const;

	TArray<FVector4> GetLocalCorners() const;

	void SetPrimitiveType(EPrimitiveType InType) { PrimitiveType = InType; }

	// ───── 복사 관련 ────────────────────────────
	void DuplicateSubObjects() override;
	DECLARE_DUPLICATE(UAABoundingBoxComponent)

private:
	void CreateLineData(
		const FVector& Min, const FVector& Max,
		OUT TArray<FVector>& Start,
		OUT TArray<FVector>& End,
		OUT TArray<FVector4>& Color);

    // Arvo 익스텐트 계산 헬퍼
    FVector ComputeWorldExtentsArvo(const FVector& LocalExtents, const FMatrix& World) const;
    
    // 🚀 SIMD 최적화된 벡터-매트릭스 변환
    FVector TransformVectorSIMD(const FVector& vector, const FMatrix& matrix) const;
	
	FAABB LocalBound;
	FAABB WorldBound;
	EPrimitiveType PrimitiveType = EPrimitiveType::Default;
};

