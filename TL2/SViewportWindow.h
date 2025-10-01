#pragma once
#include "SWindow.h"
#include "Enums.h"

class FViewport;
class FViewportClient;

class SViewportWindow : public SWindow
{
public:
    SViewportWindow();
    virtual ~SViewportWindow();

    bool Initialize(float StartX, float StartY, float Width, float Height, UWorld* World, ID3D11Device* Device, EViewportType InViewportType);

    virtual void OnRender() override;
    virtual void OnUpdate(float DeltaSeconds) override;

    virtual void OnMouseMove(FVector2D MousePos) override;
    virtual void OnMouseDown(FVector2D MousePos, uint32 Button) override;
    virtual void OnMouseUp(FVector2D MousePos, uint32 Button) override;

    void SetActive(bool bInActive) { bIsActive = bInActive; }
    bool IsActive() const { return bIsActive; }

    FViewport* GetViewport() const { return Viewport; }
    FViewportClient* GetViewportClient() const { return ViewportClient; }
    void SetVClientWorld(UWorld* InWorld);
private:
    void RenderToolbar();

private:
    FViewport* Viewport = nullptr;
    FViewportClient* ViewportClient = nullptr;

    EViewportType ViewportType;
    FName ViewportName;

    bool bIsActive;
    bool bIsMouseDown;
};