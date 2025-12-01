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
    void SetShowTileCulling(bool b);
    void SetShowLights(bool b);
    void SetShowShadow(bool b);
    void SetShowSkinning(bool b);
    void SetShowParticles(bool b);
    void SetShowPhysics(bool b);
    void ToggleFPS();
    void ToggleMemory();
    void TogglePicking();
    void ToggleDecal();
    void ToggleTileCulling();
    void ToggleLights();
    void ToggleShadow();
    void ToggleSkinning();
    void ToggleParticles();
    void TogglePhysics();
    bool IsFPSVisible() const { return bShowFPS; }
    bool IsMemoryVisible() const { return bShowMemory; }
    bool IsPickingVisible() const { return bShowPicking; }
    bool IsDecalVisible() const { return bShowDecal; }
    bool IsTileCullingVisible() const { return bShowTileCulling; }
    bool IsLightsVisible() const { return bShowLights; }
    bool IsShadowVisible() const { return bShowShadow; }
    bool IsSkinningVisible() const { return bShowSkinning; }
    bool IsParticlesVisible() const { return bShowParticles; }
    bool IsPhysicsVisible() const { return bShowPhysics; }

private:
    UStatsOverlayD2D() = default;
    ~UStatsOverlayD2D() = default;
    UStatsOverlayD2D(const UStatsOverlayD2D&) = delete;
    UStatsOverlayD2D& operator=(const UStatsOverlayD2D&) = delete;

    void EnsureInitialized();
    void ReleaseD2DTarget();

private:
    bool bInitialized = false;
    bool bShowFPS = true;
    bool bShowMemory = false;
    bool bShowPicking = false;
    bool bShowDecal = false;
    bool bShowTileCulling = false;
    bool bShowShadow = false;
    bool bShowLights = false;
    bool bShowSkinning = false;
    bool bShowParticles = false;
    bool bShowPhysics = false;

    ID3D11Device* D3DDevice = nullptr;
    ID3D11DeviceContext* D3DContext = nullptr;
    IDXGISwapChain* SwapChain = nullptr;

    // D2D 리소스 캐싱 (매 프레임 재사용)
    struct ID2D1Factory1* D2dFactory = nullptr;
    struct ID2D1Device* D2dDevice = nullptr;
    struct ID2D1DeviceContext* D2dCtx = nullptr;
    struct IDWriteFactory* Dwrite = nullptr;
    struct IDWriteTextFormat* TextFormat = nullptr;
    struct ID2D1SolidColorBrush* CachedBrush = nullptr;
    bool bD2DInitialized = false;
};
