#pragma once

/**
 * @file SGameHUD.h
 * @brief 게임 HUD 관리자
 *
 * 게임 UI의 최상위 관리자입니다.
 * Direct2D 렌더링과 위젯 트리를 관리합니다.
 */

#include "SCanvas.h"
#include "FD2DRenderer.h"

struct ID3D11Device;
struct ID3D11DeviceContext;
struct IDXGISwapChain;

/**
 * @class SGameHUD
 * @brief 게임 HUD 관리자
 *
 * 사용 예:
 * @code
 * // 초기화
 * SGameHUD::Get().Initialize(device, context, swapChain);
 *
 * // 버튼 추가
 * auto Button = MakeShared<SButton>();
 * Button->SetText(L"시작하기")
 *        .OnClicked([]() { StartGame(); });
 *
 * SGameHUD::Get().AddWidget(Button)
 *     .SetPosition(100.f, 100.f)
 *     .SetSize(200.f, 50.f);
 *
 * // 매 프레임
 * SGameHUD::Get().Update(mouseX, mouseY, bLeftDown);
 * SGameHUD::Get().Render();
 * @endcode
 */
class SGameHUD
{
public:
    // =====================================================
    // 싱글톤
    // =====================================================

    static SGameHUD& Get();

    // =====================================================
    // 생명주기
    // =====================================================

    /**
     * D2D 리소스 초기화
     * @param Device D3D11 디바이스
     * @param Context D3D11 컨텍스트
     * @param InSwapChain DXGI 스왑체인
     */
    void Initialize(ID3D11Device* Device, ID3D11DeviceContext* Context, IDXGISwapChain* InSwapChain);

    /** 리소스 해제 */
    void Shutdown();

    /** 초기화 여부 */
    bool IsInitialized() const { return bInitialized; }

    // =====================================================
    // 프레임 업데이트
    // =====================================================

    /**
     * 입력 업데이트 (매 프레임 호출)
     * @param MouseX 마우스 X 좌표
     * @param MouseY 마우스 Y 좌표
     * @param bLeftButtonDown 왼쪽 버튼 눌림 상태
     */
    void Update(float MouseX, float MouseY, bool bLeftButtonDown);

    /** 렌더링 (매 프레임 호출) */
    void Render();

    // =====================================================
    // 위젯 관리
    // =====================================================

    /**
     * 위젯 추가
     * @param Widget 추가할 위젯
     * @return 체이닝용 CanvasSlot 참조
     */
    FCanvasSlot& AddWidget(const TSharedPtr<SWidget>& Widget);

    /**
     * 위젯 제거
     * @param Widget 제거할 위젯
     * @return 제거 성공 여부
     */
    bool RemoveWidget(const TSharedPtr<SWidget>& Widget);

    /** 모든 위젯 제거 */
    void ClearWidgets();

    /** 루트 캔버스 접근 */
    TSharedPtr<SCanvas> GetRootCanvas() { return RootCanvas; }

    // =====================================================
    // 뷰포트 설정
    // =====================================================

    /**
     * 뷰포트 영역 설정 (UI가 그려질 영역)
     * @param Position 뷰포트 시작 위치 (화면 좌표)
     * @param Size 뷰포트 크기
     */
    void SetViewport(const FVector2D& Position, const FVector2D& Size);

    /** 뷰포트를 전체 화면으로 설정 */
    void SetViewportFullScreen();

    // =====================================================
    // 상태 조회
    // =====================================================

    /** 화면 크기 */
    FVector2D GetScreenSize() const { return ScreenSize; }

    /** 뷰포트 위치 */
    FVector2D GetViewportPosition() const { return ViewportPosition; }

    /** 뷰포트 크기 */
    FVector2D GetViewportSize() const { return ViewportSize; }

    /** 마우스 위치 (뷰포트 기준) */
    FVector2D GetMousePosition() const { return MousePosition; }

    /** 마우스가 UI 위에 있는지 */
    bool IsMouseOverUI() const;

private:
    // =====================================================
    // 생성자 (싱글톤)
    // =====================================================

    SGameHUD() = default;
    ~SGameHUD() = default;
    SGameHUD(const SGameHUD&) = delete;
    SGameHUD& operator=(const SGameHUD&) = delete;

    // =====================================================
    // 내부 헬퍼
    // =====================================================

    /** 화면 크기 업데이트 */
    void UpdateScreenSize();

    // =====================================================
    // 데이터
    // =====================================================

    bool bInitialized = false;

    // D3D 참조 (Shutdown 시 필요)
    IDXGISwapChain* SwapChain = nullptr;

    // D2D 렌더러
    TUniquePtr<FD2DRenderer> Renderer;

    // 위젯 트리
    TSharedPtr<SCanvas> RootCanvas;

    // 화면 정보
    FVector2D ScreenSize;

    // 뷰포트 정보
    FVector2D ViewportPosition;
    FVector2D ViewportSize;

    // 입력 상태
    FVector2D MousePosition;
    bool bWasLeftButtonDown = false;
};
