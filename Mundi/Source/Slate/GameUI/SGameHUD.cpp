#include "pch.h"
#include "SGameHUD.h"

#include <dxgi.h>

// =====================================================
// 싱글톤
// =====================================================

SGameHUD& SGameHUD::Get()
{
    static SGameHUD Instance;
    return Instance;
}

// =====================================================
// 생명주기
// =====================================================

void SGameHUD::Initialize(ID3D11Device* Device, ID3D11DeviceContext* Context, IDXGISwapChain* InSwapChain)
{
    if (bInitialized)
        return;

    SwapChain = InSwapChain;

    // D2D 렌더러 초기화
    Renderer = MakeUnique<FD2DRenderer>();
    if (!Renderer->Initialize(Device, InSwapChain))
    {
        Renderer.reset();
        return;
    }

    // 루트 캔버스 생성
    RootCanvas = MakeShared<SCanvas>();

    // 화면 크기 초기화
    UpdateScreenSize();

    // 기본 뷰포트는 전체 화면
    ViewportPosition = FVector2D(0.f, 0.f);
    ViewportSize = ScreenSize;

    bInitialized = true;
}

void SGameHUD::Shutdown()
{
    if (!bInitialized)
        return;

    RootCanvas.Reset();
    Renderer.reset();
    SwapChain = nullptr;
    bInitialized = false;
}

// =====================================================
// 프레임 업데이트
// =====================================================

void SGameHUD::Update(float MouseX, float MouseY, bool bLeftButtonDown)
{
    if (!bInitialized || !RootCanvas)
        return;

    // 마우스 위치를 뷰포트 기준 상대좌표로 변환
    MousePosition = FVector2D(MouseX - ViewportPosition.X, MouseY - ViewportPosition.Y);

    // 뷰포트 영역을 루트 Geometry로 생성
    FGeometry RootGeometry = FGeometry::MakeRoot(ViewportSize, ViewportPosition);

    // 마우스 이동 처리
    RootCanvas->OnMouseMove(RootGeometry, FVector2D(MouseX, MouseY));

    // 마우스 버튼 상태 변화 감지
    if (bLeftButtonDown && !bWasLeftButtonDown)
    {
        // 버튼 다운
        RootCanvas->OnMouseButtonDown(RootGeometry, FVector2D(MouseX, MouseY));
    }
    else if (!bLeftButtonDown && bWasLeftButtonDown)
    {
        // 버튼 업
        RootCanvas->OnMouseButtonUp(RootGeometry, FVector2D(MouseX, MouseY));
    }

    bWasLeftButtonDown = bLeftButtonDown;
}

void SGameHUD::Render()
{
    if (!bInitialized || !RootCanvas || !Renderer || !SwapChain)
        return;

    // 화면 크기 업데이트 (리사이즈 대응)
    UpdateScreenSize();

    // 렌더 타겟 설정 및 프레임 시작
    if (!Renderer->BeginFrame())
        return;

    // 뷰포트 영역을 루트 Geometry로 생성
    FGeometry RootGeometry = FGeometry::MakeRoot(ViewportSize, ViewportPosition);

    // 위젯 트리 렌더링
    RootCanvas->Paint(*Renderer, RootGeometry);

    // 렌더링 완료
    Renderer->EndFrame();
}

// =====================================================
// 위젯 관리
// =====================================================

FCanvasSlot& SGameHUD::AddWidget(const TSharedPtr<SWidget>& Widget)
{
    return RootCanvas->AddChildToCanvas(Widget);
}

bool SGameHUD::RemoveWidget(const TSharedPtr<SWidget>& Widget)
{
    auto& Slots = RootCanvas->GetCanvasSlots();
    for (int32 i = 0; i < Slots.Num(); ++i)
    {
        if (Slots[i].Widget == Widget)
        {
            Slots.RemoveAt(i);
            return true;
        }
    }
    return false;
}

void SGameHUD::ClearWidgets()
{
    RootCanvas->GetCanvasSlots().Empty();
}

// =====================================================
// 상태 조회
// =====================================================

bool SGameHUD::IsMouseOverUI() const
{
    if (!bInitialized || !RootCanvas)
        return false;

    FGeometry RootGeometry = FGeometry::MakeRoot(ViewportSize, ViewportPosition);

    // 뷰포트 기준 절대좌표로 변환
    FVector2D AbsoluteMousePos = MousePosition + ViewportPosition;
    TSharedPtr<SWidget> Widget = RootCanvas->FindWidgetAt(RootGeometry, AbsoluteMousePos);

    return Widget != nullptr;
}

// =====================================================
// 뷰포트 설정
// =====================================================

void SGameHUD::SetViewport(const FVector2D& Position, const FVector2D& Size)
{
    ViewportPosition = Position;
    ViewportSize = Size;
}

void SGameHUD::SetViewportFullScreen()
{
    ViewportPosition = FVector2D(0.f, 0.f);
    ViewportSize = ScreenSize;
}

// =====================================================
// 내부 헬퍼
// =====================================================

void SGameHUD::UpdateScreenSize()
{
    if (!SwapChain)
        return;

    DXGI_SWAP_CHAIN_DESC Desc;
    if (SUCCEEDED(SwapChain->GetDesc(&Desc)))
    {
        ScreenSize = FVector2D(
            static_cast<float>(Desc.BufferDesc.Width),
            static_cast<float>(Desc.BufferDesc.Height)
        );
    }
}
