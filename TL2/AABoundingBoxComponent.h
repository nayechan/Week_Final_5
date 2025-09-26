#pragma once
#include "ShapeComponent.h"

#pragma once
#include "ShapeComponent.h"

struct FBound
{
	FVector Min;
	FVector Max;

	FBound() : Min(FVector()), Max(FVector()) {}
	FBound(const FVector& InMin, const FVector& InMax) : Min(InMin), Max(InMax) {}

	// 중심점
	FVector GetCenter() const
	{
		return (Min + Max) * 0.5f;
	}

	// 반쪽 크기 (Extent)
	FVector GetExtent() const
	{
		return (Max - Min) * 0.5f;
	}

	// 다른 박스를 완전히 포함하는지 확인
	bool Contains(const FBound& Other) const
	{
		return (Min.X <= Other.Min.X && Max.X >= Other.Max.X) &&
			(Min.Y <= Other.Min.Y && Max.Y >= Other.Max.Y) &&
			(Min.Z <= Other.Min.Z && Max.Z >= Other.Max.Z);
	}

	// 교차 여부만 확인
	bool Intersects(const FBound& Other) const
	{
		return (Min.X <= Other.Max.X && Max.X >= Other.Min.X) &&
			(Min.Y <= Other.Max.Y && Max.Y >= Other.Min.Y) &&
			(Min.Z <= Other.Max.Z && Max.Z >= Other.Min.Z);
	}

	// i번째 옥탄트 Bounds 반환
	FBound CreateOctant(int i) const
	{
		FVector Center = GetCenter();
		FVector Extent = GetExtent() * 0.5f; // 자식은 부모의 절반 크기

		FVector NewMin, NewMax;

		// X축 (i의 1비트)
		// 0 왼쪽 1 오른쪽 
		if (i & 1)
		{
			NewMin.X = Center.X;
			NewMax.X = Max.X;
		}
		else
		{
			NewMin.X = Min.X;
			NewMax.X = Center.X;
		}

		// Y축 (i의 2비트)
		// 0 앞 2 뒤 
		if (i & 2)
		{
			NewMin.Y = Center.Y;
			NewMax.Y = Max.Y;
		}
		else
		{
			NewMin.Y = Min.Y;
			NewMax.Y = Center.Y;
		}

		// Z축 (i의 4비트)
		// 0 아래 4 위 
		if (i & 4)
		{
			NewMin.Z = Center.Z;
			NewMax.Z = Max.Z;
		}
		else
		{
			NewMin.Z = Min.Z;
			NewMax.Z = Center.Z;
		}

		return FBound(NewMin, NewMax);
	}
};

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
    FBound GetWorldBound() const;

    // 월드 좌표계에서의 AABB 반환
    FBound GetWorldBoundFromCube() ;
    //FBound GetWorldBoundFromSphere() const;

	TArray<FVector4> GetLocalCorners() const;

	void SetPrimitiveType(EPrimitiveType InType) { PrimitiveType = InType; }

private:
	void CreateLineData(
		const FVector& Min, const FVector& Max,
		OUT TArray<FVector>& Start,
		OUT TArray<FVector>& End,
		OUT TArray<FVector4>& Color);

    // Arvo 익스텐트 계산 헬퍼
    FVector ComputeWorldExtentsArvo(const FVector& LocalExtents, const FMatrix& World) const;

    FVector LocalMin;
    FVector LocalMax;
	FBound Bound;
    EPrimitiveType PrimitiveType = EPrimitiveType::Default;
};

