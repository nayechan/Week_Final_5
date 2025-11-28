#pragma once

/**
 * @file Geometry.h
 * @brief 위젯 좌표 변환 및 기하학 정보
 *
 * FGeometry는 위젯의 로컬 좌표를 절대(화면) 좌표로 변환하는 핵심 클래스입니다.
 * 나중에 상대 좌표/레이아웃 시스템으로 확장할 수 있도록 설계되었습니다.
 */

#include "SlateTypes.h"

struct FGeometry
{
    // =====================================================
    // 멤버 변수
    // =====================================================

    /** 로컬 공간에서의 크기 (이 위젯에 할당된 공간) */
    FVector2D LocalSize;

    /** 절대(화면) 좌표에서의 위치 */
    FVector2D AbsolutePosition;

    /** 스케일 팩터 (DPI 스케일링 등) */
    float Scale = 1.0f;

    // =====================================================
    // 생성자
    // =====================================================

    FGeometry()
        : LocalSize(FVector2D::Zero())
        , AbsolutePosition(FVector2D::Zero())
        , Scale(1.0f)
    {}

    FGeometry(const FVector2D& InLocalSize, const FVector2D& InAbsolutePosition, float InScale = 1.0f)
        : LocalSize(InLocalSize)
        , AbsolutePosition(InAbsolutePosition)
        , Scale(InScale)
    {}

    // =====================================================
    // 팩토리 메서드
    // =====================================================

    /** 루트 Geometry 생성 (화면 전체) */
    static FGeometry MakeRoot(const FVector2D& ScreenSize)
    {
        return FGeometry(ScreenSize, FVector2D::Zero(), 1.0f);
    }

    /** 루트 Geometry 생성 (뷰포트 영역 지정) */
    static FGeometry MakeRoot(const FVector2D& ViewportSize, const FVector2D& ViewportPosition)
    {
        return FGeometry(ViewportSize, ViewportPosition, 1.0f);
    }

    // =====================================================
    // 좌표 변환
    // =====================================================

    /** 로컬 좌표를 절대 좌표로 변환 */
    FVector2D LocalToAbsolute(const FVector2D& LocalPoint) const
    {
        return AbsolutePosition + LocalPoint * Scale;
    }

    /** 절대 좌표를 로컬 좌표로 변환 */
    FVector2D AbsoluteToLocal(const FVector2D& AbsolutePoint) const
    {
        return (AbsolutePoint - AbsolutePosition) / Scale;
    }

    // =====================================================
    // 자식 Geometry 생성
    // =====================================================

    /**
     * 자식 위젯의 Geometry 생성
     * @param ChildOffset 부모 로컬 좌표계에서 자식의 오프셋
     * @param ChildSize 자식의 크기
     * @return 자식 위젯의 Geometry
     */
    FGeometry MakeChild(const FVector2D& ChildOffset, const FVector2D& ChildSize) const
    {
        FGeometry ChildGeometry;
        ChildGeometry.LocalSize = ChildSize;
        ChildGeometry.AbsolutePosition = AbsolutePosition + ChildOffset * Scale;
        ChildGeometry.Scale = Scale;
        return ChildGeometry;
    }

    /** 오프셋만 적용 (크기는 동일) */
    FGeometry MakeChildWithOffset(const FVector2D& ChildOffset) const
    {
        return MakeChild(ChildOffset, LocalSize);
    }

    // =====================================================
    // 히트 테스트
    // =====================================================

    /** 절대 좌표 점이 이 Geometry 영역 안에 있는지 검사 */
    bool IsPointInside(const FVector2D& AbsolutePoint) const
    {
        FVector2D LocalPoint = AbsoluteToLocal(AbsolutePoint);
        return LocalPoint.X >= 0 && LocalPoint.Y >= 0 &&
               LocalPoint.X <= LocalSize.X && LocalPoint.Y <= LocalSize.Y;
    }

    // =====================================================
    // 유틸리티
    // =====================================================

    /** 절대 좌표계에서의 크기 */
    FVector2D GetAbsoluteSize() const
    {
        return LocalSize * Scale;
    }

    /** 절대 좌표계에서의 사각형 영역 */
    FSlateRect GetAbsoluteRect() const
    {
        FVector2D AbsSize = GetAbsoluteSize();
        return FSlateRect(
            AbsolutePosition.X,
            AbsolutePosition.Y,
            AbsolutePosition.X + AbsSize.X,
            AbsolutePosition.Y + AbsSize.Y
        );
    }

    /** 중심점 (절대 좌표) */
    FVector2D GetAbsoluteCenter() const
    {
        FVector2D AbsSize = GetAbsoluteSize();
        return AbsolutePosition + AbsSize * 0.5f;
    }

    // =====================================================
    // 나중에 확장: 레이아웃 시스템용
    // =====================================================

    // /** 앵커 기반 위치 계산 (UMG 스타일) */
    // FGeometry MakeChildWithAnchors(
    //     const FVector2D& AnchorMin,     // (0,0) = 좌상단, (1,1) = 우하단
    //     const FVector2D& AnchorMax,
    //     const FVector2D& Offset,
    //     const FVector2D& Size
    // ) const;
    //
    // /** 정렬 기반 위치 계산 */
    // FGeometry MakeChildWithAlignment(
    //     const FVector2D& Alignment,     // (0,0) = 좌상단, (0.5,0.5) = 중앙
    //     const FVector2D& Size
    // ) const;
};
