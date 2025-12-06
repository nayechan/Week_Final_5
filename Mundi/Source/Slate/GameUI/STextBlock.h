#pragma once

/**
 * @file STextBlock.h
 * @brief 텍스트 표시 위젯
 *
 * 정적 또는 동적 텍스트를 표시합니다.
 * 점수 표시 등에 사용됩니다.
 */

#include "SWidget.h"
#include <functional>

/**
 * @class STextBlock
 * @brief 텍스트 표시 위젯
 */
class STextBlock : public SWidget
{
public:
    using FTextGetter = std::function<FWideString()>;

    // =====================================================
    // 생성자
    // =====================================================

    STextBlock() = default;
    virtual ~STextBlock() override = default;

    // =====================================================
    // 렌더링
    // =====================================================

    void Paint(FD2DRenderer& Renderer, const FGeometry& Geometry) override;

    // =====================================================
    // 텍스트 설정 (Fluent API)
    // =====================================================

    /** 정적 텍스트 설정 */
    STextBlock& SetText(const FWideString& InText);
    STextBlock& SetText(const FString& InText);

    /** 동적 텍스트 바인딩 (매 프레임 호출) */
    STextBlock& BindText(FTextGetter InGetter);

    /** 현재 텍스트 가져오기 */
    FWideString GetText() const;

    // =====================================================
    // 스타일 설정 (Fluent API)
    // =====================================================

    /** 색상 */
    STextBlock& SetColor(const FSlateColor& InColor);
    const FSlateColor& GetColor() const { return TextColor; }

    /** 폰트 크기 */
    STextBlock& SetFontSize(float InSize);
    float GetFontSize() const { return FontSize; }

    /** 가로 정렬 */
    STextBlock& SetHAlign(ETextHAlign InAlign);
    ETextHAlign GetHAlign() const { return HAlign; }

    /** 세로 정렬 */
    STextBlock& SetVAlign(ETextVAlign InAlign);
    ETextVAlign GetVAlign() const { return VAlign; }

    /** 그림자 효과 */
    STextBlock& SetShadow(bool bEnable, const FVector2D& Offset = FVector2D(1.f, 1.f), const FSlateColor& Color = FSlateColor::Black());

    /** 배경 이미지 설정 */
    STextBlock& SetBackgroundImage(const FString& ImagePath);

private:
    // =====================================================
    // 텍스트 데이터
    // =====================================================

    FWideString Text;
    FTextGetter TextGetter;  // 동적 텍스트용

    // =====================================================
    // 스타일
    // =====================================================

    FSlateColor TextColor = FSlateColor::White();
    float FontSize = 16.f;
    ETextHAlign HAlign = ETextHAlign::Left;
    ETextVAlign VAlign = ETextVAlign::Top;

    // 그림자
    bool bHasShadow = false;
    FVector2D ShadowOffset = FVector2D(1.f, 1.f);
    FSlateColor ShadowColor = FSlateColor::Black();

    // 배경 이미지
    FString BackgroundImagePath;
};
