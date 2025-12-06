#pragma once
#include "SViewerWindow.h"
#include "Source/Runtime/Engine/Viewer/ViewerState.h"

class FViewport;
class FViewportClient;
class UWorld;
struct ID3D11Device;
class UTexture;
class UPhysicsAsset;
class UBodySetup;
struct FConstraintInstance;

class SPhysicsAssetEditorWindow : public SViewerWindow
{
public:
    // 다른 위젯에서 Physics Asset Editor 포커스 상태 확인용
    static bool bIsAnyPhysicsAssetEditorFocused;

    SPhysicsAssetEditorWindow();
    virtual ~SPhysicsAssetEditorWindow();

    virtual void OnRender() override;
    virtual void OnUpdate(float DeltaSeconds) override;
    virtual void PreRenderViewportUpdate() override;
    virtual void OnSave() override;
    virtual void OnMouseDown(FVector2D MousePos, uint32 Button) override;
    virtual void OnMouseUp(FVector2D MousePos, uint32 Button) override;

    // Save/Load 함수
    void SavePhysicsAsset();
    void SavePhysicsAssetAs();
    void LoadPhysicsAsset();

protected:
    virtual ViewerState* CreateViewerState(const char* Name, UEditorAssetPreviewContext* Context) override;
    virtual void DestroyViewerState(ViewerState*& State) override;
    virtual FString GetWindowTitle() const override { return "Physics Asset Editor"; }
    virtual void OnSkeletalMeshLoaded(ViewerState* State, const FString& Path) override;
    virtual void RenderTabsAndToolbar(EViewerType CurrentViewerType) override;

    virtual void RenderLeftPanel(float PanelWidth) override;
    virtual void RenderRightPanel() override;
    virtual void RenderBottomPanel() override;

private:
    // Helper to get typed state
    PhysicsAssetEditorState* GetPhysicsState() const;

    // Left Panel - Skeleton Tree (상단)
    void RenderSkeletonTreePanel(float Width, float Height);
    void RenderBoneTreeNode(int32 BoneIndex, int32 Depth = 0);
    void RenderBodyTreeNode(int32 BodyIndex, const FSkeleton* Skeleton);  // Hide Bones 모드용

    // Left Panel - Graph View (하단)
    void RenderGraphViewPanel(float Width, float Height);
    void RenderGraphNode(const FVector2D& Position, const FString& Label, bool bIsBody, bool bSelected, uint32 Color);

    // Right Panel - Details
    void RenderBodyDetails(UBodySetup* Body);
    void RenderConstraintDetails(FConstraintInstance* Constraint);

    // Viewport overlay
    void RenderViewportOverlay();

    // Physics body wireframe rendering
    void RenderPhysicsBodies();
    void RenderConstraintVisuals();

    // Selection helpers
    void SelectBody(int32 Index, PhysicsAssetEditorState::ESelectionSource Source);
    void SelectConstraint(int32 Index, PhysicsAssetEditorState::ESelectionSource Source);
    void SelectBone(int32 BoneIndex);  // 본 선택 시 기즈모 표시
    void ClearSelection();

    // Display Options
    void RenderDisplayOptions(float PanelWidth);

    // Body 생성/삭제
    void AddBodyToSelectedBone();
    void AddBodyToBone(int32 BoneIndex);
    void RemoveBody(int32 BodyIndex);
    UBodySetup* CreateDefaultBodySetup(const FString& BoneName);

    // Shape 삭제
    void DeleteSelectedShape();

    // Shape 개수 헬퍼
    int32 GetShapeCountByType(UBodySetup* Body, EShapeType ShapeType);

    // Shape 기즈모 업데이트
    void UpdateShapeGizmo();

    // Constraint 기즈모 업데이트
    void UpdateConstraintGizmo();

    // Context menu
    void RenderBoneContextMenu(int32 BoneIndex, bool bHasBody, int32 BodyIndex);

    // Left panel splitter ratio (상단/하단 분할)
    float LeftPanelSplitRatio = 0.6f;  // 60% tree, 40% graph

    // 툴바 아이콘
    UTexture* IconSave = nullptr;
    UTexture* IconSaveAs = nullptr;
    UTexture* IconLoad = nullptr;

    // Context menu state
    int32 ContextMenuBoneIndex = -1;
    int32 ContextMenuConstraintIndex = -1;

    // Skeleton Tree Settings
    void RenderSkeletonTreeSettings();

    // 본 표시 모드 (하나만 선택 가능)
    enum class EBoneDisplayMode
    {
        AllBones,       // 모든 본 표시
        MeshBones,      // 메시 본 표시
        HideBones       // 본 숨김 (Body만 표시)
    };

    // 트리 표시 옵션
    struct FTreeDisplaySettings
    {
        EBoneDisplayMode BoneDisplayMode = EBoneDisplayMode::AllBones;
        bool bShowConstraintsInTree = true;  // 트리에 컨스트레인트 표시
    };
    FTreeDisplaySettings TreeSettings;

    // Shape 추가 (기존 Body에 추가하거나 새 Body 생성)
    void AddShapeToBone(int32 BoneIndex, EShapeType ShapeType);
    void AddShapeToBody(UBodySetup* Body, EShapeType ShapeType);

    // Shape 타입별 Body 생성 (deprecated - use AddShapeToBone)
    void AddBodyToBoneWithShape(int32 BoneIndex, EShapeType ShapeType);
    UBodySetup* CreateBodySetupWithShape(const FString& BoneName, EShapeType ShapeType);

    // Constraint 생성/삭제
    void AddConstraintBetweenBodies(int32 BodyIndex1, int32 BodyIndex2);
    void AddConstraintToParentBody(int32 ChildBodyIndex);
    void RemoveConstraint(int32 ConstraintIndex);
    FConstraintInstance CreateDefaultConstraint(const FName& ChildBone, const FName& ParentBone);

    // Constraint 트리 표시
    void RenderConstraintTreeNode(int32 ConstraintIndex, const FName& CurrentBoneName);
    void RenderConstraintContextMenu(int32 ConstraintIndex);

    // === Tools 패널 (언리얼 피직스 에셋 에디터 스타일) ===
    void RenderToolsPanel();
    void RenderGenerateConfirmPopup();

    // 모든 바디 자동 생성
    void GenerateAllBodies();
    void DoGenerateAllBodies();

    // Helper 함수들 (바디 자동 생성용)
    void GenerateConstraintsForBodies();
    void AdjustShapeForBoneType(UBodySetup* Body, const FString& BoneName, float BoneLength);
    float CalculateBoneLengthForGenerate(const FSkeleton& Skeleton, int32 BoneIndex, FVector& OutBoneDir);

    // Tools 패널 상태
    EShapeType SelectedPrimitiveType = EShapeType::Capsule;
    bool bShowGenerateConfirmPopup = false;

    // Body 생성 파라미터 (UI에서 조절 가능)
    float GenerateMinBoneSize = 0.03f;  // 이 크기 미만의 본은 스킵
    float GenerateMaxBoneSize = 0.50f;  // 본 크기 상한 클램프

    // ===== 에디터 시뮬레이션 =====
    bool bSimulateInEditor = false;  // 시뮬레이션 활성화 여부

    // 에디터 월드의 SkeletalMeshComponent들에 PhysicsAsset 새로고침
    void RefreshPhysicsAssetInWorld(UPhysicsAsset* Asset);

    // FBX 메시 선택 및 로드
    void LoadMeshForPhysicsAsset(const FString& MeshPath);
    static FString ExtractFileName(const FString& Path);
};
