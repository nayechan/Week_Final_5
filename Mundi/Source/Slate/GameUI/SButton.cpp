#include "pch.h"
#include "SButton.h"

// =====================================================
// 렌더링
// =====================================================

void SButton::Paint(FD2DRenderer& Renderer, const FGeometry& Geometry)
{
    if (!IsVisible())
        return;

    FVector2D Position = Geometry.AbsolutePosition;
    FVector2D Size = Geometry.GetAbsoluteSize();

    // 배경 그리기 (이미지 우선, 없으면 색상)
    bool bImageRendered = false;

    // 아틀라스 사용 여부 확인
    if (bUseAtlas && !AtlasImagePath.empty())
    {
        // 상태에 따라 아틀라스 영역 선택
        FSlateRect CurrentAtlasRect = NormalAtlasRect;

        if (bIsEnabled)
        {
            if (bIsPressed)
            {
                CurrentAtlasRect = PressedAtlasRect;
            }
            else if (bIsHovered)
            {
                CurrentAtlasRect = HoveredAtlasRect;
            }
        }

        FSlateColor ImageTint = bIsEnabled ? FSlateColor::White() : FSlateColor(0.5f, 0.5f, 0.5f, 0.7f);
        Renderer.DrawImageRegion(AtlasImagePath, Position, Size, CurrentAtlasRect, ImageTint);
        bImageRendered = true;
    }
    else
    {
        // 기존 개별 이미지 방식
        FString CurrentImagePath;

        // 상태에 따라 이미지 경로 선택
        if (!bIsEnabled)
        {
            // Disabled 상태는 Normal 이미지 사용 (회색 톤으로)
            CurrentImagePath = NormalImagePath;
        }
        else if (bIsPressed && !PressedImagePath.empty())
        {
            CurrentImagePath = PressedImagePath;
        }
        else if (bIsHovered && !HoveredImagePath.empty())
        {
            CurrentImagePath = HoveredImagePath;
        }
        else if (!NormalImagePath.empty())
        {
            CurrentImagePath = NormalImagePath;
        }

        // 이미지가 있으면 이미지로 렌더링
        if (!CurrentImagePath.empty())
        {
            FSlateColor ImageTint = bIsEnabled ? FSlateColor::White() : FSlateColor(0.5f, 0.5f, 0.5f, 0.7f);
            Renderer.DrawImage(CurrentImagePath, Position, Size, ImageTint);
            bImageRendered = true;
        }
    }

    // 이미지가 렌더링되지 않았으면 색상으로 렌더링
    if (!bImageRendered)
    {
        FSlateColor BgColor = GetCurrentBackgroundColor();

        if (CornerRadius > 0.f)
        {
            Renderer.DrawFilledRoundedRect(Position, Size, BgColor, CornerRadius);

            // 테두리
            if (BorderThickness > 0.f)
            {
                Renderer.DrawRoundedRect(Position, Size, BorderColor, CornerRadius, BorderThickness);
            }
        }
        else
        {
            Renderer.DrawFilledRect(Position, Size, BgColor);

            // 테두리
            if (BorderThickness > 0.f)
            {
                Renderer.DrawRect(Position, Size, BorderColor, BorderThickness);
            }
        }
    }

    // 텍스트 그리기 (중앙 정렬)
    if (!Text.empty())
    {
        FSlateColor CurrentTextColor = bIsEnabled ? TextColor : TextColor.WithAlpha(0.5f);

        Renderer.DrawText(
            Text,
            Position,
            Size,
            CurrentTextColor,
            FontSize,
            ETextHAlign::Center,
            ETextVAlign::Center
        );
    }
}

// =====================================================
// 입력 처리
// =====================================================

FReply SButton::OnMouseButtonDown(const FGeometry& Geometry, const FVector2D& MousePosition)
{
    if (!bIsEnabled)
        return FReply::Unhandled();

    if (HitTest(Geometry, MousePosition))
    {
        bIsPressed = true;
        return FReply::Handled();
    }

    return FReply::Unhandled();
}

