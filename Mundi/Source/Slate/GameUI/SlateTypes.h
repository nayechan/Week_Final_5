#pragma once

/**
 * @file SlateTypes.h
 * @brief 게임 UI 시스템의 공통 타입 정의
 *
 * 기존 시스템 활용:
 * - Delegates.h: TDelegate 시스템
 * - InputManager.h: UInputManager 입력 처리
 * - PointerTypes.h: 스마트 포인터
 */

#include "Source/Runtime/Core/Memory/PointerTypes.h"
#include "Vector.h"
#include "UEContainer.h"
#include "Delegates.h"

// =====================================================
// Delegate Type Definitions for UI
// =====================================================

DECLARE_DELEGATE_TYPE(FOnClicked);                  // void()
DECLARE_DELEGATE_TYPE(FOnHovered, bool);            // void(bool bIsHovered)
DECLARE_DELEGATE_TYPE(FOnPressed);                  // void()
DECLARE_DELEGATE_TYPE(FOnReleased);                 // void()

// =====================================================
// Visibility
// =====================================================

enum class ESlateVisibility : uint8
{
    Visible,            // 보이고 히트 테스트 가능
    Hidden,             // 안 보이지만 공간 차지
    Collapsed,          // 안 보이고 공간도 없음
    HitTestInvisible,   // 보이지만 클릭 불가
    SelfHitTestInvisible // 자신은 클릭 불가, 자식은 가능
};

// =====================================================
// Text Alignment
// =====================================================

enum class ETextHAlign : uint8
{
    Left,
    Center,
    Right
};

enum class ETextVAlign : uint8
{
    Top,
    Center,
    Bottom
};

// =====================================================
// FSlateColor - RGBA 색상 (0.0 ~ 1.0 범위)
// =====================================================

struct FSlateColor
{
    float R, G, B, A;

    FSlateColor() : R(1.f), G(1.f), B(1.f), A(1.f) {}
    FSlateColor(float InR, float InG, float InB, float InA = 1.f)
        : R(InR), G(InG), B(InB), A(InA) {}

    // 정적 팩토리 메서드
    static FSlateColor White()       { return FSlateColor(1.f, 1.f, 1.f, 1.f); }
    static FSlateColor Black()       { return FSlateColor(0.f, 0.f, 0.f, 1.f); }
    static FSlateColor Red()         { return FSlateColor(1.f, 0.f, 0.f, 1.f); }
    static FSlateColor Green()       { return FSlateColor(0.f, 1.f, 0.f, 1.f); }
    static FSlateColor Blue()        { return FSlateColor(0.f, 0.f, 1.f, 1.f); }
    static FSlateColor Yellow()      { return FSlateColor(1.f, 1.f, 0.f, 1.f); }
    static FSlateColor Cyan()        { return FSlateColor(0.f, 1.f, 1.f, 1.f); }
    static FSlateColor Magenta()     { return FSlateColor(1.f, 0.f, 1.f, 1.f); }
    static FSlateColor Gray()        { return FSlateColor(0.5f, 0.5f, 0.5f, 1.f); }
    static FSlateColor DarkGray()    { return FSlateColor(0.2f, 0.2f, 0.2f, 1.f); }
    static FSlateColor LightGray()   { return FSlateColor(0.75f, 0.75f, 0.75f, 1.f); }
    static FSlateColor Transparent() { return FSlateColor(0.f, 0.f, 0.f, 0.f); }

    FSlateColor operator*(float Scalar) const
    {
        return FSlateColor(R * Scalar, G * Scalar, B * Scalar, A);
    }

    FSlateColor WithAlpha(float NewAlpha) const
    {
        return FSlateColor(R, G, B, NewAlpha);
    }

    static FSlateColor Lerp(const FSlateColor& A, const FSlateColor& B, float T)
    {
        return FSlateColor(
            A.R + (B.R - A.R) * T,
            A.G + (B.G - A.G) * T,
            A.B + (B.B - A.B) * T,
            A.A + (B.A - A.A) * T
        );
    }
};

// =====================================================
// FReply - 이벤트 처리 결과
// =====================================================

struct FReply
{
private:
    bool bHandled = false;

public:
    static FReply Handled()   { FReply R; R.bHandled = true;  return R; }
    static FReply Unhandled() { FReply R; R.bHandled = false; return R; }

    bool IsEventHandled() const { return bHandled; }
};

// =====================================================
// FSlateRect - 2D 축 정렬 사각형 (AABB)
// =====================================================

struct FSlateRect
{
    float Left, Top, Right, Bottom;

    FSlateRect() : Left(0), Top(0), Right(0), Bottom(0) {}
    FSlateRect(float InLeft, float InTop, float InRight, float InBottom)
        : Left(InLeft), Top(InTop), Right(InRight), Bottom(InBottom) {}

    static FSlateRect FromPositionSize(const FVector2D& Position, const FVector2D& Size)
    {
        return FSlateRect(Position.X, Position.Y, Position.X + Size.X, Position.Y + Size.Y);
    }

    float GetWidth() const  { return Right - Left; }
    float GetHeight() const { return Bottom - Top; }
    FVector2D GetSize() const { return FVector2D(GetWidth(), GetHeight()); }
    FVector2D GetTopLeft() const { return FVector2D(Left, Top); }
    FVector2D GetBottomRight() const { return FVector2D(Right, Bottom); }
    FVector2D GetCenter() const
    {
        return FVector2D((Left + Right) * 0.5f, (Top + Bottom) * 0.5f);
    }

    bool ContainsPoint(const FVector2D& Point) const
    {
        return Point.X >= Left && Point.X <= Right &&
               Point.Y >= Top  && Point.Y <= Bottom;
    }

    FSlateRect IntersectionWith(const FSlateRect& Other) const
    {
        return FSlateRect(
            FMath::Max(Left, Other.Left),
            FMath::Max(Top, Other.Top),
            FMath::Min(Right, Other.Right),
            FMath::Min(Bottom, Other.Bottom)
        );
    }

    bool IsEmpty() const
    {
        return Right <= Left || Bottom <= Top;
    }

    FSlateRect ExpandBy(float Amount) const
    {
        return FSlateRect(Left - Amount, Top - Amount, Right + Amount, Bottom + Amount);
    }
};

// =====================================================
// Forward Declarations
// =====================================================

class SWidget;
class SPanel;
class SCanvas;
class STextBlock;
class SButton;
class SImage;
class FD2DRenderer;
struct FGeometry;
struct FSlot;
