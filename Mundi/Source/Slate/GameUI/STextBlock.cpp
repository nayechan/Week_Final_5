#include "pch.h"
#include "STextBlock.h"

// =====================================================
// 렌더링
// =====================================================

void STextBlock::Paint(FD2DRenderer& Renderer, const FGeometry& Geometry)
{
    if (!IsVisible())
        return;

    FVector2D Position = Geometry.AbsolutePosition;
    FVector2D Size = Geometry.GetAbsoluteSize();

    // 배경 이미지 먼저 그리기
    if (!BackgroundImagePath.empty())
    {
        Renderer.DrawImage(BackgroundImagePath, Position, Size);
    }

    FWideString DisplayText = GetText();
    if (DisplayText.empty())
        return;

    // 그림자 먼저 그리기
    if (bHasShadow)
    {
        Renderer.DrawText(
            DisplayText,
            Position + ShadowOffset,
            Size,
            ShadowColor,
            FontSize,
            HAlign,
            VAlign
        );
    }

    // 메인 텍스트
    Renderer.DrawText(
        DisplayText,
        Position,
        Size,
        TextColor,
        FontSize,
        HAlign,
        VAlign
    );
}

// =====================================================
// 텍스트 설정
// =====================================================

STextBlock& STextBlock::SetText(const FWideString& InText)
{
    Text = InText;
    TextGetter = nullptr;  // 정적 텍스트 사용 시 바인딩 해제
    return *this;
}

STextBlock& STextBlock::SetText(const FString& InText)
{
    // ANSI -> Wide 변환
    if (InText.empty())
    {
        Text.clear();
    }
    else
    {
        int WideLen = MultiByteToWideChar(CP_UTF8, 0, InText.c_str(), -1, nullptr, 0);
        Text.resize(static_cast<size_t>(WideLen - 1));
        MultiByteToWideChar(CP_UTF8, 0, InText.c_str(), -1, Text.data(), WideLen);
    }
    TextGetter = nullptr;
    return *this;
}

STextBlock& STextBlock::BindText(FTextGetter InGetter)
{
    TextGetter = std::move(InGetter);
    return *this;
}

FWideString STextBlock::GetText() const
{
    if (TextGetter)
    {
        return TextGetter();
    }
    return Text;
}

// =====================================================
// 스타일 설정
// =====================================================

STextBlock& STextBlock::SetColor(const FSlateColor& InColor)
{
    TextColor = InColor;
    return *this;
}

STextBlock& STextBlock::SetFontSize(float InSize)
{
    FontSize = InSize;
    return *this;
}

STextBlock& STextBlock::SetHAlign(ETextHAlign InAlign)
{
    HAlign = InAlign;
    return *this;
}

STextBlock& STextBlock::SetVAlign(ETextVAlign InAlign)
{
    VAlign = InAlign;
    return *this;
}

STextBlock& STextBlock::SetShadow(bool bEnable, const FVector2D& Offset, const FSlateColor& Color)
{
    bHasShadow = bEnable;
    ShadowOffset = Offset;
    ShadowColor = Color;
    return *this;
}

STextBlock& STextBlock::SetBackgroundImage(const FString& ImagePath)
{
    BackgroundImagePath = ImagePath;
    return *this;
}
