#pragma once

/**
 * @file SPanel.h
 * @brief 자식 위젯들을 담는 컨테이너 위젯
 *
 * SPanel은 여러 자식 위젯을 관리하는 기본 컨테이너입니다.
 * SCanvas 등의 구체적인 레이아웃 패널의 기반 클래스입니다.
 */

#include "SWidget.h"
#include "Slot.h"

/**
 * @class SPanel
 * @brief 자식 위젯 컨테이너
 */
class SPanel : public SWidget
{
public:
    // =====================================================
    // 생성자
    // =====================================================

    SPanel() = default;
    virtual ~SPanel() override = default;

    // =====================================================
    // 렌더링
    // =====================================================

    void Paint(FD2DRenderer& Renderer, const FGeometry& Geometry) override;

    // =====================================================
    // 입력 처리 (자식에게 전파)
    // =====================================================

    FReply OnMouseButtonDown(const FGeometry& Geometry, const FVector2D& MousePosition) override;
    FReply OnMouseButtonUp(const FGeometry& Geometry, const FVector2D& MousePosition) override;
    FReply OnMouseMove(const FGeometry& Geometry, const FVector2D& MousePosition) override;

    // =====================================================
    // 자식 관리
    // =====================================================

    /** 자식 추가 (Slot 반환으로 체이닝 가능) */
    FSlot& AddChild(const TSharedPtr<SWidget>& Child);

    /** 자식 제거 */
    bool RemoveChild(const TSharedPtr<SWidget>& Child);

    /** 모든 자식 제거 */
    void ClearChildren();

    /** 자식 수 */
    int32 GetChildCount() const { return ChildSlots.Num(); }

    /** 슬롯 접근 */
    const TArray<FSlot>& GetSlots() const { return ChildSlots; }
    TArray<FSlot>& GetSlots() { return ChildSlots; }

    // =====================================================
    // 히트 테스트 (자식 포함)
    // =====================================================

    /**
     * 주어진 위치에 있는 위젯 찾기
     * @param Geometry 이 패널의 기하학
     * @param AbsolutePosition 절대 좌표
     * @return 해당 위치의 위젯 (없으면 nullptr)
     */
    virtual TSharedPtr<SWidget> FindWidgetAt(const FGeometry& Geometry, const FVector2D& AbsolutePosition);

protected:
    // =====================================================
    // 자식 렌더링 헬퍼
    // =====================================================

    /** 모든 자식 렌더링 */
    void PaintChildren(FD2DRenderer& Renderer, const FGeometry& Geometry);

    /** 호버 상태 업데이트 */
    void UpdateHoveredWidget(const FGeometry& Geometry, const FVector2D& MousePosition);

    // =====================================================
    // 데이터
    // =====================================================

    TArray<FSlot> ChildSlots;
    TWeakPtr<SWidget> HoveredChild;
};
