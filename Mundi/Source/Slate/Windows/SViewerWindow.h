#pragma once
#include "SWindow.h"
#include "Source/Runtime/Engine/Viewer/ViewerState.h"

class UEditorAssetPreviewContext;
class SViewerWindow : public SWindow
{
public:
	SViewerWindow();
	virtual ~SViewerWindow();

	bool Initialize(float StartX, float StartY, float Width, float Height, UWorld* InWorld, 
		ID3D11Device* InDevice, UEditorAssetPreviewContext* InContext);

	// SWindow overrides
	virtual void OnUpdate(float DeltaSeconds) override;
	virtual void OnMouseMove(FVector2D MousePos) override;
	virtual void OnMouseDown(FVector2D MousePos, uint32 Button) override;
	virtual void OnMouseUp(FVector2D MousePos, uint32 Button) override;

	void RequestFocus() { bRequestFocus = true; }
	void OnRenderViewport();
	virtual void PreRenderViewportUpdate() {}

	// Accessors (active tab)
	FViewport* GetViewport() const { return ActiveState ? ActiveState->Viewport : nullptr; }
	FViewportClient* GetViewportClient() const { return ActiveState ? ActiveState->Client : nullptr; }
	UEditorAssetPreviewContext* GetContext() const { return Context; }
	ViewerState* GetActiveState() const { return ActiveState; }

	// 윈도우 상태 접근자
	bool IsWindowHovered() const { return bIsWindowHovered; }
	bool IsWindowFocused() const { return bIsWindowFocused; }
	const FRect& GetCenterRect() const { return CenterRect; }

protected:
	// Per-tab state
	UEditorAssetPreviewContext* Context = nullptr;
	ViewerState* ActiveState = nullptr;
	TArray<ViewerState*> Tabs;
	int ActiveTabIndex = -1;

	// For legacy single-state flows; removed once tabs are stable
	UWorld* World = nullptr;
	ID3D11Device* Device = nullptr;

	// Cached center region used for viewport sizing and input mapping
	FRect CenterRect;

	// Layout state
	float LeftPanelRatio = 0.25f;   // 25% of width
	float RightPanelRatio = 0.25f;  // 25% of width

	// Whether we've applied the initial ImGui window placement
	bool bInitialPlacementDone = false;

	// Request focus on first open
	bool bRequestFocus = false;

	// Window open state
	bool bIsOpen = true;
	FString WindowTitle;
	bool bHasBottomPanel = true;

	// 윈도우 hover/focus 상태 (매 프레임 업데이트됨)
	bool bIsWindowHovered = false;
	bool bIsWindowFocused = false;

	// 우클릭 카메라 조작 상태
	bool bRightMousePressed = false;

	void OpenNewTab(const char* Name = "Viewer");
	void CloseTab(int Index);
	
	virtual ViewerState* CreateViewerState(const char* Name, UEditorAssetPreviewContext* Context) = 0;
	virtual void DestroyViewerState(ViewerState*& State) = 0;
	virtual FString GetWindowTitle() const = 0;

	virtual void RenderLeftPanel(float PanelWidth);
	virtual void RenderRightPanel();
	virtual void RenderBottomPanel() {};

	void UpdateBoneTransformFromSkeleton(ViewerState* State);
	void ApplyBoneTransform(ViewerState* State);
	void ExpandToSelectedBone(ViewerState* State, int32 BoneIndex);

private:
	bool IsOpen() const { return bIsOpen; }
	void Close() { bIsOpen = false; }

	// TODO: check functions .. (not used by inherited class)
};