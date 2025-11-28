#pragma once

/**
 * @file SCanvas.h
 * @brief ZOrder를 지원하는 절대 좌표 컨테이너
 *
 * SCanvas는 자식 위젯을 절대 좌표(앵커 기반)로 배치하며,
 * ZOrder를 통해 렌더링 순서를 제어합니다.
 *
 * SPanel과 달리 FCanvasSlot을 사용하여 앵커, 피벗, 오프셋 등
 * 풍부한 레이아웃 옵션을 제공합니다.
 */

#include "SWidget.h"
#include "Slot.h"

/**
 * @class SCanvas
 * @brief 절대 좌표 + ZOrder 컨테이너 (SWidget 직접 상속)
 */
class SCanvas : public SWidget
{
public:
    // =====================================================
    // 생성자
    // =====================================================

    SCanvas() = default;
    virtual ~SCanvas() override = default;

    // =====================================================
    // 렌더링 (ZOrder 순서)
    // =====================================================

    void Paint(FD2DRenderer& Renderer, const FGeometry& Geometry) override;

    // =====================================================
    // 자식 관리 (Canvas 전용)
    // =====================================================

    /**
     * 자식 추가 (Canvas Slot 반환)
     * @param Child 추가할 위젯
     * @return 체이닝용 CanvasSlot 참조
     */
    FCanvasSlot& AddChildToCanvas(const TSharedPtr<SWidget>& Child);

    /** Canvas 슬롯 접근 */
    const TArray<FCanvasSlot>& GetCanvasSlots() const { return CanvasSlots; }
    TArray<FCanvasSlot>& GetCanvasSlots() { return CanvasSlots; }

    // =====================================================
    // 입력 처리 (ZOrder 역순)
    // =====================================================

    FReply OnMouseButtonDown(const FGeometry& Geometry, const FVector2D& MousePosition) override;
    FReply OnMouseButtonUp(const FGeometry& Geometry, const FVector2D& MousePosition) override;
    FReply OnMouseMove(const FGeometry& Geometry, const FVector2D& MousePosition) override;

    // =====================================================
    // 히트 테스트 (ZOrder 역순)
    // =====================================================

    /**
     * 주어진 위치에 있는 위젯 찾기
     * @param Geometry 이 캔버스의 기하학
     * @param AbsolutePosition 절대 좌표
     * @return 해당 위치의 위젯 (없으면 nullptr)
     */
    TSharedPtr<SWidget> FindWidgetAt(const FGeometry& Geometry, const FVector2D& AbsolutePosition) const;

protected:
    // =====================================================
    // 내부 헬퍼
    // =====================================================

    /** ZOrder 기준 정렬 (mutable 멤버 수정이므로 const 가능) */
    void SortByZOrder() const;

    /** 호버 상태 업데이트 (CanvasSlots용) */
    void UpdateCanvasHoveredWidget(const FGeometry& Geometry, const FVector2D& MousePosition);

    // =====================================================
    // 데이터
    // =====================================================

    /** 자식 슬롯 배열 (정렬은 논리적 상태 변경이 아니므로 mutable) */
    mutable TArray<FCanvasSlot> CanvasSlots;

    /** 정렬 필요 플래그 (내부 최적화용 mutable) */
    mutable bool bNeedSort = false;

    /** 현재 호버된 자식 위젯 */
    TWeakPtr<SWidget> HoveredChild;
};
