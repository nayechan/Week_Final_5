#pragma once
#include "PrimitiveComponent.h"
#include "Picking.h"
class UShapeComponent : public UPrimitiveComponent
{
public:
    DECLARE_CLASS(UShapeComponent, UPrimitiveComponent)

    UShapeComponent() = default;
    virtual ~UShapeComponent() = default;

    // 충돌 검사 시 재정의
    virtual bool IntersectsRay(const FRay& Ray) const { return false; }

    // Shape 크기(예: 박스=XYZ, 스피어=반지름) 정의
    virtual FVector GetExtent() const { return FVector(1, 1, 1); }

    // ───── 복사 관련 ────────────────────────────
    void DuplicateSubObjects() override;
    DECLARE_DUPLICATE(UShapeComponent)
};

