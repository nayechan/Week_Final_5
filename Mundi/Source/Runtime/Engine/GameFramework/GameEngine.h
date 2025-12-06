#pragma once
#include "pch.h"

class URenderer;
class D3D11RHI;
class UWorld;
class FViewport;
class UClothManager;
class UGameInstance;

class UGameEngine final
{
public:
    UGameEngine();
    ~UGameEngine();

    bool Startup(HINSTANCE hInstance);
    void MainLoop();
    void Shutdown();

    bool IsPlayActive() const { return bPlayActive; }
    bool IsPIEActive() const { return bPlayActive; }

    HWND GetHWND() const { return HWnd; }

    URenderer* GetRenderer() const { return Renderer.get(); }
    D3D11RHI* GetRHIDevice() { return &RHIDevice; }
    UWorld* GetDefaultWorld();
    const TArray<FWorldContext>& GetWorldContexts() { return WorldContexts; }

    /** GameInstance 접근자 (PIE 세션 동안 유지됨) */
    UGameInstance* GetGameInstance() const { return GameInstance; }

    void AddWorldContext(const FWorldContext& InWorldContext)
    {
        WorldContexts.push_back(InWorldContext);
    }

private:
    bool CreateMainWindow(HINSTANCE hInstance);
    static LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
    static void GetViewportSize(HWND hWnd);

    void Tick(float DeltaSeconds);
    void Render();

    void HandleUVInput(float DeltaSeconds);

private:
    //윈도우 핸들
    HWND HWnd = nullptr;

    //디바이스 리소스 및 렌더러
    D3D11RHI RHIDevice;
    std::unique_ptr<URenderer> Renderer;

    //게임의 메인 뷰포트
    std::unique_ptr<FViewport> GameViewport;

    // Cloth 시뮬레이션 매니저
    UClothManager* ClothManager = nullptr;

    // GameInstance (PIE 세션 동안 유지)
    UGameInstance* GameInstance = nullptr;

    //월드 핸들
    TArray<FWorldContext> WorldContexts;

    //틱 상태
    bool bRunning = false;
    bool bUVScrollPaused = true;
    bool bPlayActive = false;
    float UVScrollTime = 0.0f;
    FVector2D UVScrollSpeed = FVector2D(0.5f, 0.5f);

    // 클라이언트 사이즈
    static float ClientWidth;
    static float ClientHeight;
};