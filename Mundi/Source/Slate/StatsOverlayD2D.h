#pragma once

#include <d3d11.h>
#include <dxgi.h>

class UStatsOverlayD2D
{
public:
    static UStatsOverlayD2D& Get();

    void Initialize(ID3D11Device* device, ID3D11DeviceContext* context, IDXGISwapChain* swapChain);
	void Shutdown();
    void Draw();

    void SetShowFPS(bool b); 
    void SetShowMemory(bool b);
    void SetShowPicking(bool b);
    void SetShowDecal(bool b);
    void ToggleFPS();
    void ToggleMemory();
    void TogglePicking();
    void ToggleDecal();
    bool IsFPSVisible() const { return bShowFPS; }
    bool IsMemoryVisible() const { return bShowMemory; }

private:
    UStatsOverlayD2D() = default;
    ~UStatsOverlayD2D() = default;
    UStatsOverlayD2D(const UStatsOverlayD2D&) = delete;
    UStatsOverlayD2D& operator=(const UStatsOverlayD2D&) = delete;

    void EnsureInitialized();
    void ReleaseD2DTarget();

private:
    bool bInitialized = false;
    bool bShowFPS = false;
    bool bShowMemory = false;
    bool bShowPicking = false;
    bool bShowDecal = true;     // NOTE: Week06 에서는 데칼 stat을 기본으로 표시

    ID3D11Device* D3DDevice = nullptr;
    ID3D11DeviceContext* D3DContext = nullptr;
    IDXGISwapChain* SwapChain = nullptr;
};
