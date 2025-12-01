#pragma once
#include "ShapeElem.h"
#include "FKConvexElem.generated.h"

// PhysX 전방 선언
namespace physx { class PxConvexMesh; }

/** 충돌용 Convex Hull Shape */
USTRUCT(DisplayName="Convex Element", Description="충돌용 볼록 껍질(Convex Hull) Shape")
struct FKConvexElem : public FKShapeElem
{
    GENERATED_REFLECTION_BODY()

public:
    // 정적 Shape 타입
    static constexpr EAggCollisionShape StaticShapeType = EAggCollisionShape::Convex;

    // --- 데이터 멤버 ---
    // Convex Hull을 구성하는 버텍스 데이터 (로컬 공간)
    UPROPERTY(EditAnywhere, Category="Shape")
    TArray<FVector> VertexData;

    // 디버그 렌더링용 인덱스 데이터 (선택 사항이지만 디버깅에 유용)
    UPROPERTY(EditAnywhere, Category="Shape")
    TArray<int32> IndexData;

    // 이 요소의 로컬 바운딩 박스 (빠른 기각용)
    UPROPERTY(EditAnywhere, Category="Shape")
    FAABB ElemBox;

    // 부모 Body에 대한 로컬 변환 (위치, 회전, 스케일)
    UPROPERTY(EditAnywhere, Category="Shape")
    FTransform Transform;

    // --- 런타임 데이터 ---
    physx::PxConvexMesh* ConvexMesh = nullptr;

    // 쿠킹된 바이너리 데이터
    TArray<uint8> CookedData;

public:
    // --- 생성자 ---
    FKConvexElem() : FKShapeElem(EAggCollisionShape::Convex) {}

    // 복사 생성자
    FKConvexElem(const FKConvexElem& Other) : FKShapeElem(Other)
        , VertexData(Other.VertexData), IndexData(Other.IndexData)
        , ElemBox(Other.ElemBox), Transform(Other.Transform), ConvexMesh(nullptr) {}

    ~FKConvexElem() override = default;

    // 대입 연산자
    FKConvexElem& operator=(const FKConvexElem& Other)
    {
        if (this != &Other)
        {
            FKShapeElem::operator=(Other);
            VertexData = Other.VertexData;
            IndexData = Other.IndexData;
            ElemBox = Other.ElemBox;
            Transform = Other.Transform;
            ConvexMesh = nullptr; // 리셋
        }
        return *this;
    }

    // 비교 연산자
    friend bool operator==(const FKConvexElem& LHS, const FKConvexElem& RHS)
    {
        // ConvexMesh 포인터는 비교하지 않음 (데이터가 중요)
        return LHS.Transform == RHS.Transform && 
               LHS.VertexData == RHS.VertexData &&
               LHS.IndexData == RHS.IndexData;
    }

    friend bool operator!=(const FKConvexElem& LHS, const FKConvexElem& RHS)
    {
        return !(LHS == RHS);
    }

    // --- FKShapeElem 가상 함수 구현 ---
    virtual FTransform GetTransform() const override
    {
        return Transform;
    }

    // Transform 설정 및 바운딩 박스 갱신 가능성 있음
    void SetTransform(const FTransform& InTransform)
    {
        Transform = InTransform;
    }

    // --- 데이터 관리 유틸리티 ---
    // 버텍스 데이터를 설정하고 바운딩 박스를 갱신
    void SetConvexData(const TArray<FVector>& InVertices, const TArray<int32>& InIndices)
    {
        VertexData = InVertices;
        IndexData = InIndices;
        UpdateElemBox();
    }

    // 바운딩 박스 재계산
    void UpdateElemBox()
    {
        ElemBox.Min = FVector(FLT_MAX, FLT_MAX, FLT_MAX);
        ElemBox.Max = FVector(-FLT_MAX, -FLT_MAX, -FLT_MAX);

        // 2. 버텍스 순회하며 확장
        if (VertexData.Num() > 0)
        {
            for (const FVector& V : VertexData)
            {
                ElemBox.Min = ElemBox.Min.ComponentMin(V);
                ElemBox.Max = ElemBox.Max.ComponentMax(V);
            }
        }
        else
        {
            // 데이터가 없으면 0,0,0
            ElemBox.Min = FVector::Zero();
            ElemBox.Max = FVector::Zero();
        }
    }

    // 런타임 메시 리셋 (재쿠킹 필요 시 호출)
    void ResetPxConvexMesh()
    {
        ConvexMesh = nullptr;
    }

