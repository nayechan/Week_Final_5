#pragma once
#include "SViewerWindow.h"
#include "Source/Runtime/Engine/Viewer/PhysicsAssetEditorState.h"

class FViewport;
class FViewportClient;
class UWorld;
struct ID3D11Device;
class UPhysicsAsset;
struct FBodySetup;
struct FConstraintSetup;

// Sub Widgets
class SkeletonTreeWidget;
class BodyPropertiesWidget;
class ConstraintPropertiesWidget;

/**
 * SPhysicsAssetEditorWindow
 *
 * Physics Asset 편집을 위한 에디터 윈도우
 * SViewerWindow를 상속하여 기본 뷰어 기능 제공
 *
 * UI 레이아웃:
 * - 좌측: 본/바디 계층 구조 트리
 * - 중앙: 3D 뷰포트
 * - 우측: 속성 편집 패널
 * - 하단: 제약 조건 그래프 (옵션)
 */
class SPhysicsAssetEditorWindow : public SViewerWindow
{
public:
	SPhysicsAssetEditorWindow();
	virtual ~SPhysicsAssetEditorWindow();

	virtual void OnRender() override;
	virtual void OnUpdate(float DeltaSeconds) override;
	virtual void PreRenderViewportUpdate() override;
	virtual void OnSave() override;

	// 파일 경로 기반 탭 검색 오버라이드
	void OpenOrFocusTab(UEditorAssetPreviewContext* Context) override;

protected:
	virtual ViewerState* CreateViewerState(const char* Name, UEditorAssetPreviewContext* Context) override;
	virtual void DestroyViewerState(ViewerState*& State) override;
	virtual FString GetWindowTitle() const override { return "Physics Asset Editor"; }
	virtual void RenderTabsAndToolbar(EViewerType CurrentViewerType) override;

	// 패널 렌더링
	virtual void RenderLeftPanel(float PanelWidth) override;   // 본/바디 트리
	virtual void RenderRightPanel() override;                  // 속성 패널
	virtual void RenderBottomPanel() override;                 // 제약 조건 그래프

	// 스켈레탈 메시 로드 후 콜백
	virtual void OnSkeletalMeshLoaded(ViewerState* State, const FString& Path) override;

private:
	// ────────────────────────────────────────────────
	// 툴바
	// ────────────────────────────────────────────────
	void RenderToolbar();
	void LoadToolbarIcons();

	// ────────────────────────────────────────────────
	// 하단 패널 (제약 조건 그래프)
	// ────────────────────────────────────────────────
	void RenderConstraintGraph();

	// ────────────────────────────────────────────────
	// 뷰포트
	// ────────────────────────────────────────────────
	void RenderViewportArea(float width, float height);
	void UpdateShapePreview();

	// ────────────────────────────────────────────────
	// 파일 작업
	// ────────────────────────────────────────────────
	void SavePhysicsAsset();
	void SavePhysicsAssetAs();
	void LoadPhysicsAsset();

	// ────────────────────────────────────────────────
	// 바디/제약 조건 작업
	// ────────────────────────────────────────────────
	void AddBodyToBone(int32 BoneIndex);
	void RemoveSelectedBody();
	void AddConstraintBetweenBodies(int32 ParentBodyIndex, int32 ChildBodyIndex);
	void RemoveSelectedConstraint();

	// ────────────────────────────────────────────────
	// 레이아웃
	// ────────────────────────────────────────────────
	float LeftPanelWidth = 250.f;    // 좌측 트리 패널 너비
	float RightPanelWidth = 300.f;   // 우측 속성 패널 너비
	float BottomPanelHeight = 200.f; // 하단 그래프 패널 높이
	bool bShowBottomPanel = false;   // 하단 패널 표시 여부

	// ────────────────────────────────────────────────
	// 툴바 아이콘
	// ────────────────────────────────────────────────
	UTexture* IconSave = nullptr;
	UTexture* IconSaveAs = nullptr;
	UTexture* IconLoad = nullptr;
	UTexture* IconAddBody = nullptr;
	UTexture* IconRemoveBody = nullptr;
	UTexture* IconAddConstraint = nullptr;
	UTexture* IconRemoveConstraint = nullptr;
	UTexture* IconSimulate = nullptr;
	UTexture* IconShowBodies = nullptr;
	UTexture* IconShowConstraints = nullptr;

	// ────────────────────────────────────────────────
	// 헬퍼 함수
	// ────────────────────────────────────────────────
	PhysicsAssetEditorState* GetActivePhysicsState() const
	{
		return static_cast<PhysicsAssetEditorState*>(ActiveState);
	}
};
