#pragma once

/**
 * @file SButton.h
 * @brief 클릭 가능한 버튼 위젯
 *
 * 마우스 클릭에 반응하는 버튼입니다.
 * "시작하기" 버튼 등에 사용됩니다.
 */

#include "SWidget.h"

/**
 * @class SButton
 * @brief 버튼 위젯
 */
class SButton : public SWidget
{
public:
    // =====================================================
    // 생성자
    // =====================================================

    SButton() = default;
    virtual ~SButton() override = default;

    // =====================================================
    // 렌더링
    // =====================================================

    void Paint(FD2DRenderer& Renderer, const FGeometry& Geometry) override;

    // =====================================================
    // 입력 처리
    // =====================================================

    FReply OnMouseButtonDown(const FGeometry& Geometry, const FVector2D& MousePosition) override;
    FReply OnMouseButtonUp(const FGeometry& Geometry, const FVector2D& MousePosition) override;
    void OnMouseEnter() override;
    void OnMouseLeave() override;

    // =====================================================
    // 텍스트 설정 (Fluent API)
    // =====================================================

    SButton& SetText(const FWideString& InText);
    SButton& SetText(const FString& InText);
    const FWideString& GetText() const { return Text; }

    // =====================================================
    // 스타일 설정 (Fluent API)
    // =====================================================

    /** 배경 색상 (Normal, Hovered, Pressed) */
    SButton& SetBackgroundColor(const FSlateColor& Normal);
    SButton& SetBackgroundColors(const FSlateColor& Normal, const FSlateColor& Hovered, const FSlateColor& Pressed);

    /** 텍스트 색상 */
    SButton& SetTextColor(const FSlateColor& InColor);

    /** 폰트 크기 */
    SButton& SetFontSize(float InSize);

    /** 모서리 반경 */
    SButton& SetCornerRadius(float InRadius);

    /** 테두리 */
    SButton& SetBorder(float Thickness, const FSlateColor& Color);

    // =====================================================
    // 이벤트 바인딩
    // =====================================================

    /** 클릭 이벤트 */
    SButton& OnClicked(std::function<void()> Callback);

    /** 호버 이벤트 */
    SButton& OnHover(std::function<void(bool)> Callback);

    // =====================================================
    // 델리게이트 접근 (외부 바인딩용)
    // =====================================================

    FOnClicked& GetOnClickedDelegate() { return OnClickedDelegate; }
    FOnHovered& GetOnHoveredDelegate() { return OnHoveredDelegate; }

private:
    // =====================================================
    // 헬퍼
    // =====================================================

    FSlateColor GetCurrentBackgroundColor() const;

    // =====================================================
    // 텍스트
    // =====================================================

    FWideString Text;
    FSlateColor TextColor = FSlateColor::White();
    float FontSize = 16.f;

    // =====================================================
    // 배경 스타일
    // =====================================================

    FSlateColor NormalColor = FSlateColor(0.15f, 0.15f, 0.15f, 1.f);
    FSlateColor HoveredColor = FSlateColor(0.25f, 0.25f, 0.25f, 1.f);
    FSlateColor PressedColor = FSlateColor(0.1f, 0.1f, 0.1f, 1.f);
    FSlateColor DisabledColor = FSlateColor(0.3f, 0.3f, 0.3f, 0.5f);

    float CornerRadius = 4.f;

    // 테두리
    float BorderThickness = 0.f;
    FSlateColor BorderColor = FSlateColor::White();

    // =====================================================
    // 델리게이트
    // =====================================================

    FOnClicked OnClickedDelegate;
    FOnHovered OnHoveredDelegate;
};
