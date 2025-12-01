#pragma once
#include "SphereElem.h"
#include "BoxElem.h"
#include "ConvexElem.h"
#include "SphylElem.h"
#include "FKAggregateGeom.generated.h"

/** 충돌 Shape들의 컨테이너 */
USTRUCT(DisplayName="Aggregate Geometry", Description="충돌 Shape 컨테이너")
struct FKAggregateGeom
{
    GENERATED_REFLECTION_BODY()

public:
    // Shape 배열
    UPROPERTY(EditAnywhere, Category="Shapes")
    TArray<FKSphereElem> SphereElems;

    UPROPERTY(EditAnywhere, Category="Shapes")
    TArray<FKBoxElem> BoxElems;

    UPROPERTY(EditAnywhere, Category="Shapes")
    TArray<FKSphylElem> SphylElems;
    
    UPROPERTY(EditAnywhere, Category="Shapes")
    TArray<FKConvexElem> ConvexElems;

    // 생성자
    FKAggregateGeom() = default;

    FKAggregateGeom(const FKAggregateGeom& Other)
        : SphereElems(Other.SphereElems), BoxElems(Other.BoxElems)
        , SphylElems(Other.SphylElems), ConvexElems(Other.ConvexElems)
    {
    }

    FKAggregateGeom& operator=(const FKAggregateGeom& Other)
    {
        if (this != &Other)
        {
            SphereElems = Other.SphereElems;
            BoxElems = Other.BoxElems;
            SphylElems = Other.SphylElems;
            ConvexElems = Other.ConvexElems;
        }
        return *this;
    }

    // 전체 Element 개수
    int32 GetElementCount() const
    {
        return SphereElems.Num() + BoxElems.Num() + SphylElems.Num() + ConvexElems.Num();
    }

    // 타입별 Element 개수
    int32 GetElementCount(EAggCollisionShape Type) const
    {
        switch (Type)
        {
        case EAggCollisionShape::Sphere: return SphereElems.Num();
        case EAggCollisionShape::Box:    return BoxElems.Num();
        case EAggCollisionShape::Sphyl:  return SphylElems.Num();
        case EAggCollisionShape::Convex:  return ConvexElems.Num();
        default: return 0;
        }
    }

    // 타입과 인덱스로 Element 접근
    FKShapeElem* GetElement(EAggCollisionShape Type, int32 Index)
    {
        switch (Type)
        {
        case EAggCollisionShape::Sphere:
            if (Index >= 0 && Index < SphereElems.Num())
                return &SphereElems[Index];
            break;
        case EAggCollisionShape::Box:
            if (Index >= 0 && Index < BoxElems.Num())
                return &BoxElems[Index];
            break;
        case EAggCollisionShape::Sphyl:
            if (Index >= 0 && Index < SphylElems.Num())
                return &SphylElems[Index];
            break;
        case EAggCollisionShape::Convex:
            if (Index >= 0 && Index < ConvexElems.Num())
                return &ConvexElems[Index];
            break;
        default:
            break;
        }
        return nullptr;
    }

    const FKShapeElem* GetElement(EAggCollisionShape Type, int32 Index) const
    {
        return const_cast<FKAggregateGeom*>(this)->GetElement(Type, Index);
    }

    // 통합 인덱스로 Element 접근 (Sphere -> Box -> Sphyl 순서)
    FKShapeElem* GetElement(int32 InIndex)
    {
        int32 SphereCount = SphereElems.Num();
        int32 BoxCount = BoxElems.Num();
        int32 SphylCount = SphylElems.Num();
        int32 ConvexCount = ConvexElems.Num();

        if (InIndex < SphereCount)
        {
            return &SphereElems[InIndex];
        }
        InIndex -= SphereCount;

        if (InIndex < BoxCount)
        {
            return &BoxElems[InIndex];
        }
        InIndex -= BoxCount;

        if (InIndex < SphylCount)
        {
            return &SphylElems[InIndex];
        }
        InIndex -= SphylCount;
        
        if (InIndex < ConvexCount)
        {
            return &ConvexElems[InIndex];
        }

        return nullptr;
    }

    const FKShapeElem* GetElement(int32 InIndex) const
    {
        return const_cast<FKAggregateGeom*>(this)->GetElement(InIndex);
    }

    // 이름으로 Element 검색
    const FKShapeElem* GetElementByName(const FName& InName) const
    {
        for (int32 i = 0; i < SphereElems.Num(); ++i)
        {
            if (SphereElems[i].GetName() == InName)
                return &SphereElems[i];
        }
        for (int32 i = 0; i < BoxElems.Num(); ++i)
        {
            if (BoxElems[i].GetName() == InName)
                return &BoxElems[i];
        }
        for (int32 i = 0; i < SphylElems.Num(); ++i)
        {
            if (SphylElems[i].GetName() == InName)
                return &SphylElems[i];
        }
        for (int32 i = 0; i < ConvexElems.Num(); ++i)
        {
            if (ConvexElems[i].GetName() == InName)
                return &ConvexElems[i];
        }
        return nullptr;
    }

