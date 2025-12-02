#pragma once
#include "SViewerWindow.h"
#include "Source/Runtime/Engine/Viewer/ViewerState.h"

class FViewport;
class FViewportClient;
class UWorld;
class UPhysicsAsset;
class USkeletalBodySetup;
class UPhysicsConstraintTemplate;
class ULineComponent;
struct ID3D11Device;

class SPhysicsAssetEditorWindow : public SViewerWindow
{
public:
	SPhysicsAssetEditorWindow();
	virtual ~SPhysicsAssetEditorWindow();

	virtual void OnRender() override;
	virtual void OnUpdate(float DeltaSeconds) override;

	// 파일 경로 기반 탭 검색 오버라이드
	void OpenOrFocusTab(UEditorAssetPreviewContext* Context) override;

protected:
	virtual ViewerState* CreateViewerState(const char* Name, UEditorAssetPreviewContext* Context) override;
	virtual void DestroyViewerState(ViewerState*& State) override;
	virtual FString GetWindowTitle() const override { return "Physics Asset Editor"; }
	virtual void RenderTabsAndToolbar(EViewerType CurrentViewerType) override;

	// 패널 렌더링
	virtual void RenderLeftPanel(float PanelWidth) override;
	virtual void RenderRightPanel() override;

private:
	// 툴바
	void RenderToolbar();

	// 6패널 렌더링 함수
	void RenderLeftHierarchyArea(float totalHeight);   // 좌측 상단: Hierarchy (본 트리)
	void RenderLeftGraphArea();                        // 좌측 하단: Graph (노드 그래프)
	void RenderRightDetailsArea(float totalHeight);    // 우측 상단: Details (속성 편집)
	void RenderRightToolArea();                        // 우측 하단: Tool (생성 도구)

	// 세부 패널 렌더링
	void RenderHierarchyPanel();
	void RenderGraphPanel();
	void RenderDetailsPanel();
	void RenderToolPanel();

	// Details 패널 헬퍼
	void RenderBoneDetails(int32 BoneIndex);
	void RenderBodyDetails(USkeletalBodySetup* Body, int32 BodyIndex);
	bool RenderShapeDetails(USkeletalBodySetup* Body);  // 변경 여부 반환
	void AddBodyToBone(int32 BoneIndex, int32 ShapeType);
	void RemoveBody(int32 BodyIndex);

	// 뷰포트
	void RenderViewportArea(float width, float height);

	// 시뮬레이션 제어
	void StartSimulation();
	void StopSimulation();
	void ResetPose();

	// 바디/조인트 생성
	void CreateAllBodies(int32 ShapeType);  // 0: Sphere, 1: Box, 2: Capsule
	void RemoveAllBodies();
	void AutoCreateConstraints();

	// 시각화 라인 재구성
	void RebuildBodyShapeLines();
	void RebuildConstraintLines();

	// 파일 작업
	void SavePhysicsAsset();
	void SavePhysicsAssetAs();
	void LoadPhysicsAsset();

	// 레이아웃 비율 및 크기
	float LeftHierarchyHeight = 350.f;   // 좌측 상단 Hierarchy 높이
	float RightDetailsHeight = 350.f;    // 우측 상단 Details 높이

	// 툴바 아이콘
	UTexture* IconSave = nullptr;
	UTexture* IconSaveAs = nullptr;
	UTexture* IconLoad = nullptr;
	UTexture* IconPlay = nullptr;
	UTexture* IconStop = nullptr;
	UTexture* IconReset = nullptr;

	// Hierarchy 패널 아이콘
	UTexture* IconBone = nullptr;
	UTexture* IconBody = nullptr;

	// Hierarchy 검색
	char BoneSearchBuffer[256] = {0};

	// 툴바 상태
	void LoadToolbarIcons();
	void LoadHierarchyIcons();

	// Shape 타입 선택 (Tool 패널용)
	int32 SelectedShapeType = 0;  // 0: Sphere, 1: Box, 2: Capsule

	// 탭 선택 요청 (새 탭 열기/포커스 시에만 사용, -1이면 무시)
	int32 PendingSelectTabIndex = -1;

	// 헬퍼 함수
	PhysicsAssetEditorState* GetActivePhysicsState() const
	{
		return static_cast<PhysicsAssetEditorState*>(ActiveState);
	}
};
