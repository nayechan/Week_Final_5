#include "pch.h"
#include "SWidget.h"

// =====================================================
// 렌더링
// =====================================================

void SWidget::Paint(FD2DRenderer& Renderer, const FGeometry& Geometry)
{
    // 기본 구현: 아무것도 그리지 않음
    // 서브클래스에서 오버라이드
}

// =====================================================
// 입력 처리
// =====================================================

FReply SWidget::OnMouseButtonDown(const FGeometry& Geometry, const FVector2D& MousePosition)
{
    // 기본 구현: 처리하지 않음
    return FReply::Unhandled();
}

FReply SWidget::OnMouseButtonUp(const FGeometry& Geometry, const FVector2D& MousePosition)
{
    // 기본 구현: 처리하지 않음
    return FReply::Unhandled();
}

FReply SWidget::OnMouseMove(const FGeometry& Geometry, const FVector2D& MousePosition)
{
    // 기본 구현: 처리하지 않음
    return FReply::Unhandled();
}

void SWidget::OnMouseEnter()
{
    bIsHovered = true;
}

void SWidget::OnMouseLeave()
{
    bIsHovered = false;
    bIsPressed = false;  // 마우스가 나가면 눌림 상태도 해제
}

// =====================================================
// 히트 테스트
// =====================================================

bool SWidget::HitTest(const FGeometry& Geometry, const FVector2D& AbsolutePosition) const
{
    if (!IsHitTestVisible())
        return false;

    return Geometry.IsPointInside(AbsolutePosition);
}
