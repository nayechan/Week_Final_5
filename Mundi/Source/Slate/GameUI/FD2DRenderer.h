#pragma once

/**
 * @file FD2DRenderer.h
 * @brief Direct2D 렌더링 래퍼 클래스
 *
 * Direct2D를 사용하여 2D UI 요소를 렌더링합니다.
 * 기존 StatsOverlayD2D의 D2D 초기화 코드를 재사용합니다.
 */

#include <d3d11.h>
#include <dxgi.h>
#include <d2d1_1.h>
#include <dwrite.h>

#include "SlateTypes.h"

/**
 * @class FD2DRenderer
 * @brief Direct2D 기반 2D 렌더링 클래스
 */
class FD2DRenderer
{
public:
    // =====================================================
    // 생명주기
    // =====================================================

    FD2DRenderer() = default;
    ~FD2DRenderer();

    // 복사 방지
    FD2DRenderer(const FD2DRenderer&) = delete;
    FD2DRenderer& operator=(const FD2DRenderer&) = delete;

    /**
     * Direct2D 초기화
     * @param InDevice D3D11 디바이스
     * @param InSwapChain 스왑체인 (렌더 타겟용)
     * @return 성공 여부
     */
    bool Initialize(ID3D11Device* InDevice, IDXGISwapChain* InSwapChain);

    /** 리소스 해제 */
    void Shutdown();

    /** 초기화 여부 */
    bool IsInitialized() const { return bInitialized; }

    // =====================================================
    // 프레임 관리
    // =====================================================

    /** 프레임 시작 - BeginDraw 호출 */
    bool BeginFrame();

    /** 프레임 종료 - EndDraw 호출 */
    void EndFrame();

    // =====================================================
    // 기본 도형 그리기
    // =====================================================

    /** 채워진 사각형 */
    void DrawFilledRect(const FVector2D& Position, const FVector2D& Size, const FSlateColor& Color);

    /** 테두리 사각형 */
    void DrawRect(const FVector2D& Position, const FVector2D& Size, const FSlateColor& Color, float Thickness = 1.0f);

    /** 채워진 둥근 사각형 */
    void DrawFilledRoundedRect(const FVector2D& Position, const FVector2D& Size, const FSlateColor& Color, float Radius);

    /** 테두리 둥근 사각형 */
    void DrawRoundedRect(const FVector2D& Position, const FVector2D& Size, const FSlateColor& Color, float Radius, float Thickness = 1.0f);

    /** 선 */
    void DrawLine(const FVector2D& Start, const FVector2D& End, const FSlateColor& Color, float Thickness = 1.0f);

    // =====================================================
    // 이미지 렌더링
    // =====================================================

    /**
     * 이미지 그리기
     * @param ImagePath 이미지 파일 경로
     * @param Position 위치
     * @param Size 크기
     * @param Tint 색상 틴트 (기본: 흰색 = 원본 색상 유지)
     */
    void DrawImage(const FString& ImagePath, const FVector2D& Position, const FVector2D& Size, const FSlateColor& Tint = FSlateColor::White());

    /**
     * 이미지의 특정 영역 그리기 (텍스처 아틀라스용)
     * @param ImagePath 이미지 파일 경로
     * @param Position 화면 위치
     * @param Size 화면 크기
     * @param SourceRect 원본 이미지에서 잘라낼 영역 (Left, Top, Right, Bottom)
     * @param Tint 색상 틴트 (기본: 흰색 = 원본 색상 유지)
     */
    void DrawImageRegion(const FString& ImagePath, const FVector2D& Position, const FVector2D& Size, const FSlateRect& SourceRect, const FSlateColor& Tint = FSlateColor::White());

    // =====================================================
    // 텍스트 렌더링
    // =====================================================

    /**
     * 텍스트 그리기
     * @param Text 텍스트 내용
     * @param Position 위치
     * @param Size 영역 크기
     * @param Color 색상
     * @param FontSize 폰트 크기
     * @param HAlign 가로 정렬
     * @param VAlign 세로 정렬
     */
    void DrawText(
        const FWideString& Text,
        const FVector2D& Position,
        const FVector2D& Size,
        const FSlateColor& Color,
        float FontSize = 16.0f,
        ETextHAlign HAlign = ETextHAlign::Left,
        ETextVAlign VAlign = ETextVAlign::Top
    );

    /** 단순 텍스트 (정렬 없음) */
    void DrawTextSimple(
        const FWideString& Text,
        const FVector2D& Position,
        const FSlateColor& Color,
        float FontSize = 16.0f
    );

    // =====================================================
    // 클리핑
    // =====================================================

    /** 클리핑 영역 푸시 */
    void PushClip(const FSlateRect& ClipRect);

    /** 클리핑 영역 팝 */
    void PopClip();

    // =====================================================
    // 유틸리티
    // =====================================================

    /** 화면 크기 */
    FVector2D GetScreenSize() const { return ScreenSize; }

    /** 화면 크기 업데이트 (리사이즈 시) */
    void OnScreenResize(const FVector2D& NewSize);

private:
    // =====================================================
    // 내부 헬퍼
    // =====================================================

    /** D2D 타겟 비트맵 생성/갱신 */
    bool UpdateRenderTarget();

    /** 브러시 색상 설정 */
    void SetBrushColor(const FSlateColor& Color);

    /** TextFormat 가져오기/생성 */
    IDWriteTextFormat* GetOrCreateTextFormat(float FontSize);

    // =====================================================
    // D3D11 리소스 (외부 소유)
    // =====================================================

    ID3D11Device* D3DDevice = nullptr;
    IDXGISwapChain* SwapChain = nullptr;

    // =====================================================
    // D2D 리소스 (내부 소유)
    // =====================================================

    ID2D1Factory1* D2DFactory = nullptr;
    ID2D1Device* D2DDevice = nullptr;
    ID2D1DeviceContext* D2DContext = nullptr;
    ID2D1Bitmap1* RenderTarget = nullptr;
    ID2D1SolidColorBrush* SolidBrush = nullptr;

    // =====================================================
    // DirectWrite 리소스
    // =====================================================

    IDWriteFactory* DWriteFactory = nullptr;
    TMap<int32, IDWriteTextFormat*> TextFormatCache;  // FontSize -> TextFormat

    // =====================================================
    // 이미지 리소스
    // =====================================================

    TMap<FString, ID2D1Bitmap*> BitmapCache;  // ImagePath -> Bitmap

    /** 비트맵 로드/캐시 */
    ID2D1Bitmap* GetOrLoadBitmap(const FString& ImagePath);

    // =====================================================
    // 상태
    // =====================================================

    bool bInitialized = false;
    bool bInFrame = false;
    FVector2D ScreenSize;
    int32 ClipStackDepth = 0;
};
