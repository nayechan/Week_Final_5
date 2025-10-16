#pragma once
#include "StaticMeshComponent.h"
class UGizmoArrowComponent : public UStaticMeshComponent
{
public:
    DECLARE_CLASS(UGizmoArrowComponent, UStaticMeshComponent)
    UGizmoArrowComponent();
    
    void Render(URenderer* Renderer, const FMatrix& View, const FMatrix& Proj) override;

protected:
    ~UGizmoArrowComponent() override;
    
public:
    const FVector& GetDirection() const { return Direction; }
    const FVector& GetColor() const { return Color; }

    void SetDirection(const FVector& InDirection) { Direction = InDirection; }
    void SetColor(const FVector& InColor) { Color = InColor; }

    // Gizmo visual state
    void SetAxisIndex(uint32 InAxisIndex) { AxisIndex = InAxisIndex; }
    void SetDefaultScale(const FVector& InSize) { DefaultScale = InSize; }
    void SetHighlighted(bool bInHighlighted, uint32 InAxisIndex) { bHighlighted = bInHighlighted; AxisIndex = InAxisIndex; }
    bool IsHighlighted() const { return bHighlighted; }
    uint32 GetAxisIndex() const { return AxisIndex; }

    // ───── 복사 관련 ────────────────────────────
    void DuplicateSubObjects() override;
    DECLARE_DUPLICATE(UGizmoArrowComponent)
protected:
    // Compute uniform scale so gizmo appears with a constant on-screen size per viewport
    float ComputeScreenConstantScale(URenderer* Renderer, const FMatrix& View, const FMatrix& Proj, float targetPixels = 30.0f) const;

protected:
    FVector Direction;
    FVector DefaultScale{ 1.f,1.f,1.f };
    FVector Color;
    bool bHighlighted = false;
    uint32 AxisIndex = 0;
};

