#pragma once

/**
 * @file Slot.h
 * @brief 위젯 슬롯 - 자식 위젯의 배치 정보
 *
 * FSlot은 부모 위젯이 자식 위젯을 어떻게 배치할지 정의합니다.
 * 현재는 절대 좌표(Position, Size)만 지원하며,
 * 나중에 앵커, 정렬, 패딩 등으로 확장할 수 있습니다.
 */

#include "Geometry.h"

// Forward declaration
class SWidget;

/**
 * @struct FSlot
 * @brief 기본 슬롯 구조체 - 자식 위젯의 배치 정보를 담습니다
 */
struct FSlot
{
    // =====================================================
    // 자식 위젯 참조
    // =====================================================

    TSharedPtr<SWidget> Widget;

    // =====================================================
    // 현재: 절대 좌표 시스템
    // =====================================================

    /** 부모 로컬 좌표계에서의 위치 */
    FVector2D Position = FVector2D::Zero();

    /** 위젯 크기 */
    FVector2D Size = FVector2D(100.f, 30.f);

    // =====================================================
    // 나중에: 상대 좌표 시스템 (주석 처리)
    // =====================================================

    // /** 앵커 최소점 (0,0 = 좌상단, 1,1 = 우하단) */
    // FVector2D AnchorMin = FVector2D::Zero();
    //
    // /** 앵커 최대점 */
    // FVector2D AnchorMax = FVector2D::Zero();
    //
    // /** 앵커 기준 오프셋 */
    // FVector2D Offset = FVector2D::Zero();
    //
    // /** 피벗 (회전/스케일 기준점) */
    // FVector2D Pivot = FVector2D(0.5f, 0.5f);
    //
    // /** 정렬 (부모 내에서의 정렬) */
    // FVector2D Alignment = FVector2D::Zero();
    //
    // /** 패딩 */
    // struct { float Left, Top, Right, Bottom; } Padding = {0,0,0,0};

    // =====================================================
    // 메서드
    // =====================================================

    FSlot() = default;

    explicit FSlot(const TSharedPtr<SWidget>& InWidget)
        : Widget(InWidget)
    {}

    /** 위치 설정 (Fluent API) */
    FSlot& SetPosition(const FVector2D& InPosition)
    {
        Position = InPosition;
        return *this;
    }

    FSlot& SetPosition(float X, float Y)
    {
        Position = FVector2D(X, Y);
        return *this;
    }

    /** 크기 설정 (Fluent API) */
    FSlot& SetSize(const FVector2D& InSize)
    {
        Size = InSize;
        return *this;
    }

    FSlot& SetSize(float Width, float Height)
    {
        Size = FVector2D(Width, Height);
        return *this;
    }

    /**
     * 부모 Geometry로부터 자식 Geometry 계산
     * 현재는 단순히 Position/Size 적용
     * 나중에 앵커, 정렬 등 적용하도록 확장
     */
    FGeometry ComputeGeometry(const FGeometry& ParentGeometry) const
    {
        return ParentGeometry.MakeChild(Position, Size);
    }

    /** 유효성 검사 */
    bool IsValid() const
    {
        return Widget != nullptr;
    }
};

/**
 * @struct FCanvasSlot
 * @brief 캔버스용 슬롯 - 앵커 기반 상대좌표 시스템
 *
 * 앵커 시스템 (UMG 스타일):
 * - Anchor: 부모 영역 내 기준점 (0,0 = 좌상단, 1,1 = 우하단)
 * - Offset: 앵커 기준 픽셀 오프셋
 * - bUseRelativeSize: true면 Size가 부모 대비 비율 (0.0~1.0)
 */
struct FCanvasSlot : public FSlot
{
    /** Z-Order (높을수록 위에 그려짐) */
    int32 ZOrder = 0;

    /** 앵커 위치 (0,0 = 좌상단, 1,1 = 우하단, 0.5,0.5 = 중앙) */
    FVector2D Anchor = FVector2D(0.f, 0.f);

    /** 앵커 기준 픽셀 오프셋 */
    FVector2D Offset = FVector2D(0.f, 0.f);