FReply SButton::OnMouseButtonUp(const FGeometry& Geometry, const FVector2D& MousePosition)
{
    if (!bIsEnabled)
        return FReply::Unhandled();

    bool bWasPressed = bIsPressed;
    bIsPressed = false;

    // 눌린 상태에서 같은 위젯 위에서 뗐을 때만 클릭으로 인정
    if (bWasPressed && HitTest(Geometry, MousePosition))
    {
        // 클릭 이벤트 발생
        OnClickedDelegate.Broadcast();
        return FReply::Handled();
    }

    return FReply::Unhandled();
}

void SButton::OnMouseEnter()
{
    SWidget::OnMouseEnter();
    OnHoveredDelegate.Broadcast(true);
}

void SButton::OnMouseLeave()
{
    SWidget::OnMouseLeave();
    OnHoveredDelegate.Broadcast(false);
}

// =====================================================
// 텍스트 설정
// =====================================================

SButton& SButton::SetText(const FWideString& InText)
{
    Text = InText;
    return *this;
}

SButton& SButton::SetText(const FString& InText)
{
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
    return *this;
}

// =====================================================
// 스타일 설정
// =====================================================

SButton& SButton::SetBackgroundColor(const FSlateColor& Normal)
{
    NormalColor = Normal;
    // Hovered와 Pressed는 자동 계산
    HoveredColor = FSlateColor(
        FMath::Min(Normal.R + 0.1f, 1.f),
        FMath::Min(Normal.G + 0.1f, 1.f),
        FMath::Min(Normal.B + 0.1f, 1.f),
        Normal.A
    );
    PressedColor = FSlateColor(
        FMath::Max(Normal.R - 0.05f, 0.f),
        FMath::Max(Normal.G - 0.05f, 0.f),
        FMath::Max(Normal.B - 0.05f, 0.f),
        Normal.A
    );
    return *this;
}

SButton& SButton::SetBackgroundColors(const FSlateColor& Normal, const FSlateColor& Hovered, const FSlateColor& Pressed)
{
    NormalColor = Normal;
    HoveredColor = Hovered;
    PressedColor = Pressed;
    return *this;
}

SButton& SButton::SetTextColor(const FSlateColor& InColor)
{
    TextColor = InColor;
    return *this;
}

SButton& SButton::SetFontSize(float InSize)
{
    FontSize = InSize;
    return *this;
}

SButton& SButton::SetCornerRadius(float InRadius)
{
    CornerRadius = InRadius;
    return *this;
}

SButton& SButton::SetBorder(float Thickness, const FSlateColor& Color)
{
    BorderThickness = Thickness;
    BorderColor = Color;
    return *this;
}

SButton& SButton::SetBackgroundImage(const FString& NormalImagePath)
{
    this->NormalImagePath = NormalImagePath;
    // Hovered와 Pressed는 같은 이미지 사용
    this->HoveredImagePath = NormalImagePath;
    this->PressedImagePath = NormalImagePath;
    return *this;
}

SButton& SButton::SetBackgroundImages(const FString& NormalPath, const FString& HoveredPath, const FString& PressedPath)
{
    this->NormalImagePath = NormalPath;
    this->HoveredImagePath = HoveredPath;
    this->PressedImagePath = PressedPath;
    return *this;
}

SButton& SButton::SetBackgroundImageAtlas(const FString& AtlasPath, const FSlateRect& NormalRect, const FSlateRect& HoveredRect, const FSlateRect& PressedRect)
{
    bUseAtlas = true;
    AtlasImagePath = AtlasPath;
    NormalAtlasRect = NormalRect;
    HoveredAtlasRect = HoveredRect;
    PressedAtlasRect = PressedRect;
    return *this;
}

// =====================================================
// 이벤트 바인딩
// =====================================================

SButton& SButton::OnClicked(std::function<void()> Callback)
{
    OnClickedDelegate.Add(Callback);
    return *this;
}

SButton& SButton::OnHover(std::function<void(bool)> Callback)
{
    OnHoveredDelegate.Add(Callback);
    return *this;
}

// =====================================================
// 헬퍼
// =====================================================

FSlateColor SButton::GetCurrentBackgroundColor() const
{
    if (!bIsEnabled)
        return DisabledColor;
    if (bIsPressed)
        return PressedColor;
    if (bIsHovered)
        return HoveredColor;
    return NormalColor;
}
