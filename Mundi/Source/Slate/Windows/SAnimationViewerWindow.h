#pragma once
#include "SViewerWindow.h"
#include "Source/Runtime/Engine/Viewer/ViewerState.h"

class FViewport;
class FViewportClient;
class UWorld;
struct ID3D11Device;

class SAnimationViewerWindow : public SViewerWindow
{
public:
    SAnimationViewerWindow();
    virtual ~SAnimationViewerWindow();

    virtual void OnRender() override;
    virtual void OnUpdate(float DeltaSeconds) override;
    virtual void PreRenderViewportUpdate() override;

protected:
    virtual ViewerState* CreateViewerState(const char* Name, UEditorAssetPreviewContext* Context) override;
    virtual void DestroyViewerState(ViewerState*& State) override;
    virtual FString GetWindowTitle() const override { return "Animation Viewer"; }
    virtual void RenderRightPanel() override;

private:
    // Load a skeletal mesh into the active tab
    void LoadSkeletalMesh(ViewerState* State, const FString& Path);
    void RenderNotifyProperties();

    void AnimJumpToStart();
    void AnimJumpToEnd();
    void AnimStep(bool bForward);

    // High-level layout
    void RenderCenterPanel();

    // Center Panel Subsections
    void RenderViewportArea(float width, float height);
    void RenderTimelineArea(float width, float height);

    // Timeline Components
    void RenderTimelineControls();
    void RenderTrackAndGridLayout();
    void RenderLeftTrackList(float width, float RowHeight, float HeaderHeight, const TArray<FString>& LeftRows, const TArray<int>& RowToNotifyIndex);
    void RenderRightTimeline(float width, float RowHeight, float HeaderHeight, const TArray<FString>& LeftRows, const TArray<int>& RowToNotifyIndex);
    void RenderTimelineHeader(float height);
    void RenderTimelineGridBody(float RowHeight, const TArray<FString>& LeftRows, const TArray<int>& RowToNotifyIndex);

    // Timeline Helpers
    void BuildLeftRows(TArray<FString>& OutRows);
    void BuildRowToNotifyIndex(const TArray<FString>& InRows, TArray<int>& OutMapping);

    // ImGui draw callback for viewport rendering
    static void ViewportRenderCallback(const ImDrawList* parent_list, const ImDrawCmd* cmd);
};