    /** 크기를 부모 대비 비율로 사용할지 여부 */
    bool bUseRelativeSize = false;

    /** 피벗 (위젯 내 기준점, 0,0 = 좌상단, 0.5,0.5 = 중앙) */
    FVector2D Pivot = FVector2D(0.f, 0.f);

    FCanvasSlot() = default;

    explicit FCanvasSlot(const TSharedPtr<SWidget>& InWidget)
        : FSlot(InWidget)
    {}

    // =====================================================
    // 앵커 기반 설정 (Fluent API)
    // =====================================================

    /** 앵커 설정 */
    FCanvasSlot& SetAnchor(const FVector2D& InAnchor)
    {
        Anchor = InAnchor;
        return *this;
    }

    FCanvasSlot& SetAnchor(float X, float Y)
    {
        Anchor = FVector2D(X, Y);
        return *this;
    }

    /** 오프셋 설정 (앵커 기준 픽셀) */
    FCanvasSlot& SetOffset(const FVector2D& InOffset)
    {
        Offset = InOffset;
        return *this;
    }

    FCanvasSlot& SetOffset(float X, float Y)
    {
        Offset = FVector2D(X, Y);
        return *this;
    }

    /** 피벗 설정 */
    FCanvasSlot& SetPivot(const FVector2D& InPivot)
    {
        Pivot = InPivot;
        return *this;
    }

    FCanvasSlot& SetPivot(float X, float Y)
    {
        Pivot = FVector2D(X, Y);
        return *this;
    }

    /** Z-Order 설정 */
    FCanvasSlot& SetZOrder(int32 InZOrder)
    {
        ZOrder = InZOrder;
        return *this;
    }

    // =====================================================
    // 절대 좌표 설정 (하위 호환용)
    // =====================================================

    FCanvasSlot& SetPosition(const FVector2D& InPosition)
    {
        Position = InPosition;
        return *this;
    }

    FCanvasSlot& SetPosition(float X, float Y)
    {
        Position = FVector2D(X, Y);
        return *this;
    }

    FCanvasSlot& SetSize(const FVector2D& InSize)
    {
        Size = InSize;
        bUseRelativeSize = false;
        return *this;
    }

    FCanvasSlot& SetSize(float Width, float Height)
    {
        Size = FVector2D(Width, Height);
        bUseRelativeSize = false;
        return *this;
    }

    /** 상대 크기 설정 (부모 대비 비율, 0.0~1.0) */
    FCanvasSlot& SetRelativeSize(const FVector2D& InRelativeSize)
    {
        Size = InRelativeSize;
        bUseRelativeSize = true;
        return *this;
    }

    FCanvasSlot& SetRelativeSize(float WidthRatio, float HeightRatio)
    {
        Size = FVector2D(WidthRatio, HeightRatio);
        bUseRelativeSize = true;
        return *this;
    }

    // =====================================================
    // Geometry 계산
    // =====================================================

    /**
     * 부모 Geometry로부터 자식 Geometry 계산 (앵커 기반)
     */
    FGeometry ComputeGeometry(const FGeometry& ParentGeometry) const
    {
        FVector2D ParentSize = ParentGeometry.LocalSize;

        // 실제 크기 계산
        FVector2D ActualSize = bUseRelativeSize
            ? FVector2D(ParentSize.X * Size.X, ParentSize.Y * Size.Y)
            : Size;

        // 앵커 위치 계산 (부모 내 절대 위치)
        FVector2D AnchorPos = FVector2D(
            ParentSize.X * Anchor.X,
            ParentSize.Y * Anchor.Y
        );

        // 피벗 오프셋 계산 (위젯 크기 기준)
        FVector2D PivotOffset = FVector2D(
            ActualSize.X * Pivot.X,
            ActualSize.Y * Pivot.Y
        );

        // 최종 위치 = 앵커 위치 + 오프셋 - 피벗 오프셋 + Position(하위호환)
        FVector2D FinalPosition = AnchorPos + Offset - PivotOffset + Position;

        return ParentGeometry.MakeChild(FinalPosition, ActualSize);
    }
};
