#pragma once
#include "SViewerWindow.h"

class UEditorAssetPreviewContext;

class SBlendSpaceEditorWindow : public SViewerWindow
{
public:
    SBlendSpaceEditorWindow();
    ~SBlendSpaceEditorWindow() override;

    // SWindow override
    void OnRender() override;

protected:
    // SViewerWindow requirements
    ViewerState* CreateViewerState(const char* Name, UEditorAssetPreviewContext* Context) override;
    void DestroyViewerState(ViewerState*& State) override;
    FString GetWindowTitle() const override { return "Blend Space 2D Editor"; }

private:
    void RenderCenterViewport(float Width, float Height);

    // ImGui draw callback for Direct3D viewport rendering
    static void ViewportRenderCallback(const ImDrawList* parent_list, const ImDrawCmd* cmd);

private:
    // Cached instance on preview skeletal mesh component
    class UAnimBlendSpaceInstance* BlendInst = nullptr;

    // UI state
    float UI_SampleX = 0.0f;
    float UI_SampleY = 0.0f;
    char  UI_SamplePath[260] = {0};
    float UI_SampleRate = 1.0f;
    bool  UI_SampleLoop = true;

    int   UI_Tri0 = 0, UI_Tri1 = 1, UI_Tri2 = 2;

    float UI_BlendX = 0.0f, UI_BlendY = 0.0f;
    float UI_PlayRate = 1.0f;
    bool  UI_Looping = true;
    int   UI_DriverIndex = 0;

    // Canvas state (param-space editor)
    FVector2D CanvasPan = FVector2D(0.0f, 0.0f);   // param units
    float     CanvasZoom = 150.0f;                 // pixels per 1.0 param unit
    int       SelectedSample = -1;
    int       HoveredSample = -1;
    bool      bDraggingSample = false;
    bool      bDraggingBlendPoint = false;
    FVector2D DragStartParam = FVector2D(0,0);
    FVector2D DragStartPan = FVector2D(0,0);
    float     CanvasHeightRatio = 0.35f;

    void RenderBlendCanvas(float Width, float Height);
    void RebuildTriangles();
};
