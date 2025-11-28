#include "pch.h"
#include "FD2DRenderer.h"

#include <dxgi1_2.h>

#pragma comment(lib, "d2d1")
#pragma comment(lib, "dwrite")

// =====================================================
// 헬퍼 함수
// =====================================================

static void SafeRelease(IUnknown* p)
{
    if (p) p->Release();
}

static D2D1_COLOR_F ToD2DColor(const FSlateColor& Color)
{
    return D2D1::ColorF(Color.R, Color.G, Color.B, Color.A);
}

// =====================================================
// 생명주기
// =====================================================

FD2DRenderer::~FD2DRenderer()
{
    Shutdown();
}

bool FD2DRenderer::Initialize(ID3D11Device* InDevice, IDXGISwapChain* InSwapChain)
{
    if (bInitialized)
        return true;

    if (!InDevice || !InSwapChain)
        return false;

    D3DDevice = InDevice;
    SwapChain = InSwapChain;

    // D2D Factory 생성
    D2D1_FACTORY_OPTIONS FactoryOptions = {};
#ifdef _DEBUG
    FactoryOptions.debugLevel = D2D1_DEBUG_LEVEL_INFORMATION;
#endif

    HRESULT Hr = D2D1CreateFactory(
        D2D1_FACTORY_TYPE_SINGLE_THREADED,
        __uuidof(ID2D1Factory1),
        &FactoryOptions,
        reinterpret_cast<void**>(&D2DFactory)
    );
    if (FAILED(Hr))
        return false;

    // DXGI Device 쿼리
    IDXGIDevice* DxgiDevice = nullptr;
    Hr = D3DDevice->QueryInterface(__uuidof(IDXGIDevice), reinterpret_cast<void**>(&DxgiDevice));
    if (FAILED(Hr))
    {
        Shutdown();
        return false;
    }

    // D2D Device 생성
    Hr = D2DFactory->CreateDevice(DxgiDevice, &D2DDevice);
    SafeRelease(DxgiDevice);
    if (FAILED(Hr))
    {
        Shutdown();
        return false;
    }

    // D2D DeviceContext 생성
    Hr = D2DDevice->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &D2DContext);
    if (FAILED(Hr))
    {
        Shutdown();
        return false;
    }

    // DWrite Factory 생성
    Hr = DWriteCreateFactory(
        DWRITE_FACTORY_TYPE_SHARED,
        __uuidof(IDWriteFactory),
        reinterpret_cast<IUnknown**>(&DWriteFactory)
    );
    if (FAILED(Hr))
    {
        Shutdown();
        return false;
    }

    // 기본 브러시 생성
    Hr = D2DContext->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White), &SolidBrush);
    if (FAILED(Hr))
    {
        Shutdown();
        return false;
    }

    bInitialized = true;
    return true;
}

void FD2DRenderer::Shutdown()
{
    if (!bInitialized)
        return;

    // TextFormat 캐시 해제
    for (auto& Pair : TextFormatCache)
    {
        SafeRelease(Pair.second);
    }
    TextFormatCache.Empty();

    SafeRelease(RenderTarget);
    SafeRelease(SolidBrush);
    SafeRelease(DWriteFactory);
    SafeRelease(D2DContext);
    SafeRelease(D2DDevice);
    SafeRelease(D2DFactory);

    D3DDevice = nullptr;
    SwapChain = nullptr;
    bInitialized = false;
}

// =====================================================
// 프레임 관리
// =====================================================

bool FD2DRenderer::BeginFrame()
{
    if (!bInitialized || bInFrame)
        return false;

    // 렌더 타겟 업데이트
    if (!UpdateRenderTarget())
        return false;

    D2DContext->SetTarget(RenderTarget);
    D2DContext->BeginDraw();
    bInFrame = true;
    return true;
}

void FD2DRenderer::EndFrame()
{
    if (!bInFrame)
        return;

    D2DContext->EndDraw();
    D2DContext->SetTarget(nullptr);

    // 렌더 타겟 해제 (스왑체인 리사이즈 대비)
    SafeRelease(RenderTarget);
    RenderTarget = nullptr;

    bInFrame = false;
}

