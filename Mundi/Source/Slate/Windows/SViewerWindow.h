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

	void OpenOrFocusTab(UEditorAssetPreviewContext* Context);
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

	// 뷰어 툴
	void RenderAnimationBrowser(
		std::function<void(UAnimSequence*)> OnAnimationSelected = nullptr,
		std::function<bool(UAnimSequence*)> IsAnimationSelected = nullptr);

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

	// 마우스 드래그 상태 추적
	bool bLeftMousePressed = false;   // 좌클릭 드래그 (기즈모 조작)
	bool bRightMousePressed = false;  // 우클릭 드래그 (카메라 조작)

	void RenderTabBar();
	void RenderTabsAndToolbar(EViewerType CurrentViewerType);
	void OpenNewTab(const char* Name = "Viewer");
	void CloseTab(int Index);
	
	virtual ViewerState* CreateViewerState(const char* Name, UEditorAssetPreviewContext* Context) = 0;
	virtual void DestroyViewerState(ViewerState*& State) = 0;
	virtual FString GetWindowTitle() const = 0;

	virtual void RenderLeftPanel(float PanelWidth);
	virtual void RenderRightPanel();
	virtual void RenderBottomPanel() {};

	void UpdateBoneTransformFromSkeleton(ViewerState* State);
	void UpdateBoneTransformFromGizmo(ViewerState* State);
	void ApplyBoneTransform(ViewerState* State);
	void ExpandToSelectedBone(ViewerState* State, int32 BoneIndex);

	// 뷰어 툴바 관련 메서드
	void LoadViewerToolbarIcons(ID3D11Device* Device);
	void RenderViewerToolbar();
	void RenderViewerGizmoButtons();
	void RenderViewerGizmoSpaceButton();
	void RenderCameraOptionDropdownMenu();
	void RenderViewModeDropdownMenu();
	class AGizmoActor* GetGizmoActor();

private:
	bool IsOpen() const { return bIsOpen; }
	void Close() { bIsOpen = false; }

	// TODO: check functions .. (not used by inherited class)

	// 뷰어 툴바 아이콘 텍스처
	class UTexture* IconSelect = nullptr;
	class UTexture* IconMove = nullptr;
	class UTexture* IconRotate = nullptr;
	class UTexture* IconScale = nullptr;
	class UTexture* IconWorldSpace = nullptr;
	class UTexture* IconLocalSpace = nullptr;

	// 카메라 모드 아이콘
	class UTexture* IconCamera = nullptr;
	class UTexture* IconPerspective = nullptr;

	// 직교 모드 아이콘
	class UTexture* IconTop = nullptr;
	class UTexture* IconBottom = nullptr;
	class UTexture* IconLeft = nullptr;
	class UTexture* IconRight = nullptr;
	class UTexture* IconFront = nullptr;
	class UTexture* IconBack = nullptr;

	// 카메라 설정 아이콘
	class UTexture* IconSpeed = nullptr;
	class UTexture* IconFOV = nullptr;
	class UTexture* IconNearClip = nullptr;
	class UTexture* IconFarClip = nullptr;

	// 뷰모드 아이콘
	class UTexture* IconViewMode_Lit = nullptr;
	class UTexture* IconViewMode_Unlit = nullptr;
	class UTexture* IconViewMode_Wireframe = nullptr;
	class UTexture* IconViewMode_BufferVis = nullptr;
};