    // --- 기하학적 유틸리티 ---
    float GetScaledVolume(const FVector& Scale3D) const
    {
        FVector TotalScale = Transform.Scale3D * Scale3D;
        FVector AbsScale(FMath::Abs(TotalScale.X), FMath::Abs(TotalScale.Y), FMath::Abs(TotalScale.Z));
        FVector ScaledHalfExtent = ElemBox.GetHalfExtent() * AbsScale;
    
        FVector Size = ScaledHalfExtent * 2.0f;
        return Size.X * Size.Y * Size.Z;
    }

    // 최종 스케일이 적용된 버전 반환
    FKConvexElem GetFinalScaled(const FVector& Scale3D, const FTransform& RelativeTM) const
    {
        FKConvexElem Result(*this); // 복사 생성
        Result.Transform.Scale3D = Result.Transform.Scale3D * Scale3D;
        Result.Transform = Result.Transform * RelativeTM;
        return Result;
    }

    // --- 공간 쿼리 (단순화된 버전) ---
    // 점과의 최단 거리 계산 (정확한 계산은 GJK 알고리즘 등이 필요하므로 BBox로 근사)
    float GetShortestDistanceToPoint(const FVector& WorldPosition, const FTransform& BodyToWorldTM) const
    {
        FTransform ElementToWorld = Transform * BodyToWorldTM;

        // 2. 로컬 AABB의 8개 코너를 구함
        FVector Vertices[8];
        FVector Center = ElemBox.GetCenter();
        FVector Extent = ElemBox.GetHalfExtent();

        // 8개 조합 (Min, Max 조합)
        Vertices[0] = Center + FVector(Extent.X, Extent.Y, Extent.Z);
        Vertices[1] = Center + FVector(Extent.X, Extent.Y, -Extent.Z);
        Vertices[2] = Center + FVector(Extent.X, -Extent.Y, Extent.Z);
        Vertices[3] = Center + FVector(Extent.X, -Extent.Y, -Extent.Z);
        Vertices[4] = Center + FVector(-Extent.X, Extent.Y, Extent.Z);
        Vertices[5] = Center + FVector(-Extent.X, Extent.Y, -Extent.Z);
        Vertices[6] = Center + FVector(-Extent.X, -Extent.Y, Extent.Z);
        Vertices[7] = Center + FVector(-Extent.X, -Extent.Y, -Extent.Z);

        // 3. 8개 코너를 월드 공간으로 변환하면서 새로운 Min/Max 찾기 (AABB of OBB)
        FVector WorldMin(FLT_MAX, FLT_MAX, FLT_MAX);
        FVector WorldMax(-FLT_MAX, -FLT_MAX, -FLT_MAX);

        for (auto Vertex : Vertices)
        {
            // 로컬 -> 월드 변환
            FVector WorldVert = ElementToWorld.TransformPosition(Vertex);
            WorldMin = WorldMin.ComponentMin(WorldVert);
            WorldMax = WorldMax.ComponentMax(WorldVert);
        }

        // 점 P가 박스 안에 있으면 거리는 0, 밖에 있으면 가장 가까운 면까지의 거리
        float DistSquared = 0.0f;

        // X축 검사
        if (WorldPosition.X < WorldMin.X) DistSquared += FMath::Square(WorldPosition.X - WorldMin.X);
        else if (WorldPosition.X > WorldMax.X) DistSquared += FMath::Square(WorldPosition.X - WorldMax.X);

        // Y축 검사
        if (WorldPosition.Y < WorldMin.Y) DistSquared += FMath::Square(WorldPosition.Y - WorldMin.Y);
        else if (WorldPosition.Y > WorldMax.Y) DistSquared += FMath::Square(WorldPosition.Y - WorldMax.Y);

        // Z축 검사
        if (WorldPosition.Z < WorldMin.Z) DistSquared += FMath::Square(WorldPosition.Z - WorldMin.Z);
        else if (WorldPosition.Z > WorldMax.Z) DistSquared += FMath::Square(WorldPosition.Z - WorldMax.Z);

        return FMath::Sqrt(DistSquared);
    }

    // --- 디버그 렌더링 ---
    virtual void DrawElemWire(FPrimitiveDrawInterface* PDI, const FTransform& ElemTM,
                              float Scale, const FLinearColor& Color) const override
    {
        // TODO: IndexData를 이용하여 와이어프레임 렌더링 구현
        // Transform = this->Transform * ElemTM
    }

    virtual void DrawElemSolid(FPrimitiveDrawInterface* PDI, const FTransform& ElemTM,
                               float Scale, const FLinearColor& Color) const override
    {
        // TODO: VertexData와 IndexData를 이용한 트라이앵글 렌더링 구현
    }
};