bool FD2DRenderer::UpdateRenderTarget()
{
    if (RenderTarget)
        return true;

    if (!SwapChain)
        return false;

    // 스왑체인 백버퍼 가져오기
    IDXGISurface* Surface = nullptr;
    HRESULT Hr = SwapChain->GetBuffer(0, __uuidof(IDXGISurface), reinterpret_cast<void**>(&Surface));
    if (FAILED(Hr))
        return false;

    // 화면 크기 업데이트
    DXGI_SURFACE_DESC SurfaceDesc;
    Surface->GetDesc(&SurfaceDesc);
    ScreenSize = FVector2D(static_cast<float>(SurfaceDesc.Width), static_cast<float>(SurfaceDesc.Height));

    // D2D 비트맵 생성
    D2D1_BITMAP_PROPERTIES1 BitmapProps = {};
    BitmapProps.pixelFormat.format = DXGI_FORMAT_B8G8R8A8_UNORM;
    BitmapProps.pixelFormat.alphaMode = D2D1_ALPHA_MODE_PREMULTIPLIED;
    BitmapProps.dpiX = 96.0f;
    BitmapProps.dpiY = 96.0f;
    BitmapProps.bitmapOptions = D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW;

    Hr = D2DContext->CreateBitmapFromDxgiSurface(Surface, &BitmapProps, &RenderTarget);
    SafeRelease(Surface);

    return SUCCEEDED(Hr);
}

// =====================================================
// 기본 도형 그리기
// =====================================================

void FD2DRenderer::DrawFilledRect(const FVector2D& Position, const FVector2D& Size, const FSlateColor& Color)
{
    if (!bInFrame) return;

    SetBrushColor(Color);
    D2D1_RECT_F Rect = D2D1::RectF(Position.X, Position.Y, Position.X + Size.X, Position.Y + Size.Y);
    D2DContext->FillRectangle(Rect, SolidBrush);
}

void FD2DRenderer::DrawRect(const FVector2D& Position, const FVector2D& Size, const FSlateColor& Color, float Thickness)
{
    if (!bInFrame) return;

    SetBrushColor(Color);
    D2D1_RECT_F Rect = D2D1::RectF(Position.X, Position.Y, Position.X + Size.X, Position.Y + Size.Y);
    D2DContext->DrawRectangle(Rect, SolidBrush, Thickness);
}

void FD2DRenderer::DrawFilledRoundedRect(const FVector2D& Position, const FVector2D& Size, const FSlateColor& Color, float Radius)
{
    if (!bInFrame) return;

    SetBrushColor(Color);
    D2D1_ROUNDED_RECT RoundedRect = D2D1::RoundedRect(
        D2D1::RectF(Position.X, Position.Y, Position.X + Size.X, Position.Y + Size.Y),
        Radius, Radius
    );
    D2DContext->FillRoundedRectangle(RoundedRect, SolidBrush);
}

void FD2DRenderer::DrawRoundedRect(const FVector2D& Position, const FVector2D& Size, const FSlateColor& Color, float Radius, float Thickness)
{
    if (!bInFrame) return;

    SetBrushColor(Color);
    D2D1_ROUNDED_RECT RoundedRect = D2D1::RoundedRect(
        D2D1::RectF(Position.X, Position.Y, Position.X + Size.X, Position.Y + Size.Y),
        Radius, Radius
    );
    D2DContext->DrawRoundedRectangle(RoundedRect, SolidBrush, Thickness);
}

void FD2DRenderer::DrawLine(const FVector2D& Start, const FVector2D& End, const FSlateColor& Color, float Thickness)
{
    if (!bInFrame) return;

    SetBrushColor(Color);
    D2DContext->DrawLine(
        D2D1::Point2F(Start.X, Start.Y),
        D2D1::Point2F(End.X, End.Y),
        SolidBrush,
        Thickness
    );
}

// =====================================================
// 텍스트 렌더링
// =====================================================

