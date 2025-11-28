#pragma once

/**
 * @file SWidget.h
 * @brief 모든 UI 위젯의 기본 클래스
 *
 * SWidget은 Slate UI 시스템의 기본 위젯 클래스입니다.
 * 모든 위젯은 이 클래스를 상속받아 구현됩니다.
 */

#include "Slot.h"
#include "FD2DRenderer.h"

/**
 * @class SWidget
 * @brief UI 위젯 기본 클래스
 */
class SWidget : public TEnableSharedFromThis<SWidget>
{
public:
    // =====================================================
    // 생성자/소멸자
    // =====================================================

    SWidget() = default;
    virtual ~SWidget() = default;

    // =====================================================
    // 렌더링 (서브클래스에서 구현)
    // =====================================================

    /**
     * 위젯 렌더링
     * @param Renderer D2D 렌더러
     * @param Geometry 이 위젯의 기하학 정보
     */
    virtual void Paint(FD2DRenderer& Renderer, const FGeometry& Geometry);

    // =====================================================
    // 입력 처리 (서브클래스에서 오버라이드)
    // =====================================================

    /** 마우스 버튼 눌림 */
    virtual FReply OnMouseButtonDown(const FGeometry& Geometry, const FVector2D& MousePosition);

    /** 마우스 버튼 떼어짐 */
    virtual FReply OnMouseButtonUp(const FGeometry& Geometry, const FVector2D& MousePosition);

    /** 마우스 이동 */
    virtual FReply OnMouseMove(const FGeometry& Geometry, const FVector2D& MousePosition);

    /** 마우스 진입 */
    virtual void OnMouseEnter();

    /** 마우스 이탈 */
    virtual void OnMouseLeave();

    // =====================================================
    // 히트 테스트
    // =====================================================

    /**
     * 점이 이 위젯 영역 안에 있는지 검사
     * @param Geometry 위젯 기하학
     * @param AbsolutePosition 절대 좌표 (화면 좌표)
     */
    virtual bool HitTest(const FGeometry& Geometry, const FVector2D& AbsolutePosition) const;

    // =====================================================
    // 크기 계산 (나중에 레이아웃 시스템용)
    // =====================================================

    /** 원하는 크기 반환 (자동 레이아웃용) */
    virtual FVector2D ComputeDesiredSize() const { return DesiredSize; }

    /** 원하는 크기 설정 */
    void SetDesiredSize(const FVector2D& InSize) { DesiredSize = InSize; }

    // =====================================================
    // 가시성
    // =====================================================

    void SetVisibility(ESlateVisibility InVisibility) { Visibility = InVisibility; }
    ESlateVisibility GetVisibility() const { return Visibility; }

    bool IsVisible() const
    {
        return Visibility == ESlateVisibility::Visible ||
               Visibility == ESlateVisibility::HitTestInvisible ||
               Visibility == ESlateVisibility::SelfHitTestInvisible;
    }

    bool IsHitTestVisible() const
    {
        return Visibility == ESlateVisibility::Visible;
    }

    // =====================================================
    // 상태 접근
    // =====================================================

    bool IsHovered() const { return bIsHovered; }
    bool IsPressed() const { return bIsPressed; }
    bool IsEnabled() const { return bIsEnabled; }

    void SetEnabled(bool bEnabled) { bIsEnabled = bEnabled; }

    // =====================================================
    // 부모/자식 관계 (SPanel에서 관리)
    // =====================================================

    void SetParent(const TWeakPtr<SWidget>& InParent) { Parent = InParent; }
    TSharedPtr<SWidget> GetParent() const { return Parent.Pin(); }

protected:
    // =====================================================
    // 상태 변수 (서브클래스에서 접근 가능)
    // =====================================================

    /** 호버 상태 */
    bool bIsHovered = false;

    /** 눌림 상태 */
    bool bIsPressed = false;

    /** 활성화 상태 */
    bool bIsEnabled = true;

    /** 가시성 */
    ESlateVisibility Visibility = ESlateVisibility::Visible;

    /** 원하는 크기 (레이아웃 힌트) */
    FVector2D DesiredSize = FVector2D(100.f, 30.f);

    /** 부모 위젯 (약한 참조) */
    TWeakPtr<SWidget> Parent;
};
