#pragma once
#include "SViewerWindow.h"
#include "Source/Runtime/Engine/Viewer/PhysicsAssetEditorState.h"
#include <functional>

class FViewport;
class FViewportClient;
class UWorld;
struct ID3D11Device;
class UPhysicsAsset;
class UBodySetup;
struct FConstraintSetup;

// Sub Widgets (UWidget 기반)
class USkeletonTreeWidget;
class UBodyPropertiesWidget;
class UConstraintPropertiesWidget;
class UToolsWidget;

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

	// 마우스 입력 오버라이드 (베이스 클래스의 bone picking 제거)
	virtual void OnMouseDown(FVector2D MousePos, uint32 Button) override;
	virtual void OnMouseUp(FVector2D MousePos, uint32 Button) override;

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

	// ────────────────────────────────────────────────
	// Shape 라인 관리 (캐시 기반)
	// ────────────────────────────────────────────────
	void RebuildShapeLines();           // 라인 재구성 (바디 추가/제거 시)
	void UpdateSelectedBodyLines();     // 선택된 바디의 라인 좌표만 업데이트 (속성 변경 시)
	void UpdateSelectionColors();       // 선택 색상만 업데이트

	// ────────────────────────────────────────────────
	// 기즈모 연동
	// ────────────────────────────────────────────────
	void RepositionAnchorToBody(int32 BodyIndex);   // 기즈모 앵커를 바디 위치로 이동
	void UpdateBodyTransformFromGizmo();            // 기즈모에서 바디 LocalTransform 업데이트

	// ────────────────────────────────────────────────
	// 파일 작업
	// ────────────────────────────────────────────────
	void SavePhysicsAsset();
	void SavePhysicsAssetAs();
	void LoadPhysicsAsset();

	// 미저장 변경사항 확인 후 액션 실행 (비동기)
	void CheckUnsavedChangesAndExecute(std::function<void()> Action);
	std::function<void()> PendingAction;  // 다이얼로그 후 실행할 액션

public:
	// ────────────────────────────────────────────────
	// 바디/제약 조건 작업
	// ────────────────────────────────────────────────
	void AutoGenerateBodies();
	void AddBodyToBone(int32 BoneIndex);
	void RemoveSelectedBody();
	void AddConstraintBetweenBodies(int32 ParentBodyIndex, int32 ChildBodyIndex);
	void RemoveSelectedConstraint();
	void RegenerateSelectedBody();
	void AddPrimitiveToBody(int32 BodyIndex, int32 PrimitiveType);  // 0=Box, 1=Sphere, 2=Capsule

private:
	// ────────────────────────────────────────────────
	// 레이아웃
	// ────────────────────────────────────────────────
	float LeftPanelWidth = 250.f;    // 좌측 트리 패널 너비
	float RightPanelWidth = 300.f;   // 우측 속성 패널 너비

	// ────────────────────────────────────────────────
	// Sub Widget 인스턴스 (UWidget 기반)
	// ────────────────────────────────────────────────
	USkeletonTreeWidget* SkeletonTreeWidget = nullptr;
	UBodyPropertiesWidget* BodyPropertiesWidget = nullptr;
	UConstraintPropertiesWidget* ConstraintPropertiesWidget = nullptr;
	UToolsWidget* ToolsWidget = nullptr;

	void CreateSubWidgets();
	void DestroySubWidgets();
	void UpdateSubWidgetEditorState();

	// ────────────────────────────────────────────────
	// 헬퍼 함수
	// ────────────────────────────────────────────────
	PhysicsAssetEditorState* GetActivePhysicsState() const
	{
		return static_cast<PhysicsAssetEditorState*>(ActiveState);
	}

	// 메시 로드 및 Physics Asset 초기화
	void LoadMeshAndResetPhysics(PhysicsAssetEditorState* State, const FString& MeshPath);

	// 파일 경로에서 파일명만 추출
	static FString ExtractFileName(const FString& Path);
};
