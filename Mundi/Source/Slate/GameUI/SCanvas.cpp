#include "pch.h"
#include "SCanvas.h"
#include <algorithm>

// =====================================================
// 렌더링 (ZOrder 순서)
// =====================================================

void SCanvas::Paint(FD2DRenderer& Renderer, const FGeometry& Geometry)
{
    if (!IsVisible())
        return;

    // 정렬 필요시 ZOrder로 정렬
    if (bNeedSort)
    {
        SortByZOrder();
        bNeedSort = false;
    }

    // ZOrder 순서대로 렌더링 (낮은 값이 먼저 = 아래에)
    for (const FCanvasSlot& Slot : CanvasSlots)
    {
        if (!Slot.Widget)
            continue;

        // 자식의 Geometry 계산 (앵커 기반)
        FGeometry ChildGeometry = Slot.ComputeGeometry(Geometry);

        // 자식 렌더링
        Slot.Widget->Paint(Renderer, ChildGeometry);
    }
}

// =====================================================
// 자식 관리
// =====================================================

FCanvasSlot& SCanvas::AddChildToCanvas(const TSharedPtr<SWidget>& Child)
{
    FCanvasSlot NewSlot;
    NewSlot.Widget = Child;
    CanvasSlots.Add(NewSlot);
    bNeedSort = true;
    return CanvasSlots[CanvasSlots.Num() - 1];
}

// =====================================================
// 입력 처리 (ZOrder 역순)
// =====================================================

FReply SCanvas::OnMouseButtonDown(const FGeometry& Geometry, const FVector2D& MousePosition)
{
    if (!IsVisible() || !bIsEnabled)
        return FReply::Unhandled();

    // 정렬 필요시 ZOrder로 정렬
    if (bNeedSort)
    {
        SortByZOrder();
        bNeedSort = false;
    }

    // 역순으로 순회 (ZOrder 높은 것부터 = 위에 있는 것부터)
    for (int32 i = CanvasSlots.Num() - 1; i >= 0; --i)
    {
        const FCanvasSlot& Slot = CanvasSlots[i];
        if (!Slot.Widget)
            continue;

        FGeometry ChildGeometry = Slot.ComputeGeometry(Geometry);

        if (Slot.Widget->HitTest(ChildGeometry, MousePosition))
        {
            FReply Reply = Slot.Widget->OnMouseButtonDown(ChildGeometry, MousePosition);
            if (Reply.IsEventHandled())
                return Reply;
        }
    }

    return FReply::Unhandled();
}

FReply SCanvas::OnMouseButtonUp(const FGeometry& Geometry, const FVector2D& MousePosition)
{
    if (!IsVisible() || !bIsEnabled)
        return FReply::Unhandled();

    // 정렬 필요시 ZOrder로 정렬
    if (bNeedSort)
    {
        SortByZOrder();
        bNeedSort = false;
    }

    // 역순으로 순회 (ZOrder 높은 것부터 = 위에 있는 것부터)
    for (int32 i = CanvasSlots.Num() - 1; i >= 0; --i)
    {
        const FCanvasSlot& Slot = CanvasSlots[i];
        if (!Slot.Widget)
            continue;

        FGeometry ChildGeometry = Slot.ComputeGeometry(Geometry);

        if (Slot.Widget->HitTest(ChildGeometry, MousePosition))
        {
            FReply Reply = Slot.Widget->OnMouseButtonUp(ChildGeometry, MousePosition);
            if (Reply.IsEventHandled())
                return Reply;
        }
    }

    return FReply::Unhandled();
}

FReply SCanvas::OnMouseMove(const FGeometry& Geometry, const FVector2D& MousePosition)
{
    if (!IsVisible() || !bIsEnabled)
        return FReply::Unhandled();

    // 정렬 필요시 ZOrder로 정렬
    if (bNeedSort)
    {
        SortByZOrder();
        bNeedSort = false;
    }

    // 호버 상태 업데이트
    UpdateCanvasHoveredWidget(Geometry, MousePosition);

    // 역순으로 순회 (ZOrder 높은 것부터 = 위에 있는 것부터)
    for (int32 i = CanvasSlots.Num() - 1; i >= 0; --i)
    {
        const FCanvasSlot& Slot = CanvasSlots[i];
        if (!Slot.Widget)
            continue;

        FGeometry ChildGeometry = Slot.ComputeGeometry(Geometry);

        if (Slot.Widget->HitTest(ChildGeometry, MousePosition))
        {
            FReply Reply = Slot.Widget->OnMouseMove(ChildGeometry, MousePosition);
            if (Reply.IsEventHandled())
                return Reply;
        }
    }

    return FReply::Unhandled();
}

// =====================================================
// 히트 테스트 (ZOrder 역순)
// =====================================================

TSharedPtr<SWidget> SCanvas::FindWidgetAt(const FGeometry& Geometry, const FVector2D& AbsolutePosition) const
{
    if (!IsVisible())
        return nullptr;

    // 정렬 필요시 ZOrder로 정렬
    if (bNeedSort)
    {
        SortByZOrder();
        bNeedSort = false;
    }

    // 역순으로 순회 (ZOrder 높은 것부터 = 위에 있는 것부터)
    for (int32 i = CanvasSlots.Num() - 1; i >= 0; --i)
    {
        const FCanvasSlot& Slot = CanvasSlots[i];
        if (!Slot.Widget || !Slot.Widget->IsVisible())
            continue;

        FGeometry ChildGeometry = Slot.ComputeGeometry(Geometry);

        if (ChildGeometry.IsPointInside(AbsolutePosition))
        {
            // 자식이 SCanvas인 경우 재귀적으로 찾기
            if (SCanvas* ChildCanvas = dynamic_cast<SCanvas*>(Slot.Widget.Get()))
            {
                TSharedPtr<SWidget> Found = ChildCanvas->FindWidgetAt(ChildGeometry, AbsolutePosition);
                if (Found)
                    return Found;
            }

            return Slot.Widget;
        }
    }

    return nullptr;
}

// =====================================================
// 내부 헬퍼
// =====================================================

void SCanvas::SortByZOrder() const
{
    std::sort(CanvasSlots.begin(), CanvasSlots.end(),
        [](const FCanvasSlot& A, const FCanvasSlot& B)
        {
            return A.ZOrder < B.ZOrder;  // 낮은 ZOrder가 먼저 (아래에)
        });
}

void SCanvas::UpdateCanvasHoveredWidget(const FGeometry& Geometry, const FVector2D& MousePosition)
{
    TSharedPtr<SWidget> NewHovered = nullptr;

    // 현재 마우스 위치의 위젯 찾기 (ZOrder 역순)
    for (int32 i = CanvasSlots.Num() - 1; i >= 0; --i)
    {
        const FCanvasSlot& Slot = CanvasSlots[i];
        if (!Slot.Widget || !Slot.Widget->IsVisible())
            continue;

        FGeometry ChildGeometry = Slot.ComputeGeometry(Geometry);

        if (Slot.Widget->HitTest(ChildGeometry, MousePosition))
        {
            NewHovered = Slot.Widget;
            break;
        }
    }

    TSharedPtr<SWidget> OldHovered = HoveredChild.Pin();

    // 호버된 위젯이 변경되었는지 확인
    if (NewHovered != OldHovered)
    {
        // 이전 호버 위젯에 Leave 이벤트
        if (OldHovered)
        {
            OldHovered->OnMouseLeave();
        }

        // 새 호버 위젯에 Enter 이벤트
        if (NewHovered)
        {
            NewHovered->OnMouseEnter();
        }

        HoveredChild = NewHovered;
    }
}