void FD2DRenderer::DrawText(
    const FWideString& Text,
    const FVector2D& Position,
    const FVector2D& Size,
    const FSlateColor& Color,
    float FontSize,
    ETextHAlign HAlign,
    ETextVAlign VAlign)
{
    if (!bInFrame || Text.empty()) return;

    IDWriteTextFormat* TextFormat = GetOrCreateTextFormat(FontSize);
    if (!TextFormat) return;

    // 정렬 설정
    DWRITE_TEXT_ALIGNMENT DWriteHAlign = DWRITE_TEXT_ALIGNMENT_LEADING;
    switch (HAlign)
    {
    case ETextHAlign::Left:   DWriteHAlign = DWRITE_TEXT_ALIGNMENT_LEADING; break;
    case ETextHAlign::Center: DWriteHAlign = DWRITE_TEXT_ALIGNMENT_CENTER; break;
    case ETextHAlign::Right:  DWriteHAlign = DWRITE_TEXT_ALIGNMENT_TRAILING; break;
    }

    DWRITE_PARAGRAPH_ALIGNMENT DWriteVAlign = DWRITE_PARAGRAPH_ALIGNMENT_NEAR;
    switch (VAlign)
    {
    case ETextVAlign::Top:    DWriteVAlign = DWRITE_PARAGRAPH_ALIGNMENT_NEAR; break;
    case ETextVAlign::Center: DWriteVAlign = DWRITE_PARAGRAPH_ALIGNMENT_CENTER; break;
    case ETextVAlign::Bottom: DWriteVAlign = DWRITE_PARAGRAPH_ALIGNMENT_FAR; break;
    }

    TextFormat->SetTextAlignment(DWriteHAlign);
    TextFormat->SetParagraphAlignment(DWriteVAlign);

    SetBrushColor(Color);
    D2D1_RECT_F Rect = D2D1::RectF(Position.X, Position.Y, Position.X + Size.X, Position.Y + Size.Y);
    D2DContext->DrawTextW(
        Text.c_str(),
        static_cast<UINT32>(Text.length()),
        TextFormat,
        Rect,
        SolidBrush
    );
}

void FD2DRenderer::DrawTextSimple(
    const FWideString& Text,
    const FVector2D& Position,
    const FSlateColor& Color,
    float FontSize)
{
    // 충분히 큰 영역으로 그리기
    DrawText(Text, Position, FVector2D(10000.f, 10000.f), Color, FontSize, ETextHAlign::Left, ETextVAlign::Top);
}

IDWriteTextFormat* FD2DRenderer::GetOrCreateTextFormat(float FontSize)
{
    int32 FontSizeKey = static_cast<int32>(FontSize);

    // 캐시에서 찾기
    if (IDWriteTextFormat** Found = TextFormatCache.Find(FontSizeKey))
    {
        return *Found;
    }

    // 새로 생성
    IDWriteTextFormat* NewFormat = nullptr;
    HRESULT Hr = DWriteFactory->CreateTextFormat(
        L"Segoe UI",
        nullptr,
        DWRITE_FONT_WEIGHT_NORMAL,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        FontSize,
        L"en-us",
        &NewFormat
    );

    if (FAILED(Hr))
        return nullptr;

    TextFormatCache.Add(FontSizeKey, NewFormat);
    return NewFormat;
}

// =====================================================
// 클리핑
// =====================================================

void FD2DRenderer::PushClip(const FSlateRect& ClipRect)
{
    if (!bInFrame) return;

    D2D1_RECT_F Rect = D2D1::RectF(ClipRect.Left, ClipRect.Top, ClipRect.Right, ClipRect.Bottom);
    D2DContext->PushAxisAlignedClip(Rect, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
    ClipStackDepth++;
}

void FD2DRenderer::PopClip()
{
    if (!bInFrame || ClipStackDepth <= 0) return;

    D2DContext->PopAxisAlignedClip();
    ClipStackDepth--;
}

// =====================================================
// 유틸리티
// =====================================================

void FD2DRenderer::SetBrushColor(const FSlateColor& Color)
{
    if (SolidBrush)
    {
        SolidBrush->SetColor(ToD2DColor(Color));
    }
}

void FD2DRenderer::OnScreenResize(const FVector2D& NewSize)
{
    ScreenSize = NewSize;
    // 렌더 타겟은 다음 BeginFrame에서 자동 갱신
    SafeRelease(RenderTarget);
    RenderTarget = nullptr;
}