    // 이름으로 Element 인덱스 검색
    int32 GetElementIndexByName(const FName& InName) const
    {
        int32 Index = 0;

        for (int32 i = 0; i < SphereElems.Num(); ++i)
        {
            if (SphereElems[i].GetName() == InName)
                return Index;
            ++Index;
        }
        for (int32 i = 0; i < BoxElems.Num(); ++i)
        {
            if (BoxElems[i].GetName() == InName)
                return Index;
            ++Index;
        }
        for (int32 i = 0; i < SphylElems.Num(); ++i)
        {
            if (SphylElems[i].GetName() == InName)
                return Index;
            ++Index;
        }
        for (int32 i = 0; i < ConvexElems.Num(); ++i)
        {
            if (ConvexElems[i].GetName() == InName)
                return Index;
            ++Index;
        }

        return -1; // Not found
    }

    // 모든 Element 삭제
    void EmptyElements()
    {
        SphereElems.Empty();
        BoxElems.Empty();
        SphylElems.Empty();
        ConvexElems.Empty();
    }

    // Shape 추가 함수들
    int32 AddSphereElem(const FKSphereElem& Elem)
    {
        return SphereElems.Add(Elem);
    }

    int32 AddBoxElem(const FKBoxElem& Elem)
    {
        return BoxElems.Add(Elem);
    }

    int32 AddSphylElem(const FKSphylElem& Elem)
    {
        return SphylElems.Add(Elem);
    }

    int32 AddConvexElem(const FKConvexElem& Elem)
    {
        return ConvexElems.Add(Elem);
    }

    // Shape 제거 함수들
    void RemoveSphereElem(int32 Index)
    {
        if (Index >= 0 && Index < SphereElems.Num())
        {
            SphereElems.RemoveAt(Index);
        }
    }

    void RemoveBoxElem(int32 Index)
    {
        if (Index >= 0 && Index < BoxElems.Num())
        {
            BoxElems.RemoveAt(Index);
        }
    }

    void RemoveSphylElem(int32 Index)
    {
        if (Index >= 0 && Index < SphylElems.Num())
        {
            SphylElems.RemoveAt(Index);
        }
    }

    void RemoveConvexElem(int32 Index)
    {
        if (Index >= 0 && Index < ConvexElems.Num())
        {
            ConvexElems.RemoveAt(Index);
        }
    }

    // 전체 볼륨 계산
    float GetScaledVolume(const FVector& Scale3D) const
    {
        float Volume = 0.0f;

        for (int32 i = 0; i < SphereElems.Num(); ++i)
        {
            if (SphereElems[i].GetContributeToMass())
                Volume += SphereElems[i].GetScaledVolume(Scale3D);
        }
        for (int32 i = 0; i < BoxElems.Num(); ++i)
        {
            if (BoxElems[i].GetContributeToMass())
                Volume += BoxElems[i].GetScaledVolume(Scale3D);
        }
        for (int32 i = 0; i < SphylElems.Num(); ++i)
        {
            if (SphylElems[i].GetContributeToMass())
                Volume += SphylElems[i].GetScaledVolume(Scale3D);
        }
        for (int32 i = 0; i < ConvexElems.Num(); ++i)
        {
            if (ConvexElems[i].GetContributeToMass())
                Volume += ConvexElems[i].GetScaledVolume(Scale3D);
        }

        return Volume;
    }

    // 비어있는지 확인
    bool IsEmpty() const
    {
        return SphereElems.IsEmpty() && BoxElems.IsEmpty() && SphylElems.IsEmpty() && ConvexElems.IsEmpty();
    }

    // 유효한 Shape가 있는지 확인
    bool HasValidShapes() const
    {
        return GetElementCount() > 0;
    }

    // 디버그 렌더링
    void DrawAggGeom(FPrimitiveDrawInterface* PDI, const FTransform& Transform,
                     const FLinearColor& Color, bool bDrawSolid) const
    {
        float Scale = 1.0f;

        for (int32 i = 0; i < SphereElems.Num(); ++i)
        {
            FTransform ElemTM = Transform * SphereElems[i].GetTransform();
            if (bDrawSolid)
                SphereElems[i].DrawElemSolid(PDI, ElemTM, Scale, Color);
            else
                SphereElems[i].DrawElemWire(PDI, ElemTM, Scale, Color);
        }

        for (int32 i = 0; i < BoxElems.Num(); ++i)
        {
            FTransform ElemTM = Transform * BoxElems[i].GetTransform();
            if (bDrawSolid)
                BoxElems[i].DrawElemSolid(PDI, ElemTM, Scale, Color);
            else
                BoxElems[i].DrawElemWire(PDI, ElemTM, Scale, Color);
        }

        for (int32 i = 0; i < SphylElems.Num(); ++i)
        {
            FTransform ElemTM = Transform * SphylElems[i].GetTransform();
            if (bDrawSolid)
                SphylElems[i].DrawElemSolid(PDI, ElemTM, Scale, Color);
            else
                SphylElems[i].DrawElemWire(PDI, ElemTM, Scale, Color);
        }

        for (int32 i = 0; i < SphylElems.Num(); ++i)
        {
            FTransform ElemTM = Transform * ConvexElems[i].GetTransform();
            if (bDrawSolid)
                ConvexElems[i].DrawElemSolid(PDI, ElemTM, Scale, Color);
            else
                ConvexElems[i].DrawElemWire(PDI, ElemTM, Scale, Color);
        }
    }
};
