#include "pch.h"
#include "SPhysicsAssetEditorWindow.h"
#include "SlateManager.h"
#include "Source/Runtime/Engine/Viewer/EditorAssetPreviewContext.h"
#include "Source/Runtime/Engine/Viewer/PhysicsAssetEditorBootstrap.h"
#include "PlatformProcess.h"
#include "PhysicsAsset.h"
#include "PhysicsAssetUtils.h"
#include "SkeletalBodySetup.h"
#include "PhysicsConstraintTemplate.h"
#include "PhysicsScene.h"
#include "PhysicsSystem.h"
#include "BodyInstance.h"
#include "ConstraintInstance.h"
#include "FViewport.h"
#include "FViewportClient.h"
#include "LineComponent.h"
#include "SkeletalMeshComponent.h"
#include "SkeletalMeshActor.h"
#include "PrimitiveDrawInterface.h"
#include "SphereElem.h"
#include "BoxElem.h"
#include "SphylElem.h"
#include "SkeletalMesh.h"
#include "ResourceManager.h"
#include "Texture.h"
#include "Widgets/PropertyRenderer.h"

#include <PxPhysicsAPI.h>
using namespace physx;

// 파일 경로에서 파일명 추출 헬퍼 함수
static FString ExtractFileNameFromPath(const FString& FilePath)
{
	size_t lastSlash = FilePath.find_last_of("/\\");
	FString filename = (lastSlash != FString::npos) ? FilePath.substr(lastSlash + 1) : FilePath;
	size_t dotPos = filename.find_last_of('.');
	if (dotPos != FString::npos)
		filename = filename.substr(0, dotPos);
	return filename;
}

SPhysicsAssetEditorWindow::SPhysicsAssetEditorWindow()
{
	CenterRect = FRect(0, 0, 0, 0);
	LeftPanelRatio = 0.20f;   // 좌측 Hierarchy + Graph
	RightPanelRatio = 0.25f;  // 우측 Details + Tool
	bHasBottomPanel = false;
}

SPhysicsAssetEditorWindow::~SPhysicsAssetEditorWindow()
{
	// 툴바 아이콘은 ResourceManager가 관리하므로 포인터만 초기화
	// (DeleteObject 호출 시 캐시된 리소스가 삭제되어 재열람 시 문제 발생)
	IconSave = nullptr;
	IconSaveAs = nullptr;
	IconLoad = nullptr;
	IconPlay = nullptr;
	IconStop = nullptr;
	IconReset = nullptr;

	// 탭 정리
	for (int i = 0; i < Tabs.Num(); ++i)
	{
		ViewerState* State = Tabs[i];
		PhysicsAssetEditorBootstrap::DestroyViewerState(State);
	}
	Tabs.Empty();
	ActiveState = nullptr;
}

void SPhysicsAssetEditorWindow::LoadToolbarIcons()
{
	if (!Device) return;

	IconSave = UResourceManager::GetInstance().Load<UTexture>(GDataDir + "/Icon/Toolbar_Save.png");
	IconSaveAs = UResourceManager::GetInstance().Load<UTexture>(GDataDir + "/Icon/Toolbar_SaveAs.png");
	IconLoad = UResourceManager::GetInstance().Load<UTexture>(GDataDir + "/Icon/Toolbar_Load.png");
	IconPlay = UResourceManager::GetInstance().Load<UTexture>(GDataDir + "/Icon/Toolbar_Play.png");
	IconStop = UResourceManager::GetInstance().Load<UTexture>(GDataDir + "/Icon/Toolbar_Stop.png");
	IconReset = UResourceManager::GetInstance().Load<UTexture>(GDataDir + "/Icon/Toolbar_RePlay.png");
}

void SPhysicsAssetEditorWindow::LoadHierarchyIcons()
{
	if (!Device) return;

	IconBone = UResourceManager::GetInstance().Load<UTexture>(GDataDir + "/Icon/Bone_Hierarchy.png");
	IconBody = UResourceManager::GetInstance().Load<UTexture>(GDataDir + "/Icon/Body_Hierarchy.png");
}

ViewerState* SPhysicsAssetEditorWindow::CreateViewerState(const char* Name, UEditorAssetPreviewContext* Context)
{
	return PhysicsAssetEditorBootstrap::CreateViewerState(Name, World, Device, Context);
}

void SPhysicsAssetEditorWindow::DestroyViewerState(ViewerState*& State)
{
	PhysicsAssetEditorBootstrap::DestroyViewerState(State);
}

void SPhysicsAssetEditorWindow::OpenOrFocusTab(UEditorAssetPreviewContext* Context)
{
	if (!Context) return;

	// 열려는 PhysicsAsset 경로 결정
	FString TargetPath;
	if (!Context->PhysicsAssetPath.empty())
	{
		// Case 2: PropertyRenderer에서 기존 PhysicsAsset 편집
		TargetPath = Context->PhysicsAssetPath;
	}
	else if (!Context->AssetPath.empty())
	{
		// Case 3: ContentBrowser에서 더블클릭
		TargetPath = Context->AssetPath;
	}
	// Case 1: 새로 생성 (TargetPath 비어있음)

	// 이미 열린 탭이 있는지 확인 (경로가 있는 경우만)
	if (!TargetPath.empty())
	{
		for (int i = 0; i < Tabs.Num(); ++i)
		{
			PhysicsAssetEditorState* State = static_cast<PhysicsAssetEditorState*>(Tabs[i]);
			if (State->CurrentFilePath == TargetPath)
			{
				// 기존 탭으로 포커스
				ActiveTabIndex = i;
				ActiveState = Tabs[i];
				PendingSelectTabIndex = i;
				return;
			}
		}
	}

	// 새 탭 생성
	FString TabName = TargetPath.empty() ? "Untitled" : ExtractFileNameFromPath(TargetPath);
	ViewerState* NewState = CreateViewerState(TabName.c_str(), Context);
	Tabs.Add(NewState);
	ActiveTabIndex = Tabs.Num() - 1;
	ActiveState = NewState;
	PendingSelectTabIndex = ActiveTabIndex;
}

void SPhysicsAssetEditorWindow::OnUpdate(float DeltaSeconds)
{
	SViewerWindow::OnUpdate(DeltaSeconds);

	PhysicsAssetEditorState* State = GetActivePhysicsState();
	if (!State) return;

	// 선택 바디 변경 감지 (선택 라인만 업데이트, 비선택 라인은 모든 항목 포함하므로 재생성 불필요)
	if (State->SelectedBodyIndex != State->LastSelectedBodyIndex)
	{
		State->bSelectedBodyLineDirty = true;  // 선택 바디 하이라이트만 업데이트
		State->LastSelectedBodyIndex = State->SelectedBodyIndex;
	}

	// 선택 컨스트레인트 변경 감지 (선택 라인만 업데이트)
	if (State->SelectedConstraintIndex != State->LastSelectedConstraintIndex)
	{
		State->bSelectedConstraintLineDirty = true;  // 선택 컨스트레인트 하이라이트만 업데이트
		State->LastSelectedConstraintIndex = State->SelectedConstraintIndex;
	}

	// 본 라인 재구성
	if (State->bShowBones && State->PreviewActor && State->bBoneLinesDirty)
	{
		USkeletalMeshComponent* MeshComp = State->PreviewActor->GetSkeletalMeshComponent();
		if (MeshComp && MeshComp->GetSkeletalMesh())
		{
			if (ULineComponent* LineComp = State->PreviewActor->GetBoneLineComponent())
			{
				LineComp->SetLineVisible(true);
			}
			State->PreviewActor->RebuildBoneLines(State->SelectedBoneIndex);
			State->bBoneLinesDirty = false;
		}
	}

	// Shape 라인 재생성 (바디 와이어프레임)
	if (State->bShowBodies)
	{
		// 1. BoneTM 캐시 갱신 (바디 추가/삭제/메시 변경 시에만)
		if (State->bBoneTMCacheDirty)
		{
			RebuildBoneTMCache();
		}

		// 2. 비선택 바디 라인 갱신 (바디 추가/삭제/선택 변경 시)
		if (State->bAllBodyLinesDirty)
		{
			RebuildUnselectedBodyLines();
		}

		// 3. 선택 바디 라인 갱신 (선택 변경/Shape 편집 시)
		if (State->bSelectedBodyLineDirty)
		{
			RebuildSelectedBodyLines();
		}
	}

	// Constraint 라인 재생성
	if (State->bShowConstraints)
	{
		// 1. 비선택 컨스트레인트 라인 갱신 (컨스트레인트 추가/삭제/선택 변경 시)
		if (State->bAllConstraintLinesDirty)
		{
			RebuildUnselectedConstraintLines();
		}

		// 2. 선택 컨스트레인트 라인 갱신 (선택 변경/속성 편집 시)
		if (State->bSelectedConstraintLineDirty)
		{
			RebuildSelectedConstraintLines();
		}
	}

	// 시뮬레이션 업데이트
	if (State->bIsSimulating)
	{
		TickSimulation(DeltaSeconds);
	}
}

void SPhysicsAssetEditorWindow::OnRender()
{
	// 윈도우가 닫혔으면 정리 요청
	if (!bIsOpen)
	{
		USlateManager::GetInstance().RequestCloseDetachedWindow(this);
		return;
	}

	// 윈도우 플래그
	ImGuiWindowFlags flags = ImGuiWindowFlags_NoSavedSettings;

	// 초기 배치 (첫 프레임)
	if (!bInitialPlacementDone)
	{
		ImGui::SetNextWindowPos(ImVec2(Rect.Left, Rect.Top));
		ImGui::SetNextWindowSize(ImVec2(Rect.GetWidth(), Rect.GetHeight()));
		bInitialPlacementDone = true;
	}

	// 포커스 요청
	if (bRequestFocus)
	{
		ImGui::SetNextWindowFocus();
		bRequestFocus = false;
	}

	// 윈도우 타이틀 생성
	char UniqueTitle[256];
	FString Title = GetWindowTitle();
	sprintf_s(UniqueTitle, sizeof(UniqueTitle), "%s###%p", Title.c_str(), this);

	// 첫 프레임에 아이콘 로드
	if (!IconSave && Device)
	{
		LoadToolbarIcons();
		LoadHierarchyIcons();
	}

	if (ImGui::Begin(UniqueTitle, &bIsOpen, flags))
	{
		// hover/focus 상태 캡처
		bIsWindowHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows);
		bIsWindowFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);

		// 경고 팝업 처리
		PhysicsAssetEditorState* WarningState = GetActivePhysicsState();
		if (WarningState && !WarningState->PendingWarningMessage.empty())
		{
			ImGui::OpenPopup("Warning##PhysicsAssetEditor");
		}

		if (ImGui::BeginPopupModal("Warning##PhysicsAssetEditor", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
		{
			if (WarningState)
			{
				ImGui::Text("%s", WarningState->PendingWarningMessage.c_str());
				ImGui::Separator();
				if (ImGui::Button("OK", ImVec2(120, 0)))
				{
					WarningState->PendingWarningMessage.clear();
					ImGui::CloseCurrentPopup();
				}
			}
			ImGui::EndPopup();
		}

		// 탭바 및 툴바 렌더링
		RenderTabsAndToolbar(EViewerType::PhysicsAsset);

		// 마지막 탭을 닫은 경우
		if (!bIsOpen)
		{
			USlateManager::GetInstance().RequestCloseDetachedWindow(this);
			ImGui::End();
			return;
		}

		// 윈도우 rect 업데이트
		ImVec2 pos = ImGui::GetWindowPos();
		ImVec2 size = ImGui::GetWindowSize();
		Rect.Left = pos.x;
		Rect.Top = pos.y;
		Rect.Right = pos.x + size.x;
		Rect.Bottom = pos.y + size.y;
		Rect.UpdateMinMax();

		// 툴바 렌더링
		RenderToolbar();

		ImVec2 contentAvail = ImGui::GetContentRegionAvail();
		float totalWidth = contentAvail.x;
		float totalHeight = contentAvail.y;

		// 좌우 패널 너비 계산
		float leftWidth = totalWidth * LeftPanelRatio;
		leftWidth = ImClamp(leftWidth, 150.f, totalWidth - 400.f);

		float rightWidth = totalWidth * RightPanelRatio;
		rightWidth = ImClamp(rightWidth, 200.f, totalWidth - 400.f);

		float centerWidth = totalWidth - leftWidth - rightWidth - 12.f;  // 스플리터 여유

		// 좌측 열 Render (Hierarchy + Graph)
		ImGui::BeginChild("LeftColumn", ImVec2(leftWidth, 0), false);
		{
			RenderLeftHierarchyArea(totalHeight);
			RenderLeftGraphArea();
		}
		ImGui::EndChild();

		// 수직 스플리터 (좌-중앙)
		ImGui::SameLine();
		ImGui::Button("##VSplitter1", ImVec2(4.f, -1));
		if (ImGui::IsItemActive())
		{
			float delta = ImGui::GetIO().MouseDelta.x;
			LeftPanelRatio += delta / totalWidth;
			LeftPanelRatio = ImClamp(LeftPanelRatio, 0.10f, 0.40f);
		}
		if (ImGui::IsItemHovered())
			ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);

		ImGui::SameLine();

		// 중앙 뷰포트
		ImGui::BeginChild("CenterViewport", ImVec2(centerWidth, 0), false);
		{
			RenderViewportArea(centerWidth, totalHeight);
		}
		ImGui::EndChild();

		// 수직 스플리터 (중앙-우측)
		ImGui::SameLine();
		ImGui::Button("##VSplitter2", ImVec2(4.f, -1));
		if (ImGui::IsItemActive())
		{
			float delta = ImGui::GetIO().MouseDelta.x;
			RightPanelRatio -= delta / totalWidth;
			RightPanelRatio = ImClamp(RightPanelRatio, 0.15f, 0.40f);
		}
		if (ImGui::IsItemHovered())
			ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);

		ImGui::SameLine();

		// 우측 열 Render (Details + Tool)
		ImGui::BeginChild("RightColumn", ImVec2(0, 0), false);
		{
			RenderRightDetailsArea(totalHeight);
			RenderRightToolArea();
		}
		ImGui::EndChild();
	}
	ImGui::End();
}

void SPhysicsAssetEditorWindow::RenderTabsAndToolbar(EViewerType CurrentViewerType)
{
	// 탭바만 렌더링
	if (!ImGui::BeginTabBar("PhysicsAssetEditorTabs",
		ImGuiTabBarFlags_AutoSelectNewTabs | ImGuiTabBarFlags_Reorderable))
		return;

	// 탭 렌더링
	for (int i = 0; i < Tabs.Num(); ++i)
	{
		ViewerState* State = Tabs[i];
		PhysicsAssetEditorState* PState = static_cast<PhysicsAssetEditorState*>(State);
		bool open = true;

		// 동적 탭 이름 생성
		FString TabDisplayName;
		if (PState->CurrentFilePath.empty())
		{
			TabDisplayName = "Untitled";
		}
		else
		{
			TabDisplayName = ExtractFileNameFromPath(PState->CurrentFilePath);
		}

		// 더티 표시
		if (PState->bIsDirty)
		{
			TabDisplayName += "*";
		}

		char TabId[128];
		sprintf_s(TabId, sizeof(TabId), "%s###Tab%d", TabDisplayName.c_str(), i);

		// 탭 선택 요청이 있을 때만 SetSelected 플래그 사용 (그 외에는 사용자 클릭 따름)
		ImGuiTabItemFlags tabFlags = 0;
		if (i == PendingSelectTabIndex)
		{
			tabFlags |= ImGuiTabItemFlags_SetSelected;
			PendingSelectTabIndex = -1;  // 한 번만 적용
		}

		if (ImGui::BeginTabItem(TabId, &open, tabFlags))
		{
			// 현재 선택된 탭으로 ActiveState 업데이트
			if (ActiveTabIndex != i)
			{
				ActiveTabIndex = i;
				ActiveState = State;
			}
			ImGui::EndTabItem();
		}

		// 탭 닫기
		if (!open)
		{
			CloseTab(i);
			if (Tabs.Num() == 0)
			{
				bIsOpen = false;
			}
			break;
		}
	}

	// "+" 버튼 제거 - 새 에셋은 PropertyRenderer나 ContentBrowser에서만 열기

	ImGui::EndTabBar();
}

void SPhysicsAssetEditorWindow::RenderToolbar()
{
	PhysicsAssetEditorState* State = GetActivePhysicsState();

	const ImVec2 IconSize(20, 20);

	// ========== 파일 버튼들 ==========
	// Save 버튼
	if (IconSave && IconSave->GetShaderResourceView())
	{
		if (ImGui::ImageButton("##SavePhysAsset", (void*)IconSave->GetShaderResourceView(), IconSize))
			SavePhysicsAsset();
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Save");
	}
	else
	{
		if (ImGui::Button("Save"))
			SavePhysicsAsset();
	}
	ImGui::SameLine();

	// Save As 버튼
	if (IconSaveAs && IconSaveAs->GetShaderResourceView())
	{
		if (ImGui::ImageButton("##SaveAsPhysAsset", (void*)IconSaveAs->GetShaderResourceView(), IconSize))
			SavePhysicsAssetAs();
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Save As");
	}
	else
	{
		if (ImGui::Button("Save As"))
			SavePhysicsAssetAs();
	}
	ImGui::SameLine();

	// Load 버튼
	if (IconLoad && IconLoad->GetShaderResourceView())
	{
		if (ImGui::ImageButton("##LoadPhysAsset", (void*)IconLoad->GetShaderResourceView(), IconSize))
			LoadPhysicsAsset();
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Load");
	}
	else
	{
		if (ImGui::Button("Load"))
			LoadPhysicsAsset();
	}

	ImGui::SameLine();
	ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
	ImGui::SameLine();

	// ========== 시뮬레이션 버튼들 ==========
	if (State && State->bIsSimulating)
	{
		// Stop 버튼
		if (IconStop && IconStop->GetShaderResourceView())
		{
			if (ImGui::ImageButton("##StopSim", (void*)IconStop->GetShaderResourceView(), IconSize))
				StopSimulation();
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("Stop Simulation");
		}
		else
		{
			if (ImGui::Button("Stop"))
				StopSimulation();
		}
	}
	else
	{
		// Play 버튼
		if (IconPlay && IconPlay->GetShaderResourceView())
		{
			if (ImGui::ImageButton("##PlaySim", (void*)IconPlay->GetShaderResourceView(), IconSize))
				StartSimulation();
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("Start Simulation");
		}
		else
		{
			if (ImGui::Button("Play"))
				StartSimulation();
		}
	}
	ImGui::SameLine();

	// Reset Pose 버튼
	if (IconReset && IconReset->GetShaderResourceView())
	{
		if (ImGui::ImageButton("##ResetPose", (void*)IconReset->GetShaderResourceView(), IconSize))
			ResetPose();
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Reset Pose");
	}
	else
	{
		if (ImGui::Button("Reset Pose"))
			ResetPose();
	}

	ImGui::Separator();
}

void SPhysicsAssetEditorWindow::RenderLeftPanel(float PanelWidth)
{
	// 사용하지 않음 - 6패널 레이아웃 사용
}

void SPhysicsAssetEditorWindow::RenderRightPanel()
{
	// 사용하지 않음 - 6패널 레이아웃 사용
}

void SPhysicsAssetEditorWindow::RenderLeftHierarchyArea(float totalHeight)
{
	PhysicsAssetEditorState* State = GetActivePhysicsState();
	float hierarchyHeight = State ? totalHeight * State->LeftTopRatio : totalHeight * 0.6f;

	ImGui::BeginChild("HierarchyArea", ImVec2(0, hierarchyHeight), true);
	{
		ImGui::Text("스켈레톤 트리");
		ImGui::Separator();
		RenderHierarchyPanel();
	}
	ImGui::EndChild();

	// 수평 스플리터 (Hierarchy/Graph)
	ImGui::Button("##HSplitterLeft", ImVec2(-1, 4.f));
	if (ImGui::IsItemActive() && State)
	{
		float delta = ImGui::GetIO().MouseDelta.y;
		State->LeftTopRatio += delta / totalHeight;
		State->LeftTopRatio = ImClamp(State->LeftTopRatio, 0.2f, 0.8f);
	}
	if (ImGui::IsItemHovered())
		ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
}

void SPhysicsAssetEditorWindow::RenderLeftGraphArea()
{
	ImGui::BeginChild("GraphArea", ImVec2(0, 0), true);
	{
		ImGui::Text("그래프");
		ImGui::Separator();
		RenderGraphPanel();
	}
	ImGui::EndChild();
}

void SPhysicsAssetEditorWindow::RenderRightDetailsArea(float totalHeight)
{
	PhysicsAssetEditorState* State = GetActivePhysicsState();
	float detailsHeight = State ? totalHeight * State->RightTopRatio : totalHeight * 0.6f;

	ImGui::BeginChild("DetailsArea", ImVec2(0, detailsHeight), true);
	{
		ImGui::Text("디테일");
		ImGui::Separator();
		RenderDetailsPanel();
	}
	ImGui::EndChild();

	// 수평 스플리터 (Details/Tool)
	ImGui::Button("##HSplitterRight", ImVec2(-1, 4.f));
	if (ImGui::IsItemActive() && State)
	{
		float delta = ImGui::GetIO().MouseDelta.y;
		State->RightTopRatio += delta / totalHeight;
		State->RightTopRatio = ImClamp(State->RightTopRatio, 0.2f, 0.8f);
	}
	if (ImGui::IsItemHovered())
		ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
}

void SPhysicsAssetEditorWindow::RenderRightToolArea()
{
	ImGui::BeginChild("ToolArea", ImVec2(0, 0), true);
	{
		ImGui::Text("툴");
		ImGui::Separator();
		RenderToolPanel();
	}
	ImGui::EndChild();
}

void SPhysicsAssetEditorWindow::RenderViewportArea(float width, float height)
{
	PhysicsAssetEditorState* State = GetActivePhysicsState();
	if (!State || !State->Viewport) return;

	// 툴바와 뷰포트 사이 간격 제거
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));

	// 뷰어 툴바 렌더링 (뷰포트 상단)
	RenderViewerToolbar();

	// 툴바 아래 남은 공간 계산
	ImVec2 contentAvail = ImGui::GetContentRegionAvail();
	float viewportWidth = contentAvail.x;
	float viewportHeight = contentAvail.y;

	// 뷰포트 위치 저장 (CenterRect 먼저 설정)
	ImVec2 vpPos = ImGui::GetCursorScreenPos();
	CenterRect = FRect(vpPos.x, vpPos.y, vpPos.x + viewportWidth, vpPos.y + viewportHeight);
	CenterRect.UpdateMinMax();

	// 뷰포트 리사이즈
	int32 vpWidth = static_cast<int32>(viewportWidth);
	int32 vpHeight = static_cast<int32>(viewportHeight);

	if (vpWidth > 0 && vpHeight > 0)
	{
		State->Viewport->Resize(0, 0, vpWidth, vpHeight);
	}

	// 뷰포트 렌더링 (CenterRect 설정 후)
	OnRenderViewport();

	// 뷰포트 이미지 표시
	if (State->Viewport->GetSRV())
	{
		ImGui::Image(
			(ImTextureID)State->Viewport->GetSRV(),
			ImVec2(viewportWidth, viewportHeight)
		);
		// 뷰포트 hover 상태 업데이트 (마우스 입력 처리에 필요)
		State->Viewport->SetViewportHovered(ImGui::IsItemHovered());
	}
	else
	{
		State->Viewport->SetViewportHovered(false);
	}

	ImGui::PopStyleVar(); // ItemSpacing 복원
}

void SPhysicsAssetEditorWindow::RenderHierarchyPanel()
{
	PhysicsAssetEditorState* State = GetActivePhysicsState();
	if (!State) return;

	// 프리뷰 액터에서 스켈레탈 메시 가져오기
	USkeletalMesh* Mesh = nullptr;
	if (State->PreviewActor)
	{
		if (USkeletalMeshComponent* MeshComp = State->PreviewActor->GetSkeletalMeshComponent())
		{
			Mesh = MeshComp->GetSkeletalMesh();
		}
	}

	if (!Mesh)
	{
		ImGui::TextDisabled("No skeletal mesh loaded");
		return;
	}

	const FSkeleton* Skeleton = Mesh->GetSkeleton();
	if (!Skeleton || Skeleton->Bones.IsEmpty())
	{
		ImGui::TextDisabled("No skeleton data");
		return;
	}

	UPhysicsAsset* PhysAsset = State->EditingPhysicsAsset;

	// === 검색 바 ===
	ImGui::SetNextItemWidth(-1);
	ImGui::InputTextWithHint("##BoneSearch", "본 이름 검색...", BoneSearchBuffer, sizeof(BoneSearchBuffer));
	FString SearchFilter = BoneSearchBuffer;
	bool bHasFilter = !SearchFilter.empty();

	ImGui::Separator();

	// === 본 트리뷰 ===
	ImGui::BeginChild("BoneTreeView", ImVec2(0, 0), false);
	ImGui::PushStyleVar(ImGuiStyleVar_IndentSpacing, 16.0f);

	const TArray<FBone>& Bones = Skeleton->Bones;
	const ImVec2 IconSize(16, 16);

	// 자식 인덱스 맵 구성
	TArray<TArray<int32>> Children;
	Children.resize(Bones.size());
	for (int32 i = 0; i < Bones.size(); ++i)
	{
		int32 Parent = Bones[i].ParentIndex;
		if (Parent >= 0 && Parent < Bones.size())
		{
			Children[Parent].Add(i);
		}
	}

	// 검색 필터 매칭 확인 함수
	auto MatchesFilter = [&](int32 BoneIndex) -> bool
	{
		if (!bHasFilter) return true;
		FString BoneNameStr = Bones[BoneIndex].Name;
		FString FilterLower = SearchFilter;
		// 대소문자 무시 검색
		std::transform(BoneNameStr.begin(), BoneNameStr.end(), BoneNameStr.begin(), ::tolower);
		std::transform(FilterLower.begin(), FilterLower.end(), FilterLower.begin(), ::tolower);
		return BoneNameStr.find(FilterLower) != FString::npos;
	};

	// 자식 중 필터에 매칭되는 항목이 있는지 재귀 확인
	std::function<bool(int32)> HasMatchingDescendant = [&](int32 Index) -> bool
	{
		if (MatchesFilter(Index)) return true;
		for (int32 Child : Children[Index])
		{
			if (HasMatchingDescendant(Child)) return true;
		}
		return false;
	};

	// ImDrawList를 사용하여 아이콘을 오버레이로 그리는 헬퍼
	ImDrawList* drawList = ImGui::GetWindowDrawList();
	const float iconSpacing = 2.0f;  // 아이콘과 화살표 사이 간격

	// 재귀적 노드 그리기
	std::function<void(int32)> DrawNode = [&](int32 Index)
	{
		// 필터가 있을 때, 이 노드나 자손이 매칭되지 않으면 스킵
		if (bHasFilter && !HasMatchingDescendant(Index)) return;

		const FName& BoneName = Bones[Index].Name;

		// PhysicsAsset에서 해당 본의 BodySetup 찾기
		int32 BodyIndex = PhysAsset ? PhysAsset->FindBodySetupIndex(BoneName) : -1;
		bool bHasBody = (BodyIndex != -1);

		// 바디가 있으면 자식으로 취급 (트리 구조상)
		bool bHasChildren = !Children[Index].IsEmpty();
		bool bLeaf = !bHasChildren && !bHasBody;

		// 본 선택 상태
		bool bBoneSelected = (State->SelectedBoneIndex == Index);

		ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_AllowItemOverlap;
		if (bLeaf)
		{
			flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
		}
		if (bBoneSelected)
		{
			flags |= ImGuiTreeNodeFlags_Selected;
		}

		// 필터가 있으면 자동 펼침
		if (bHasFilter)
		{
			ImGui::SetNextItemOpen(true);
		}

		ImGui::PushID(Index);

		// 선택된 본 스타일
		if (bBoneSelected)
		{
			ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.35f, 0.55f, 0.85f, 0.8f));
			ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.40f, 0.60f, 0.90f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.30f, 0.50f, 0.80f, 1.0f));
		}

		// 레이아웃: [Icon][▼][Name]
		// 1. 현재 위치 저장 (아이콘 그릴 위치)
		ImVec2 cursorScreenPos = ImGui::GetCursorScreenPos();
		float currentIndent = ImGui::GetCursorPosX();

		// 2. 아이콘 공간만큼 들여쓰기 후 TreeNode 그리기
		ImGui::SetCursorPosX(currentIndent + IconSize.x + iconSpacing);
		bool open = ImGui::TreeNodeEx((void*)(intptr_t)Index, flags, "%s", BoneName.ToString().c_str());

		// 3. 아이콘을 오버레이로 그리기 (TreeNode 앞에)
		if (IconBone && IconBone->GetShaderResourceView())
		{
			ImVec2 iconPos;
			iconPos.x = cursorScreenPos.x;
			iconPos.y = cursorScreenPos.y + (ImGui::GetTextLineHeight() - IconSize.y) * 0.5f;
			drawList->AddImage(
				(ImTextureID)IconBone->GetShaderResourceView(),
				iconPos,
				ImVec2(iconPos.x + IconSize.x, iconPos.y + IconSize.y)
			);
		}

		if (bBoneSelected)
		{
			ImGui::PopStyleColor(3);
		}

		// 본 클릭 처리
		if (ImGui::IsItemClicked())
		{
			State->SelectedBoneIndex = Index;
			State->SelectedBodyIndex = -1;
			State->SelectedConstraintIndex = -1;
			State->GraphRootBodyIndex = -1;  // 본 선택 시 그래프 루트도 초기화
		}

		if (!bLeaf && open)
		{
			// 바디가 있으면 본의 자식으로 표시
			if (bHasBody)
			{
				ImGui::PushID("Body");

				bool bBodySelected = (State->SelectedBodyIndex == BodyIndex);
				ImGuiTreeNodeFlags bodyFlags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_AllowItemOverlap;
				if (bBodySelected)
				{
					bodyFlags |= ImGuiTreeNodeFlags_Selected;
					ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.35f, 0.55f, 0.85f, 0.8f));
					ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.40f, 0.60f, 0.90f, 1.0f));
					ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.30f, 0.50f, 0.80f, 1.0f));
				}

				// 레이아웃: [Icon][▼][Name]
				ImVec2 bodyScreenPos = ImGui::GetCursorScreenPos();
				float bodyIndent = ImGui::GetCursorPosX();

				// 아이콘 공간만큼 들여쓰기 후 TreeNode 그리기
				ImGui::SetCursorPosX(bodyIndent + IconSize.x + iconSpacing);
				ImGui::TreeNodeEx("BodyNode", bodyFlags, "%s", BoneName.ToString().c_str());

				// 바디 아이콘을 오버레이로 그리기
				if (IconBody && IconBody->GetShaderResourceView())
				{
					ImVec2 iconPos;
					iconPos.x = bodyScreenPos.x;
					iconPos.y = bodyScreenPos.y + (ImGui::GetTextLineHeight() - IconSize.y) * 0.5f;
					drawList->AddImage(
						(ImTextureID)IconBody->GetShaderResourceView(),
						iconPos,
						ImVec2(iconPos.x + IconSize.x, iconPos.y + IconSize.y)
					);
				}

				if (bBodySelected)
				{
					ImGui::PopStyleColor(3);
				}

				// 바디 클릭 처리 (스켈레톤 트리에서 선택 시 그래프 루트도 변경)
				if (ImGui::IsItemClicked())
				{
					State->GraphRootBodyIndex = BodyIndex;  // 그래프 중심 변경
					State->SelectedBodyIndex = BodyIndex;   // 하이라이트도 동일하게
					State->SelectedBoneIndex = -1;
					State->SelectedConstraintIndex = -1;
				}

				ImGui::PopID();
			}

			// 자식 본 그리기
			for (int32 Child : Children[Index])
			{
				DrawNode(Child);
			}
			ImGui::TreePop();
		}
		ImGui::PopID();
	};

	// 루트 본부터 시작
	for (int32 i = 0; i < Bones.size(); ++i)
	{
		if (Bones[i].ParentIndex < 0)
		{
			DrawNode(i);
		}
	}

	ImGui::PopStyleVar();
	ImGui::EndChild();
}

void SPhysicsAssetEditorWindow::RenderGraphPanel()
{
	PhysicsAssetEditorState* State = GetActivePhysicsState();
	if (!State) return;

	UPhysicsAsset* PhysAsset = State->EditingPhysicsAsset;
	ImDrawList* DrawList = ImGui::GetWindowDrawList();
	ImVec2 WindowPos = ImGui::GetCursorScreenPos();
	ImVec2 WindowSize = ImGui::GetContentRegionAvail();

	// 클리핑 영역 설정
	DrawList->PushClipRect(WindowPos, ImVec2(WindowPos.x + WindowSize.x, WindowPos.y + WindowSize.y), true);

	// === 그리드 배경 그리기 ===
	const float GridSize = 20.0f * State->GraphZoomLevel;
	const ImU32 ColorGridLine = IM_COL32(50, 50, 50, 255);
	const ImU32 ColorGridLineBold = IM_COL32(70, 70, 70, 255);

	// 그리드 시작점 (팬 오프셋 적용)
	float GridOffsetX = fmodf(State->GraphPanOffset.x, GridSize * 5.0f);
	float GridOffsetY = fmodf(State->GraphPanOffset.y, GridSize * 5.0f);

	// 수직 그리드선
	int lineCount = 0;
	for (float x = WindowPos.x + GridOffsetX; x < WindowPos.x + WindowSize.x; x += GridSize)
	{
		ImU32 color = (lineCount % 5 == 0) ? ColorGridLineBold : ColorGridLine;
		DrawList->AddLine(ImVec2(x, WindowPos.y), ImVec2(x, WindowPos.y + WindowSize.y), color);
		lineCount++;
	}

	// 수평 그리드선
	lineCount = 0;
	for (float y = WindowPos.y + GridOffsetY; y < WindowPos.y + WindowSize.y; y += GridSize)
	{
		ImU32 color = (lineCount % 5 == 0) ? ColorGridLineBold : ColorGridLine;
		DrawList->AddLine(ImVec2(WindowPos.x, y), ImVec2(WindowPos.x + WindowSize.x, y), color);
		lineCount++;
	}

	// === 마우스 입력 처리 (영역 체크) ===
	ImVec2 MousePos = ImGui::GetIO().MousePos;
	bool bHovered = (MousePos.x >= WindowPos.x && MousePos.x <= WindowPos.x + WindowSize.x &&
	                 MousePos.y >= WindowPos.y && MousePos.y <= WindowPos.y + WindowSize.y);

	if (bHovered)
	{
		// 마우스 휠 줌
		float wheel = ImGui::GetIO().MouseWheel;
		if (wheel != 0.0f)
		{
			State->GraphZoomLevel += wheel * 0.1f;
			State->GraphZoomLevel = std::clamp(State->GraphZoomLevel, 0.3f, 2.0f);
		}

		// 우클릭 드래그로 팬
		if (ImGui::IsMouseDragging(ImGuiMouseButton_Right))
		{
			ImVec2 delta = ImGui::GetIO().MouseDelta;
			State->GraphPanOffset.x += delta.x;
			State->GraphPanOffset.y += delta.y;
		}
	}

	// 줌 레벨 표시 (우측 상단)
	char ZoomText[32];
	sprintf_s(ZoomText, "1:%.0f 줌", State->GraphZoomLevel * 100.0f);
	ImVec2 ZoomTextSize = ImGui::CalcTextSize(ZoomText);
	DrawList->AddText(ImVec2(WindowPos.x + WindowSize.x - ZoomTextSize.x - 10.0f, WindowPos.y + 5.0f),
		IM_COL32(150, 150, 150, 255), ZoomText);

	if (!PhysAsset)
	{
		DrawList->AddText(ImVec2(WindowPos.x + 10.0f, WindowPos.y + WindowSize.y * 0.5f),
			IM_COL32(128, 128, 128, 255), "PhysicsAsset이 없습니다.");
		DrawList->PopClipRect();
		return;
	}

	// 그래프 루트 바디 가져오기 (스켈레톤 트리에서 선택된 바디)
	USkeletalBodySetup* RootBody = nullptr;
	FName RootBoneName;
	if (State->GraphRootBodyIndex >= 0 && State->GraphRootBodyIndex < PhysAsset->GetBodySetupCount())
	{
		RootBody = PhysAsset->GetBodySetup(State->GraphRootBodyIndex);
		if (RootBody)
		{
			RootBoneName = RootBody->BoneName;
		}
	}

	if (!RootBody)
	{
		return;
	}

	// === 연결된 컨스트레인트와 바디 수집 ===
	struct FConnectedInfo
	{
		int32 ConstraintIndex;
		UPhysicsConstraintTemplate* Constraint;
		FName OtherBoneName;
		int32 OtherBodyIndex;
	};
	TArray<FConnectedInfo> ConnectedInfos;

	int32 ConstraintCount = PhysAsset->GetConstraintCount();
	for (int32 i = 0; i < ConstraintCount; ++i)
	{
		UPhysicsConstraintTemplate* Constraint = PhysAsset->ConstraintSetup[i];
		if (!Constraint) continue;

		FName Bone1 = Constraint->GetBone1Name();
		FName Bone2 = Constraint->GetBone2Name();

		FName OtherBone;
		if (Bone1 == RootBoneName)
		{
			OtherBone = Bone2;
		}
		else if (Bone2 == RootBoneName)
		{
			OtherBone = Bone1;
		}
		else
		{
			continue;
		}

		int32 OtherBodyIdx = -1;
		for (int32 j = 0; j < PhysAsset->GetBodySetupCount(); ++j)
		{
			USkeletalBodySetup* Body = PhysAsset->GetBodySetup(j);
			if (Body && Body->BoneName == OtherBone)
			{
				OtherBodyIdx = j;
				break;
			}
		}

		FConnectedInfo Info;
		Info.ConstraintIndex = i;
		Info.Constraint = Constraint;
		Info.OtherBoneName = OtherBone;
		Info.OtherBodyIndex = OtherBodyIdx;
		ConnectedInfos.Add(Info);
	}

	// === 줌이 적용된 노드 크기 ===
	const float Zoom = State->GraphZoomLevel;
	const float NodeWidth = 100.0f * Zoom;
	const float NodeHeight = 45.0f * Zoom;
	const float NodePadding = 8.0f * Zoom;
	const float FontScale = Zoom;

	// 색상 정의
	const ImU32 ColorBodyNode = IM_COL32(80, 120, 80, 255);
	const ImU32 ColorBodyNodeSelected = IM_COL32(120, 160, 80, 255);
	const ImU32 ColorConstraintNode = IM_COL32(180, 140, 60, 255);
	const ImU32 ColorBorder = IM_COL32(200, 200, 100, 255);
	const ImU32 ColorBorderNormal = IM_COL32(80, 80, 80, 255);
	const ImU32 ColorLine = IM_COL32(150, 150, 150, 200);
	const ImU32 ColorText = IM_COL32(255, 255, 255, 255);

	// 팬 오프셋 적용
	const float PanX = State->GraphPanOffset.x;
	const float PanY = State->GraphPanOffset.y;

	// 열 위치 계산 (줌 + 팬 적용)
	const float Col1X = WindowPos.x + 15.0f * Zoom + PanX;
	const float Col2X = WindowPos.x + WindowSize.x * 0.35f + PanX;
	const float Col3X = WindowPos.x + WindowSize.x * 0.68f + PanX;

	// 중앙 Y 위치 계산 (팬 적용)
	int32 NumConnections = ConnectedInfos.Num();
	float TotalHeight = NumConnections > 0 ? NumConnections * (NodeHeight + NodePadding) - NodePadding : NodeHeight;
	float StartY = WindowPos.y + (WindowSize.y - TotalHeight) * 0.5f + PanY;

	float RootBodyY = WindowPos.y + WindowSize.y * 0.5f - NodeHeight * 0.5f + PanY;

	// 폰트 스케일 적용
	ImGui::SetWindowFontScale(FontScale);

	// === 1. 루트 바디 노드 그리기 (좌측) ===
	ImVec2 RootBodyMin(Col1X, RootBodyY);
	ImVec2 RootBodyMax(Col1X + NodeWidth, RootBodyY + NodeHeight);

	// 루트 바디가 하이라이트 되었는지 확인
	bool bRootBodyHighlighted = (State->SelectedBodyIndex == State->GraphRootBodyIndex);
	DrawList->AddRectFilled(RootBodyMin, RootBodyMax, bRootBodyHighlighted ? ColorBodyNodeSelected : ColorBodyNode, 4.0f * Zoom);
	DrawList->AddRect(RootBodyMin, RootBodyMax, bRootBodyHighlighted ? ColorBorder : ColorBorderNormal, 4.0f * Zoom, 0, bRootBodyHighlighted ? 2.0f : 1.0f);

	const char* BodyLabel = "바디";
	ImVec2 LabelSize = ImGui::CalcTextSize(BodyLabel);
	DrawList->AddText(ImVec2(Col1X + (NodeWidth - LabelSize.x) * 0.5f, RootBodyY + 4.0f * Zoom), ColorText, BodyLabel);

	FString BoneNameStr = RootBoneName.ToString();
	if (BoneNameStr.size() > 12) BoneNameStr = BoneNameStr.substr(0, 9) + "...";
	ImVec2 BoneNameSize = ImGui::CalcTextSize(BoneNameStr.c_str());
	DrawList->AddText(ImVec2(Col1X + (NodeWidth - BoneNameSize.x) * 0.5f, RootBodyY + 17.0f * Zoom), ColorText, BoneNameStr.c_str());

	int32 ShapeCount = RootBody->AggGeom.GetElementCount();
	char ShapeText[32];
	sprintf_s(ShapeText, "셰이프 %d개", ShapeCount);
	ImVec2 ShapeSize = ImGui::CalcTextSize(ShapeText);
	DrawList->AddText(ImVec2(Col1X + (NodeWidth - ShapeSize.x) * 0.5f, RootBodyY + 30.0f * Zoom), IM_COL32(180, 180, 180, 255), ShapeText);

	// 루트 바디 클릭 처리
	ImGui::SetWindowFontScale(1.0f);
	ImGui::SetCursorScreenPos(RootBodyMin);
	ImGui::InvisibleButton("root_body_btn", ImVec2(NodeWidth, NodeHeight));
	if (ImGui::IsItemClicked())
	{
		State->SelectedBodyIndex = State->GraphRootBodyIndex;
		State->SelectedConstraintIndex = -1;
		State->SelectedBoneIndex = -1;
	}
	ImGui::SetWindowFontScale(FontScale);

	// === 2. 컨스트레인트 및 연결된 바디 노드 그리기 ===
	for (int32 i = 0; i < ConnectedInfos.Num(); ++i)
	{
		const FConnectedInfo& Info = ConnectedInfos[i];
		float NodeY = StartY + i * (NodeHeight + NodePadding);

		// --- 컨스트레인트 노드 (중앙) ---
		ImVec2 ConstraintMin(Col2X, NodeY);
		ImVec2 ConstraintMax(Col2X + NodeWidth, NodeY + NodeHeight);

		bool bConstraintSelected = (State->SelectedConstraintIndex == Info.ConstraintIndex);
		DrawList->AddRectFilled(ConstraintMin, ConstraintMax, ColorConstraintNode, 4.0f * Zoom);
		DrawList->AddRect(ConstraintMin, ConstraintMax, bConstraintSelected ? ColorBorder : ColorBorderNormal, 4.0f * Zoom, 0, bConstraintSelected ? 2.0f : 1.0f);

		const char* ConstraintLabel = "컨스트레인트";
		ImVec2 CLabelSize = ImGui::CalcTextSize(ConstraintLabel);
		DrawList->AddText(ImVec2(Col2X + (NodeWidth - CLabelSize.x) * 0.5f, NodeY + 4.0f * Zoom), ColorText, ConstraintLabel);

		FString ConstraintName = Info.Constraint->GetBone1Name().ToString() + ":" + Info.Constraint->GetBone2Name().ToString();
		if (ConstraintName.size() > 16) ConstraintName = ConstraintName.substr(0, 13) + "...";
		ImVec2 CNameSize = ImGui::CalcTextSize(ConstraintName.c_str());
		DrawList->AddText(ImVec2(Col2X + (NodeWidth - CNameSize.x) * 0.5f, NodeY + 22.0f * Zoom), ColorText, ConstraintName.c_str());

		// 클릭 영역 (폰트 스케일 복원 후)
		ImGui::SetWindowFontScale(1.0f);
		ImGui::SetCursorScreenPos(ConstraintMin);
		ImGui::InvisibleButton(("constraint_btn_" + std::to_string(i)).c_str(), ImVec2(NodeWidth, NodeHeight));
		if (ImGui::IsItemClicked())
		{
			State->SelectedConstraintIndex = Info.ConstraintIndex;
			State->SelectedBodyIndex = -1;
			State->SelectedBoneIndex = -1;
		}
		ImGui::SetWindowFontScale(FontScale);

		// --- 연결된 바디 노드 (우측) ---
		ImVec2 OtherBodyMin(Col3X, NodeY);
		ImVec2 OtherBodyMax(Col3X + NodeWidth, NodeY + NodeHeight);

		bool bOtherBodySelected = (State->SelectedBodyIndex == Info.OtherBodyIndex);
		DrawList->AddRectFilled(OtherBodyMin, OtherBodyMax, bOtherBodySelected ? ColorBodyNodeSelected : ColorBodyNode, 4.0f * Zoom);
		DrawList->AddRect(OtherBodyMin, OtherBodyMax, bOtherBodySelected ? ColorBorder : ColorBorderNormal, 4.0f * Zoom, 0, bOtherBodySelected ? 2.0f : 1.0f);

		DrawList->AddText(ImVec2(Col3X + (NodeWidth - LabelSize.x) * 0.5f, NodeY + 4.0f * Zoom), ColorText, BodyLabel);

		FString OtherBoneStr = Info.OtherBoneName.ToString();
		if (OtherBoneStr.size() > 12) OtherBoneStr = OtherBoneStr.substr(0, 9) + "...";
		ImVec2 OtherNameSize = ImGui::CalcTextSize(OtherBoneStr.c_str());
		DrawList->AddText(ImVec2(Col3X + (NodeWidth - OtherNameSize.x) * 0.5f, NodeY + 17.0f * Zoom), ColorText, OtherBoneStr.c_str());

		int32 OtherShapeCount = 0;
		if (Info.OtherBodyIndex >= 0)
		{
			USkeletalBodySetup* OtherBody = PhysAsset->GetBodySetup(Info.OtherBodyIndex);
			if (OtherBody) OtherShapeCount = OtherBody->AggGeom.GetElementCount();
		}
		sprintf_s(ShapeText, "셰이프 %d개", OtherShapeCount);
		ShapeSize = ImGui::CalcTextSize(ShapeText);
		DrawList->AddText(ImVec2(Col3X + (NodeWidth - ShapeSize.x) * 0.5f, NodeY + 30.0f * Zoom), IM_COL32(180, 180, 180, 255), ShapeText);

		// 클릭 영역
		ImGui::SetWindowFontScale(1.0f);
		ImGui::SetCursorScreenPos(OtherBodyMin);
		ImGui::InvisibleButton(("body_btn_" + std::to_string(i)).c_str(), ImVec2(NodeWidth, NodeHeight));
		if (ImGui::IsItemClicked() && Info.OtherBodyIndex >= 0)
		{
			State->SelectedBodyIndex = Info.OtherBodyIndex;
			State->SelectedConstraintIndex = -1;
			State->SelectedBoneIndex = -1;
		}
		ImGui::SetWindowFontScale(FontScale);

		// === 3. 연결선 그리기 (베지어 커브) ===
		// 커브 (줌에 따라 모양 유지)
		const float LineThickness = 1.5f;
		const float CurveRatio = 0.35f;  // 노드 간 거리의 35%를 커브 오프셋으로 사용

		ImVec2 P1(RootBodyMax.x, RootBodyY + NodeHeight * 0.5f);
		ImVec2 P2(ConstraintMin.x, NodeY + NodeHeight * 0.5f);
		float Dist1 = P2.x - P1.x;
		float Offset1 = Dist1 * CurveRatio;
		ImVec2 CP1(P1.x + Offset1, P1.y);
		ImVec2 CP2(P2.x - Offset1, P2.y);
		DrawList->AddBezierCubic(P1, CP1, CP2, P2, ColorLine, LineThickness);

		ImVec2 P3(ConstraintMax.x, NodeY + NodeHeight * 0.5f);
		ImVec2 P4(OtherBodyMin.x, NodeY + NodeHeight * 0.5f);
		float Dist2 = P4.x - P3.x;
		float Offset2 = Dist2 * CurveRatio;
		ImVec2 CP3(P3.x + Offset2, P3.y);
		ImVec2 CP4(P4.x - Offset2, P4.y);
		DrawList->AddBezierCubic(P3, CP3, CP4, P4, ColorLine, LineThickness);
	}

	// 연결이 없는 경우 메시지
	if (ConnectedInfos.Num() == 0)
	{
		DrawList->AddText(ImVec2(Col2X, RootBodyY + NodeHeight * 0.5f - 10.0f),
			IM_COL32(128, 128, 128, 255), "연결된 컨스트레인트가 없습니다.");
	}

	// 폰트 스케일 복원 및 클리핑 해제
	ImGui::SetWindowFontScale(1.0f);
	DrawList->PopClipRect();
}

void SPhysicsAssetEditorWindow::RenderDetailsPanel()
{
	PhysicsAssetEditorState* State = GetActivePhysicsState();
	if (!State) return;

	UPhysicsAsset* PhysAsset = State->EditingPhysicsAsset;

	// 본이 선택된 경우
	if (State->SelectedBoneIndex != -1)
	{
		RenderBoneDetails(State->SelectedBoneIndex);
	}
	// 바디가 선택된 경우
	else if (State->SelectedBodyIndex != -1 && PhysAsset)
	{
		USkeletalBodySetup* Body = PhysAsset->GetBodySetup(State->SelectedBodyIndex);
		if (Body)
		{
			RenderBodyDetails(Body, State->SelectedBodyIndex);
		}
	}
	// 컨스트레인트가 선택된 경우
	else if (State->SelectedConstraintIndex != -1 && PhysAsset)
	{
		if (State->SelectedConstraintIndex < PhysAsset->GetConstraintCount())
		{
			UPhysicsConstraintTemplate* Constraint = PhysAsset->ConstraintSetup[State->SelectedConstraintIndex];
			if (Constraint)
			{
				RenderConstraintDetails(Constraint, State->SelectedConstraintIndex);
			}
		}
	}
}

void SPhysicsAssetEditorWindow::RenderBoneDetails(int32 BoneIndex)
{
	PhysicsAssetEditorState* State = GetActivePhysicsState();
	if (!State) return;

	// 프리뷰 액터에서 스켈레탈 메시 가져오기
	USkeletalMesh* Mesh = nullptr;
	if (State->PreviewActor)
	{
		if (USkeletalMeshComponent* MeshComp = State->PreviewActor->GetSkeletalMeshComponent())
		{
			Mesh = MeshComp->GetSkeletalMesh();
		}
	}
	if (!Mesh) return;

	const FSkeleton* Skeleton = Mesh->GetSkeleton();
	if (!Skeleton) return;

	const TArray<FBone>& Bones = Skeleton->Bones;
	if (BoneIndex < 0 || BoneIndex >= (int32)Bones.Num()) return;

	const FBone& Bone = Bones[BoneIndex];
	UPhysicsAsset* PhysAsset = State->EditingPhysicsAsset;

	// ▼ Bone Info 섹션
	if (ImGui::CollapsingHeader("본", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::Indent(10.0f);

		// 본 이름
		ImGui::Text("본 이름:");
		ImGui::SameLine(100.0f);
		ImGui::TextColored(ImVec4(0.8f, 0.9f, 1.0f, 1.0f), "%s", Bone.Name.c_str());

		// 본 인덱스
		ImGui::Text("본 인덱스:");
		ImGui::SameLine(100.0f);
		ImGui::Text("%d", BoneIndex);

		// 부모 본
		ImGui::Text("부모 본:");
		ImGui::SameLine(100.0f);
		if (Bone.ParentIndex >= 0 && Bone.ParentIndex < (int32)Bones.Num())
		{
			ImGui::Text("%s (%d)", Bones[Bone.ParentIndex].Name.c_str(), Bone.ParentIndex);
		}
		else
		{
			ImGui::TextDisabled("None (Root)");
		}

		ImGui::Separator();

		// 본 트랜스폼 (읽기 전용) - BindPose 행렬에서 추출
		ImGui::Text("바인드 포즈:");

		// FMatrix에서 위치 추출 (행렬의 4행)
		FVector Location(Bone.BindPose.M[3][0], Bone.BindPose.M[3][1], Bone.BindPose.M[3][2]);

		ImGui::BeginDisabled(true);
		ImGui::DragFloat3("Location", &Location.X, 0.1f);
		ImGui::EndDisabled();

		ImGui::Unindent(10.0f);
	}

	ImGui::Spacing();
	ImGui::Separator();
	ImGui::Spacing();

	// 바디 존재 여부 확인
	int32 ExistingBodyIndex = PhysAsset ? PhysAsset->FindBodySetupIndex(Bone.Name) : -1;

	if (ExistingBodyIndex != -1)
	{
		// 이미 바디가 있는 경우
		ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), "이미 피직스 바디가 존재합니다.");
		ImGui::Spacing();

		if (ImGui::Button("바디 편집", ImVec2(-1, 0)))
		{
			State->GraphRootBodyIndex = ExistingBodyIndex;  // 그래프 중심 변경
			State->SelectedBodyIndex = ExistingBodyIndex;
			State->SelectedBoneIndex = -1;
			State->SelectedConstraintIndex = -1;
		}
	}
	else
	{
		// 바디가 없는 경우 - 생성 옵션
		ImGui::Indent(10.0f);
		ImGui::Text("바디 추가");
		ImGui::Spacing();

		// Shape 버튼 스타일
		const float buttonWidth = 50.0f;
		const float addButtonWidth = 60.0f;
		const float buttonSpacing = 2.0f;

		// 선택 안된 버튼 스타일 (어두운 배경)
		ImVec4 normalColor = ImVec4(0.25f, 0.25f, 0.28f, 1.0f);
		ImVec4 hoverColor = ImVec4(0.35f, 0.35f, 0.38f, 1.0f);
		ImVec4 activeColor = ImVec4(0.20f, 0.20f, 0.23f, 1.0f);
		// 선택된 버튼 스타일 (파란색)
		ImVec4 selectedColor = ImVec4(0.26f, 0.59f, 0.98f, 0.8f);
		ImVec4 selectedHoverColor = ImVec4(0.30f, 0.65f, 1.0f, 0.9f);
		ImVec4 selectedActiveColor = ImVec4(0.22f, 0.52f, 0.88f, 1.0f);

		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(buttonSpacing, 0));

		// 구
		bool bSphereSelected = (SelectedShapeType == 0);
		if (bSphereSelected)
		{
			ImGui::PushStyleColor(ImGuiCol_Button, selectedColor);
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, selectedHoverColor);
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, selectedActiveColor);
		}
		else
		{
			ImGui::PushStyleColor(ImGuiCol_Button, normalColor);
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, hoverColor);
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, activeColor);
		}
		if (ImGui::Button("구", ImVec2(buttonWidth, 0)))
			SelectedShapeType = 0;
		ImGui::PopStyleColor(3);

		ImGui::SameLine();

		// 박스
		bool bBoxSelected = (SelectedShapeType == 1);
		if (bBoxSelected)
		{
			ImGui::PushStyleColor(ImGuiCol_Button, selectedColor);
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, selectedHoverColor);
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, selectedActiveColor);
		}
		else
		{
			ImGui::PushStyleColor(ImGuiCol_Button, normalColor);
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, hoverColor);
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, activeColor);
		}
		if (ImGui::Button("박스", ImVec2(buttonWidth, 0)))
			SelectedShapeType = 1;
		ImGui::PopStyleColor(3);

		ImGui::SameLine();

		// 캡슐
		bool bCapsuleSelected = (SelectedShapeType == 2);
		if (bCapsuleSelected)
		{
			ImGui::PushStyleColor(ImGuiCol_Button, selectedColor);
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, selectedHoverColor);
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, selectedActiveColor);
		}
		else
		{
			ImGui::PushStyleColor(ImGuiCol_Button, normalColor);
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, hoverColor);
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, activeColor);
		}
		if (ImGui::Button("캡슐", ImVec2(buttonWidth, 0)))
			SelectedShapeType = 2;
		ImGui::PopStyleColor(3);

		ImGui::PopStyleVar(); // ItemSpacing

		// 오른쪽 끝에 추가 버튼 (초록색 강조)
		ImGui::SameLine(ImGui::GetContentRegionAvail().x - addButtonWidth + ImGui::GetCursorPosX());
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.55f, 0.3f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.65f, 0.35f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.15f, 0.45f, 0.25f, 1.0f));
		if (ImGui::Button("+ 추가", ImVec2(addButtonWidth, 0)))
		{
			AddBodyToBone(BoneIndex, SelectedShapeType);
		}
		ImGui::PopStyleColor(3);

		// 하단 여백 및 구분선
		ImGui::Unindent(10.0f);
		ImGui::Spacing();
		ImGui::Separator();
	}
}

void SPhysicsAssetEditorWindow::RenderBodyDetails(USkeletalBodySetup* Body, int32 BodyIndex)
{
	PhysicsAssetEditorState* State = GetActivePhysicsState();
	if (!State) return;

	bool bChanged = false;

	// ▼ Body Setup 섹션
	if (ImGui::CollapsingHeader("바디 셋업", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::Indent(10.0f);

		// 본 이름 (읽기 전용)
		ImGui::Text("본 이름:");
		ImGui::SameLine(120.0f);
		ImGui::TextColored(ImVec4(0.8f, 0.9f, 1.0f, 1.0f), "%s", Body->BoneName.ToString().c_str());

		// Physics Type
		ImGui::Text("피직스 타입:");
		ImGui::SameLine(120.0f);
		const char* physicsTypes[] = { "Default", "Simulated", "Kinematic" };
		int currentPhysType = static_cast<int>(Body->PhysicsType);
		ImGui::SetNextItemWidth(-1);
		if (ImGui::Combo("##PhysicsType", &currentPhysType, physicsTypes, IM_ARRAYSIZE(physicsTypes)))
		{
			Body->PhysicsType = static_cast<EPhysicsType>(currentPhysType);
			bChanged = true;
		}

		// Collision Response
		ImGui::Text("충돌:");
		ImGui::SameLine(120.0f);
		const char* collisionTypes[] = { "Enabled", "Disabled" };
		int currentCollision = static_cast<int>(Body->CollisionResponse);
		ImGui::SetNextItemWidth(-1);
		if (ImGui::Combo("##Collision", &currentCollision, collisionTypes, IM_ARRAYSIZE(collisionTypes)))
		{
			Body->CollisionResponse = static_cast<EBodyCollisionResponse::Type>(currentCollision);
			bChanged = true;
		}

		ImGui::Unindent(10.0f);
	}

	ImGui::Spacing();

	// ▼ Collision Shape 섹션
	if (ImGui::CollapsingHeader("충돌 프리미티브", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::Indent(10.0f);

		// 현재 Shape 정보 표시
		FKAggregateGeom& AggGeom = Body->AggGeom;
		int32 sphereCount = AggGeom.SphereElems.Num();
		int32 boxCount = AggGeom.BoxElems.Num();
		int32 capsuleCount = AggGeom.SphylElems.Num();

		if (sphereCount == 0 && boxCount == 0 && capsuleCount == 0)
		{
			ImGui::TextDisabled("충돌 프리미티브가 없습니다.");

			ImGui::Spacing();
			ImGui::Text("프리미티브 추가");
			if (ImGui::Button("+ 구")) { Body->AddSphereElem(5.0f); bChanged = true; }
			ImGui::SameLine();
			if (ImGui::Button("+ 박스")) { Body->AddBoxElem(5.0f, 5.0f, 5.0f); bChanged = true; }
			ImGui::SameLine();
			if (ImGui::Button("+ 캡슐")) { Body->AddCapsuleElem(3.0f, 10.0f); bChanged = true; }
		}
		else
		{
			// Shape 렌더링
			if (RenderShapeDetails(Body))
			{
				bChanged = true;
			}
		}

		ImGui::Unindent(10.0f);
	}

	ImGui::Spacing();
	ImGui::Separator();
	ImGui::Spacing();

	// 삭제 버튼
	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.2f, 0.2f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.8f, 0.3f, 0.3f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.5f, 0.15f, 0.15f, 1.0f));
	if (ImGui::Button("바디 삭제", ImVec2(-1, 0)))
	{
		RemoveBody(BodyIndex);
	}
	ImGui::PopStyleColor(3);

	// 변경 플래그 설정
	if (bChanged)
	{
		State->bIsDirty = true;
	}
}

void SPhysicsAssetEditorWindow::RenderConstraintDetails(UPhysicsConstraintTemplate* Constraint, int32 ConstraintIndex)
{
	PhysicsAssetEditorState* State = GetActivePhysicsState();
	if (!State || !Constraint) return;

	bool bChanged = false;
	FConstraintInstance& CI = Constraint->DefaultInstance;

	// ▼ Constraint 기본 정보
	if (ImGui::CollapsingHeader("컨스트레인트 정보", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::Indent(10.0f);

		// 조인트 이름
		ImGui::Text("조인트 이름:");
		ImGui::SameLine(120.0f);
		ImGui::TextColored(ImVec4(0.9f, 0.8f, 0.5f, 1.0f), "%s", CI.JointName.ToString().c_str());

		// 연결된 본들
		ImGui::Text("본 1 (부모):");
		ImGui::SameLine(120.0f);
		ImGui::TextColored(ImVec4(0.8f, 0.9f, 1.0f, 1.0f), "%s", CI.ConstraintBone1.ToString().c_str());

		ImGui::Text("본 2 (자식):");
		ImGui::SameLine(120.0f);
		ImGui::TextColored(ImVec4(0.8f, 0.9f, 1.0f, 1.0f), "%s", CI.ConstraintBone2.ToString().c_str());

		ImGui::Unindent(10.0f);
	}

	ImGui::Spacing();

	// ▼ Angular Limits
	if (ImGui::CollapsingHeader("각도 제한", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::Indent(10.0f);

		const char* motionTypes[] = { "Free", "Limited", "Locked" };

		// Swing1 Motion
		ImGui::Text("Swing1:");
		ImGui::SameLine(120.0f);
		int swing1 = static_cast<int>(CI.AngularSwing1Motion);
		ImGui::SetNextItemWidth(100.0f);
		if (ImGui::Combo("##Swing1", &swing1, motionTypes, IM_ARRAYSIZE(motionTypes)))
		{
			CI.AngularSwing1Motion = static_cast<EAngularConstraintMotion>(swing1);
			bChanged = true;
		}

		// Swing2 Motion
		ImGui::Text("Swing2:");
		ImGui::SameLine(120.0f);
		int swing2 = static_cast<int>(CI.AngularSwing2Motion);
		ImGui::SetNextItemWidth(100.0f);
		if (ImGui::Combo("##Swing2", &swing2, motionTypes, IM_ARRAYSIZE(motionTypes)))
		{
			CI.AngularSwing2Motion = static_cast<EAngularConstraintMotion>(swing2);
			bChanged = true;
		}

		// Twist Motion
		ImGui::Text("Twist:");
		ImGui::SameLine(120.0f);
		int twist = static_cast<int>(CI.AngularTwistMotion);
		ImGui::SetNextItemWidth(100.0f);
		if (ImGui::Combo("##Twist", &twist, motionTypes, IM_ARRAYSIZE(motionTypes)))
		{
			CI.AngularTwistMotion = static_cast<EAngularConstraintMotion>(twist);
			bChanged = true;
		}

		ImGui::Spacing();

		// Swing1 Limit Angle
		ImGui::Text("Swing1 제한각:");
		ImGui::SameLine(120.0f);
		ImGui::SetNextItemWidth(-1);
		if (ImGui::InputFloat("##Swing1Limit", &CI.Swing1LimitAngle, 1.0f, 10.0f, "%.1f"))
		{
			CI.Swing1LimitAngle = std::clamp(CI.Swing1LimitAngle, 0.0f, 180.0f);
			bChanged = true;
		}

		// Swing2 Limit Angle
		ImGui::Text("Swing2 제한각:");
		ImGui::SameLine(120.0f);
		ImGui::SetNextItemWidth(-1);
		if (ImGui::InputFloat("##Swing2Limit", &CI.Swing2LimitAngle, 1.0f, 10.0f, "%.1f"))
		{
			CI.Swing2LimitAngle = std::clamp(CI.Swing2LimitAngle, 0.0f, 180.0f);
			bChanged = true;
		}

		// Twist Limit Angle
		ImGui::Text("Twist 제한각:");
		ImGui::SameLine(120.0f);
		ImGui::SetNextItemWidth(-1);
		if (ImGui::InputFloat("##TwistLimit", &CI.TwistLimitAngle, 1.0f, 10.0f, "%.1f"))
		{
			CI.TwistLimitAngle = std::clamp(CI.TwistLimitAngle, 0.0f, 180.0f);
			bChanged = true;
		}

		ImGui::Unindent(10.0f);
	}

	ImGui::Spacing();

	// ▼ Linear Limits
	if (ImGui::CollapsingHeader("선형 제한", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::Indent(10.0f);

		const char* linearMotionTypes[] = { "Free", "Limited", "Locked" };

		// Linear X Motion
		ImGui::Text("X축:");
		ImGui::SameLine(120.0f);
		int linearX = static_cast<int>(CI.LinearXMotion);
		ImGui::SetNextItemWidth(100.0f);
		if (ImGui::Combo("##LinearX", &linearX, linearMotionTypes, IM_ARRAYSIZE(linearMotionTypes)))
		{
			CI.LinearXMotion = static_cast<ELinearConstraintMotion>(linearX);
			bChanged = true;
		}

		// Linear Y Motion
		ImGui::Text("Y축:");
		ImGui::SameLine(120.0f);
		int linearY = static_cast<int>(CI.LinearYMotion);
		ImGui::SetNextItemWidth(100.0f);
		if (ImGui::Combo("##LinearY", &linearY, linearMotionTypes, IM_ARRAYSIZE(linearMotionTypes)))
		{
			CI.LinearYMotion = static_cast<ELinearConstraintMotion>(linearY);
			bChanged = true;
		}

		// Linear Z Motion
		ImGui::Text("Z축:");
		ImGui::SameLine(120.0f);
		int linearZ = static_cast<int>(CI.LinearZMotion);
		ImGui::SetNextItemWidth(100.0f);
		if (ImGui::Combo("##LinearZ", &linearZ, linearMotionTypes, IM_ARRAYSIZE(linearMotionTypes)))
		{
			CI.LinearZMotion = static_cast<ELinearConstraintMotion>(linearZ);
			bChanged = true;
		}

		ImGui::Spacing();

		// Linear Limit Distance (3축 모두 Locked이면 비활성화)
		bool bAllLocked = (CI.LinearXMotion == ELinearConstraintMotion::Locked) &&
		                  (CI.LinearYMotion == ELinearConstraintMotion::Locked) &&
		                  (CI.LinearZMotion == ELinearConstraintMotion::Locked);

		ImGui::Text("제한 거리:");
		ImGui::SameLine(120.0f);
		ImGui::SetNextItemWidth(-1);

		if (bAllLocked)
		{
			ImGui::BeginDisabled();
		}

		if (ImGui::InputFloat("##LinearLimit", &CI.LinearLimit, 0.1f, 1.0f, "%.1f"))
		{
			if (CI.LinearLimit < 0.0f) CI.LinearLimit = 0.0f;
			bChanged = true;
		}

		if (bAllLocked)
		{
			ImGui::EndDisabled();
		}

		ImGui::Unindent(10.0f);
	}

	// 변경 플래그 설정
	if (bChanged)
	{
		State->bIsDirty = true;
		State->bSelectedConstraintLineDirty = true;  // 선택된 컨스트레인트만 업데이트
	}
}

bool SPhysicsAssetEditorWindow::RenderShapeDetails(USkeletalBodySetup* Body)
{
	PhysicsAssetEditorState* State = GetActivePhysicsState();
	if (!State) return false;

	bool bChanged = false;
	FKAggregateGeom& AggGeom = Body->AggGeom;

	// Sphere Elements
	for (int32 i = 0; i < AggGeom.SphereElems.Num(); ++i)
	{
		FKSphereElem& Elem = AggGeom.SphereElems[i];
		ImGui::PushID(("Sphere_" + std::to_string(i)).c_str());

		bool bOpen = ImGui::TreeNodeEx(("구 " + std::to_string(i)).c_str(), ImGuiTreeNodeFlags_DefaultOpen);

		// 우클릭 컨텍스트 메뉴
		if (ImGui::BeginPopupContextItem())
		{
			if (ImGui::MenuItem("삭제"))
			{
				AggGeom.RemoveSphereElem(i);
				bChanged = true;
				ImGui::EndPopup();
				ImGui::PopID();
				if (bOpen) ImGui::TreePop();
				break;
			}
			ImGui::EndPopup();
		}

		if (bOpen)
		{
			if (ImGui::DragFloat3("중앙", &Elem.Center.X, 0.1f)) bChanged = true;
			if (ImGui::DragFloat("반경", &Elem.Radius, 0.1f, 0.1f, 1000.0f)) bChanged = true;
			ImGui::TreePop();
		}
		ImGui::PopID();
	}

	// Box Elements
	for (int32 i = 0; i < AggGeom.BoxElems.Num(); ++i)
	{
		FKBoxElem& Elem = AggGeom.BoxElems[i];
		ImGui::PushID(("Box_" + std::to_string(i)).c_str());

		bool bOpen = ImGui::TreeNodeEx(("박스 " + std::to_string(i)).c_str(), ImGuiTreeNodeFlags_DefaultOpen);

		// 우클릭 컨텍스트 메뉴
		if (ImGui::BeginPopupContextItem())
		{
			if (ImGui::MenuItem("삭제"))
			{
				AggGeom.RemoveBoxElem(i);
				bChanged = true;
				ImGui::EndPopup();
				ImGui::PopID();
				if (bOpen) ImGui::TreePop();
				break;
			}
			ImGui::EndPopup();
		}

		if (bOpen)
		{
			if (ImGui::DragFloat3("중앙", &Elem.Center.X, 0.1f)) bChanged = true;

			// Rotation을 Euler로 변환하여 편집
			FVector euler = Elem.Rotation.ToEulerZYXDeg();
			if (ImGui::DragFloat3("회전", &euler.X, 1.0f))
			{
				Elem.Rotation = FQuat::MakeFromEulerZYX(euler);
				bChanged = true;
			}

			// Half Extent
			FVector halfExtent(Elem.X, Elem.Y, Elem.Z);
			if (ImGui::DragFloat3("반경", &halfExtent.X, 0.1f, 0.1f, 1000.0f))
			{
				Elem.X = halfExtent.X;
				Elem.Y = halfExtent.Y;
				Elem.Z = halfExtent.Z;
				bChanged = true;
			}
			ImGui::TreePop();
		}
		ImGui::PopID();
	}

	// Capsule (Sphyl) Elements
	for (int32 i = 0; i < AggGeom.SphylElems.Num(); ++i)
	{
		FKSphylElem& Elem = AggGeom.SphylElems[i];
		ImGui::PushID(("Capsule_" + std::to_string(i)).c_str());

		bool bOpen = ImGui::TreeNodeEx(("캡슐 " + std::to_string(i)).c_str(), ImGuiTreeNodeFlags_DefaultOpen);

		// 우클릭 컨텍스트 메뉴
		if (ImGui::BeginPopupContextItem())
		{
			if (ImGui::MenuItem("삭제"))
			{
				AggGeom.RemoveSphylElem(i);
				bChanged = true;
				ImGui::EndPopup();
				ImGui::PopID();
				if (bOpen) ImGui::TreePop();
				break;
			}
			ImGui::EndPopup();
		}

		if (bOpen)
		{
			if (ImGui::DragFloat3("중앙", &Elem.Center.X, 0.1f)) bChanged = true;

			// Rotation을 Euler로 변환하여 편집
			FVector euler = Elem.Rotation.ToEulerZYXDeg();
			if (ImGui::DragFloat3("회전", &euler.X, 1.0f))
			{
				Elem.Rotation = FQuat::MakeFromEulerZYX(euler);
				bChanged = true;
			}

			if (ImGui::DragFloat("반경", &Elem.Radius, 0.1f, 0.1f, 1000.0f)) bChanged = true;
			if (ImGui::DragFloat("길이", &Elem.Length, 0.1f, 0.0f, 1000.0f)) bChanged = true;
			ImGui::TreePop();
		}
		ImGui::PopID();
	}

	// Shape 추가 버튼
	ImGui::Spacing();
	ImGui::Separator();
	ImGui::Text("프리미티브 추가");
	if (ImGui::Button("+ 구")) { Body->AddSphereElem(5.0f); bChanged = true; }
	ImGui::SameLine();
	if (ImGui::Button("+ 박스")) { Body->AddBoxElem(5.0f, 5.0f, 5.0f); bChanged = true; }
	ImGui::SameLine();
	if (ImGui::Button("+ 캡슐")) { Body->AddCapsuleElem(3.0f, 10.0f); bChanged = true; }

	if (bChanged)
	{
		State->bIsDirty = true;
		State->bSelectedBodyLineDirty = true;  // 선택 바디 라인만 재생성
	}

	return bChanged;
}

void SPhysicsAssetEditorWindow::AddBodyToBone(int32 BoneIndex, int32 ShapeType)
{
	PhysicsAssetEditorState* State = GetActivePhysicsState();
	if (!State) return;

	// 프리뷰 액터에서 스켈레탈 메시 가져오기
	USkeletalMesh* Mesh = nullptr;
	if (State->PreviewActor)
	{
		if (USkeletalMeshComponent* MeshComp = State->PreviewActor->GetSkeletalMeshComponent())
		{
			Mesh = MeshComp->GetSkeletalMesh();
		}
	}
	if (!Mesh) return;

	UPhysicsAsset* PhysAsset = State->EditingPhysicsAsset;
	if (!PhysAsset)
	{
		// PhysicsAsset이 없으면 새로 생성
		PhysAsset = new UPhysicsAsset();
		State->EditingPhysicsAsset = PhysAsset;
	}

	const FSkeleton* Skeleton = Mesh->GetSkeleton();
	if (!Skeleton) return;

	const TArray<FBone>& Bones = Skeleton->Bones;
	if (BoneIndex < 0 || BoneIndex >= (int32)Bones.Num()) return;

	const FBone& Bone = Bones[BoneIndex];

	// 새 BodySetup 생성
	USkeletalBodySetup* NewBody = new USkeletalBodySetup();
	NewBody->BoneName = Bone.Name;
	NewBody->PhysicsType = EPhysicsType::Default;
	NewBody->CollisionResponse = EBodyCollisionResponse::BodyCollision_Enabled;

	// Shape 추가 (기본 크기)
	switch (ShapeType)
	{
	case 0: // Sphere
		NewBody->AddSphereElem(5.0f);
		break;
	case 1: // Box
		NewBody->AddBoxElem(5.0f, 5.0f, 5.0f);
		break;
	case 2: // Capsule
		NewBody->AddCapsuleElem(3.0f, 10.0f);
		break;
	}

	// PhysicsAsset에 추가
	int32 NewIndex = PhysAsset->AddBodySetup(NewBody);

	// 선택 상태 업데이트
	State->GraphRootBodyIndex = NewIndex;  // 새 바디를 그래프 중심으로
	State->SelectedBodyIndex = NewIndex;
	State->SelectedBoneIndex = -1;
	State->SelectedConstraintIndex = -1;
	State->bIsDirty = true;

	// 바디 추가 시 캐시 및 전체 라인 재생성
	State->bBoneTMCacheDirty = true;
	State->bAllBodyLinesDirty = true;
	State->bSelectedBodyLineDirty = true;
}

void SPhysicsAssetEditorWindow::RemoveBody(int32 BodyIndex)
{
	PhysicsAssetEditorState* State = GetActivePhysicsState();
	if (!State) return;

	UPhysicsAsset* PhysAsset = State->EditingPhysicsAsset;
	if (!PhysAsset) return;

	if (BodyIndex < 0 || BodyIndex >= PhysAsset->GetBodySetupCount()) return;

	// 바디 제거
	PhysAsset->RemoveBodySetup(BodyIndex);

	// 선택 상태 초기화
	State->GraphRootBodyIndex = -1;
	State->SelectedBodyIndex = -1;
	State->SelectedConstraintIndex = -1;
	State->bIsDirty = true;

	// 바디 삭제 시 캐시 및 전체 라인 재생성
	State->bBoneTMCacheDirty = true;
	State->bAllBodyLinesDirty = true;
	State->bSelectedBodyLineDirty = true;
}

void SPhysicsAssetEditorWindow::RenderToolPanel()
{
	PhysicsAssetEditorState* State = GetActivePhysicsState();
	if (!State) return;

	// 레이아웃 설정
	const float LabelWidth = 155.0f;  // 라벨 너비 증가 (긴 한글 텍스트 대응)
	const float ContentWidth = ImGui::GetContentRegionAvail().x;
	const float ControlWidth = ContentWidth - LabelWidth;
	const float ControlStartX = LabelWidth;
	const float ControlEndX = ContentWidth - 5.0f;

	// PropertyRow 헬퍼 람다: 라벨 좌측, 컨트롤 우측 (통일된 정렬)
	auto PropertyRow = [&](const char* label) {
		ImGui::AlignTextToFramePadding();
		ImGui::SetWindowFontScale(0.95f);  // 살짝 작은 폰트
		ImGui::Text("%s", label);
		ImGui::SetWindowFontScale(1.0f);
		ImGui::SameLine(ControlStartX);
		ImGui::SetNextItemWidth(ControlEndX - ControlStartX);
	};

	// CheckboxRow 헬퍼 람다: 라벨 좌측, 체크박스 우측 (동일 정렬선)
	auto CheckboxRow = [&](const char* label, const char* id, bool* value) {
		ImGui::AlignTextToFramePadding();
		ImGui::SetWindowFontScale(0.95f);
		ImGui::Text("%s", label);
		ImGui::SetWindowFontScale(1.0f);
		ImGui::SameLine(ControlStartX);
		// 컨트롤 영역 내 우측 정렬 (다른 컨트롤과 끝점 맞춤)
		float checkboxWidth = ImGui::GetFrameHeight();
		ImGui::SetCursorPosX(ControlEndX - checkboxWidth);
		ImGui::Checkbox(id, value);
	};

	// === 바디 생성 섹션 ===
	if (ImGui::TreeNodeEx("바디 생성", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed))
	{
		ImGui::Spacing();
		ImGui::Indent(5.0f);

		// 최소 본 크기
		PropertyRow("최소 본 크기");
		float minBoneSize = State->ToolMinBoneSize;
		if (ImGui::DragFloat("##MinBoneSize", &minBoneSize, 0.1f, 0.1f, 100.0f, "%.2f"))
		{
			State->ToolMinBoneSize = minBoneSize;
		}

		// 프리미티브 타입
		PropertyRow("프리미티브 타입");
		const char* geomTypes[] = { "Sphere", "Box", "Capsule" };
		ImGui::Combo("##GeomType", &State->ToolGeomType, geomTypes, IM_ARRAYSIZE(geomTypes));

		// 바디 크기 비율
		PropertyRow("바디 크기 비율");
		float scalePercent = State->ToolBodySizeScale * 100.0f;
		if (ImGui::DragFloat("##BodySizeScale", &scalePercent, 1.0f, 50.0f, 100.0f, "%.0f%%"))
		{
			State->ToolBodySizeScale = scalePercent / 100.0f;
		}

		// 모든 본에 바디 생성
		CheckboxRow("모든 본에 바디 생성", "##BodyForAll", &State->bToolBodyForAll);

		ImGui::Unindent(5.0f);
		ImGui::Spacing();
		ImGui::TreePop();
	}

	// === 컨스트레인트 생성 섹션 ===
	if (ImGui::TreeNodeEx("컨스트레인트 생성", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed))
	{
		ImGui::Spacing();
		ImGui::Indent(5.0f);

		// 컨스트레인트 생성 여부
		CheckboxRow("컨스트레인트 생성", "##CreateConstraints", &State->bToolCreateConstraints);

		// 비활성화 영역 시작
		ImGui::BeginDisabled(!State->bToolCreateConstraints);
		{
			// 각 컨스트레인트 모드
			PropertyRow("컨스트레인트 모드");
			const char* angularModes[] = { "Free", "Limited", "Locked" };
			ImGui::Combo("##AngularMode", &State->ToolAngularMode, angularModes, IM_ARRAYSIZE(angularModes));

			// Limited 모드일 때만 각도 설정 표시
			if (State->ToolAngularMode == 1)
			{
				PropertyRow("Swing 제한 (도)");
				ImGui::DragFloat("##SwingLimit", &State->ToolSwingLimit, 1.0f, 0.0f, 180.0f, "%.1f");

				PropertyRow("Twist 제한 (도)");
				ImGui::DragFloat("##TwistLimit", &State->ToolTwistLimit, 1.0f, 0.0f, 180.0f, "%.1f");
			}
		}
		ImGui::EndDisabled();

		ImGui::Unindent(5.0f);
		ImGui::Spacing();
		ImGui::TreePop();
	}

	// === 하단 버튼 영역 ===
	ImGui::Spacing();
	ImGui::Spacing();

	// 버튼 두 개를 한 줄에 배치
	float buttonWidth = (ContentWidth - 10.0f) * 0.5f;

	// "모든 바디 생성" 버튼 (파란색)
	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.4f, 0.8f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.5f, 0.9f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.15f, 0.35f, 0.7f, 1.0f));
	if (ImGui::Button("모든 바디 생성", ImVec2(buttonWidth, 25)))
	{
		CreateAllBodies(State->ToolGeomType);
	}
	ImGui::PopStyleColor(3);

	ImGui::SameLine();

	// "모든 바디 삭제" 버튼 (빨간색)
	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.2f, 0.2f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.85f, 0.3f, 0.3f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.55f, 0.15f, 0.15f, 1.0f));
	if (ImGui::Button("모든 바디 삭제", ImVec2(buttonWidth, 25)))
	{
		RemoveAllBodies();
	}
	ImGui::PopStyleColor(3);
}

void SPhysicsAssetEditorWindow::StartSimulation()
{
	PhysicsAssetEditorState* State = GetActivePhysicsState();
	if (!State || State->bIsSimulating)
	{
		return;
	}

	UPhysicsAsset* PhysAsset = State->EditingPhysicsAsset;
	if (!PhysAsset || PhysAsset->GetBodySetupCount() == 0)
	{
		return;
	}

	// 스켈레탈 메시 컴포넌트 가져오기
	USkeletalMeshComponent* MeshComp = nullptr;
	USkeletalMesh* Mesh = nullptr;
	if (State->PreviewActor)
	{
		MeshComp = State->PreviewActor->GetSkeletalMeshComponent();
		if (MeshComp)
		{
			Mesh = MeshComp->GetSkeletalMesh();
		}
	}
	if (!MeshComp || !Mesh)
	{
		return;
	}

	const FSkeleton* Skeleton = Mesh->GetSkeleton();
	if (!Skeleton)
	{
		return;
	}

	// === 1. 원본 본 트랜스폼 저장 (리셋용) ===
	State->OriginalBoneTransforms.Empty();
	State->OriginalBoneTransforms.SetNum(Skeleton->Bones.Num());
	for (int32 i = 0; i < (int32)Skeleton->Bones.Num(); ++i)
	{
		State->OriginalBoneTransforms[i] = MeshComp->GetBoneWorldTransform(i);
	}

	// === 2. PhysicsScene 생성 ===
	FPhysicsSystem* PhysSystem = GEngine.GetPhysicsSystem();
	if (!PhysSystem)
	{
		return;
	}

	State->SimulationScene = new FPhysicsScene();
	State->SimulationScene->Initialize(PhysSystem);

	// === 2-1. 바닥 평면 생성 ===
	PxPhysics* Physics = PhysSystem->GetPhysics();
	if (Physics && State->SimulationScene->GetPxScene())
	{
		// Y-Up 평면 (PhysX 기본 좌표계)
		PxPlane groundPlane(0.0f, 1.0f, 0.0f, 0.1f);  // normal=(0,1,0), distance=0
		PxMaterial* DefaultMaterial = Physics->createMaterial(0.5f, 0.5f, 0.1f);  // 정적마찰, 동적마찰, 반발계수

		State->GroundPlane = PxCreatePlane(*Physics, groundPlane, *DefaultMaterial);
		if (State->GroundPlane)
		{
			State->SimulationScene->GetPxScene()->addActor(*State->GroundPlane);
		}
	}

	// === 3. 각 BodySetup에 대해 FBodyInstance 생성 ===
	State->SimulatedBodies.Empty();
	int32 BodyCount = PhysAsset->GetBodySetupCount();
	for (int32 BodyIdx = 0; BodyIdx < BodyCount; ++BodyIdx)
	{
		USkeletalBodySetup* BodySetup = PhysAsset->GetBodySetup(BodyIdx);
		if (!BodySetup)
		{
			continue;
		}

		// 본 인덱스 찾기
		auto it = Skeleton->BoneNameToIndex.find(BodySetup->BoneName.ToString());
		if (it == Skeleton->BoneNameToIndex.end())
		{
			continue;
		}
		int32 BoneIndex = it->second;

		// 본 월드 트랜스폼 가져오기
		FTransform BoneTM = MeshComp->GetBoneWorldTransform(BoneIndex);

		// FBodyInstance 생성 및 초기화
		FBodyInstance* Body = new FBodyInstance();
		Body->BoneName = BodySetup->BoneName;
		Body->BoneIndex = BoneIndex;
		Body->InitBody(BodySetup, BoneTM, nullptr, State->SimulationScene);
		Body->SetSimulatePhysics(true);
		Body->SetEnableGravity(true);

		State->SimulatedBodies.Add(Body);
	}

	// === 4. 각 Constraint에 대해 Joint 생성 ===
	State->SimulatedJoints.Empty();
	int32 ConstraintCount = PhysAsset->GetConstraintCount();

	for (int32 ConstraintIdx = 0; ConstraintIdx < ConstraintCount; ++ConstraintIdx)
	{
		UPhysicsConstraintTemplate* ConstraintTemplate = PhysAsset->ConstraintSetup[ConstraintIdx];
		if (!ConstraintTemplate)
		{
			continue;
		}

		FConstraintInstance& Instance = ConstraintTemplate->DefaultInstance;

		// Bone1, Bone2에 해당하는 FBodyInstance 찾기
		FBodyInstance* Body1 = nullptr;
		FBodyInstance* Body2 = nullptr;

		for (FBodyInstance* Body : State->SimulatedBodies)
		{
			if (Body->BoneName == Instance.ConstraintBone1)
			{
				Body1 = Body;
			}
			if (Body->BoneName == Instance.ConstraintBone2)
			{
				Body2 = Body;
			}
		}

		if (!Body1 || !Body2)
		{
			continue;
		}

		// Joint 생성
		FConstraintInstance* RuntimeConstraint = new FConstraintInstance(Instance);
		RuntimeConstraint->InitConstraint(Body1, Body2, nullptr);

		if (RuntimeConstraint->PxJoint)
		{
			State->SimulatedJoints.Add(RuntimeConstraint->PxJoint);
		}
		delete RuntimeConstraint;  // PxJoint는 PhysX가 소유
	}

	// === 5. 시뮬레이션 시작 ===
	State->bIsSimulating = true;
	State->SimulationLeftoverTime = 0.0f;

	// === 6. 디버그 라인 비활성화 (시뮬레이션 중에는 표시 안 함) ===
	State->bShowBodies = false;
	State->bShowConstraints = false;
	State->bShowBones = false;

	// LineComponent 가시성 끄기
	if (State->BodyShapeLineComponent)
	{
		State->BodyShapeLineComponent->SetLineVisible(false);
	}
	if (State->SelectedBodyLineComponent)
	{
		State->SelectedBodyLineComponent->SetLineVisible(false);
	}
	if (State->ConstraintLineComponent)
	{
		State->ConstraintLineComponent->SetLineVisible(false);
	}
	if (State->SelectedConstraintLineComponent)
	{
		State->SelectedConstraintLineComponent->SetLineVisible(false);
	}
	if (State->PreviewActor)
	{
		if (ULineComponent* BoneLineComp = State->PreviewActor->GetBoneLineComponent())
		{
			BoneLineComp->SetLineVisible(false);
		}
	}

	UE_LOG("[PhysicsAssetEditor] 시뮬레이션 시작: %d 바디, %d 조인트",
		State->SimulatedBodies.Num(), State->SimulatedJoints.Num());
}

void SPhysicsAssetEditorWindow::StopSimulation()
{
	PhysicsAssetEditorState* State = GetActivePhysicsState();
	if (!State || !State->bIsSimulating)
	{
		return;
	}

	// === 1. Joint 정리 (PhysX가 소유하므로 release만 호출) ===
	for (PxJoint* Joint : State->SimulatedJoints)
	{
		if (Joint)
		{
			Joint->release();
		}
	}
	State->SimulatedJoints.Empty();

	// === 2. Body 정리 ===
	for (FBodyInstance* Body : State->SimulatedBodies)
	{
		if (Body)
		{
			Body->TermBody();
			delete Body;
		}
	}
	State->SimulatedBodies.Empty();

	// === 2-1. 바닥 평면 정리 ===
	if (State->GroundPlane)
	{
		State->GroundPlane->release();
		State->GroundPlane = nullptr;
	}

	// === 3. PhysicsScene 정리 ===
	if (State->SimulationScene)
	{
		State->SimulationScene->Shutdown();
		delete State->SimulationScene;
		State->SimulationScene = nullptr;
	}

	State->bIsSimulating = false;

	// 디버그 라인 재활성화 (바디, 컨스트레인트만 - 본은 제외)
	State->bShowBodies = true;
	State->bShowConstraints = true;
	// State->bShowBones는 false 유지

	// LineComponent 가시성 켜기 (바디, 컨스트레인트만)
	if (State->BodyShapeLineComponent)
	{
		State->BodyShapeLineComponent->SetLineVisible(true);
	}
	if (State->SelectedBodyLineComponent)
	{
		State->SelectedBodyLineComponent->SetLineVisible(true);
	}
	if (State->ConstraintLineComponent)
	{
		State->ConstraintLineComponent->SetLineVisible(true);
	}
	if (State->SelectedConstraintLineComponent)
	{
		State->SelectedConstraintLineComponent->SetLineVisible(true);
	}

	// 즉시 라인 재구성 (다음 프레임 지연 방지)
	RebuildBoneTMCache();
	RebuildUnselectedBodyLines();
	RebuildSelectedBodyLines();
	RebuildUnselectedConstraintLines();
	RebuildSelectedConstraintLines();

	UE_LOG("[PhysicsAssetEditor] 시뮬레이션 중지");
}

void SPhysicsAssetEditorWindow::ResetPose()
{
	PhysicsAssetEditorState* State = GetActivePhysicsState();
	if (!State)
	{
		return;
	}

	// 시뮬레이션 중이면 먼저 중지
	if (State->bIsSimulating)
	{
		StopSimulation();
	}

	// 스켈레탈 메시 컴포넌트 가져오기
	USkeletalMeshComponent* MeshComp = nullptr;
	if (State->PreviewActor)
	{
		MeshComp = State->PreviewActor->GetSkeletalMeshComponent();
	}
	if (!MeshComp)
	{
		return;
	}

	// 원본 포즈가 저장되어 있으면 복원
	if (State->OriginalBoneTransforms.Num() > 0)
	{
		for (int32 i = 0; i < State->OriginalBoneTransforms.Num(); ++i)
		{
			MeshComp->SetBoneWorldTransform(i, State->OriginalBoneTransforms[i]);
		}
		State->OriginalBoneTransforms.Empty();  // 사용 후 정리
	}

	// 디버그 라인 모두 재활성화 (본 포함)
	State->bShowBodies = true;
	State->bShowConstraints = true;
	State->bShowBones = true;

	// LineComponent 가시성 모두 켜기
	if (State->BodyShapeLineComponent)
	{
		State->BodyShapeLineComponent->SetLineVisible(true);
	}
	if (State->SelectedBodyLineComponent)
	{
		State->SelectedBodyLineComponent->SetLineVisible(true);
	}
	if (State->ConstraintLineComponent)
	{
		State->ConstraintLineComponent->SetLineVisible(true);
	}
	if (State->SelectedConstraintLineComponent)
	{
		State->SelectedConstraintLineComponent->SetLineVisible(true);
	}
	if (State->PreviewActor)
	{
		if (ULineComponent* BoneLineComp = State->PreviewActor->GetBoneLineComponent())
		{
			BoneLineComp->SetLineVisible(true);
		}
	}

	// 즉시 라인 재구성 (다음 프레임 지연 방지)
	RebuildBoneTMCache();
	RebuildUnselectedBodyLines();
	RebuildSelectedBodyLines();
	RebuildUnselectedConstraintLines();
	RebuildSelectedConstraintLines();
	if (State->PreviewActor)
	{
		State->PreviewActor->RebuildBoneLines(State->SelectedBoneIndex);
	}

	UE_LOG("[PhysicsAssetEditor] 포즈 리셋");
}

void SPhysicsAssetEditorWindow::TickSimulation(float DeltaTime)
{
	PhysicsAssetEditorState* State = GetActivePhysicsState();
	if (!State || !State->bIsSimulating)
	{
		return;
	}
	if (!State->SimulationScene)
	{
		return;
	}

	// 스켈레탈 메시 컴포넌트 가져오기
	USkeletalMeshComponent* MeshComp = nullptr;
	if (State->PreviewActor)
	{
		MeshComp = State->PreviewActor->GetSkeletalMeshComponent();
	}
	if (!MeshComp)
	{
		return;
	}

	// === 1. 명령 큐 처리 ===
	State->SimulationScene->ProcessCommandQueue();

	// === 2. 물리 시뮬레이션 스텝 (내부적으로 고정 시간 스텝 처리) ===
	State->SimulationScene->Simulation(DeltaTime);

	// === 3. 결과 동기화 (fetchResults 호출) ===
	State->SimulationScene->FetchAndSync();

	// === 4. 바디 트랜스폼을 본에 동기화 ===
	for (FBodyInstance* Body : State->SimulatedBodies)
	{
		if (!Body)
		{
			continue;
		}

		// 물리 바디의 월드 트랜스폼 가져오기
		FTransform BodyTM = Body->GetWorldTransform();

		// 해당 본에 트랜스폼 적용
		int32 BoneIndex = Body->BoneIndex;
		if (BoneIndex >= 0)
		{
			MeshComp->SetBoneWorldTransform(BoneIndex, BodyTM);
		}
	}

	// === 5. 시각화 업데이트 플래그 설정 ===
	State->bBoneTMCacheDirty = true;
	State->bAllBodyLinesDirty = true;
	State->bSelectedBodyLineDirty = true;
	State->bAllConstraintLinesDirty = true;
	State->bSelectedConstraintLineDirty = true;
	State->bBoneLinesDirty = true;
}

void SPhysicsAssetEditorWindow::CreateAllBodies(int32 ShapeType)
{
	PhysicsAssetEditorState* State = GetActivePhysicsState();
	if (!State) return;

	// 스켈레탈 메시 가져오기
	USkeletalMesh* Mesh = nullptr;
	USkeletalMeshComponent* MeshComp = nullptr;
	if (State->PreviewActor)
	{
		MeshComp = State->PreviewActor->GetSkeletalMeshComponent();
		if (MeshComp)
		{
			Mesh = MeshComp->GetSkeletalMesh();
		}
	}
	if (!Mesh || !MeshComp) return;

	// PhysicsAsset 확인/생성
	UPhysicsAsset* PhysAsset = State->EditingPhysicsAsset;
	if (!PhysAsset)
	{
		PhysAsset = new UPhysicsAsset();
		State->EditingPhysicsAsset = PhysAsset;
	}

	// 옵션 설정
	FPhysicsAssetCreateOptions Options;
	Options.ShapeType = ShapeType;
	Options.BodySizeScale = State->ToolBodySizeScale;
	Options.bBodyForAll = State->bToolBodyForAll;
	Options.MinBoneSize = State->ToolMinBoneSize;
	Options.bCreateConstraints = State->bToolCreateConstraints;
	Options.AngularMode = State->ToolAngularMode;
	Options.SwingLimit = State->ToolSwingLimit;
	Options.TwistLimit = State->ToolTwistLimit;

	// 바디 생성 (유틸리티 함수 사용)
	int32 BodyCount = FPhysicsAssetUtils::CreateAllBodies(PhysAsset, Mesh, MeshComp, Options);

	// 컨스트레인트 자동 생성
	int32 ConstraintCount = 0;
	if (Options.bCreateConstraints)
	{
		ConstraintCount = FPhysicsAssetUtils::CreateConstraintsForBodies(PhysAsset, Mesh, Options);
	}

	// 상태 업데이트
	State->GraphRootBodyIndex = -1;
	State->SelectedBodyIndex = -1;
	State->SelectedConstraintIndex = -1;
	State->SelectedBoneIndex = -1;
	State->bIsDirty = true;
	State->bBoneTMCacheDirty = true;
	State->bAllBodyLinesDirty = true;
	State->bSelectedBodyLineDirty = true;
	State->bAllConstraintLinesDirty = true;
	State->bSelectedConstraintLineDirty = true;

	UE_LOG("[PhysicsAssetEditor] 모든 바디 생성 완료: %d개 바디, %d개 컨스트레인트",
		BodyCount, ConstraintCount);
}

void SPhysicsAssetEditorWindow::RemoveAllBodies()
{
	PhysicsAssetEditorState* State = GetActivePhysicsState();
	if (!State) return;

	UPhysicsAsset* PhysAsset = State->EditingPhysicsAsset;
	if (!PhysAsset) return;

	// 유틸리티 함수 사용
	FPhysicsAssetUtils::RemoveAllBodiesAndConstraints(PhysAsset);

	// 상태 업데이트
	State->GraphRootBodyIndex = -1;
	State->SelectedBodyIndex = -1;
	State->SelectedConstraintIndex = -1;
	State->bIsDirty = true;
	State->bBoneTMCacheDirty = true;
	State->bAllBodyLinesDirty = true;
	State->bSelectedBodyLineDirty = true;
	State->bAllConstraintLinesDirty = true;
	State->bSelectedConstraintLineDirty = true;

	UE_LOG("[PhysicsAssetEditor] 모든 바디 및 컨스트레인트 삭제됨");
}

void SPhysicsAssetEditorWindow::AutoCreateConstraints()
{
	PhysicsAssetEditorState* State = GetActivePhysicsState();
	if (!State) return;

	UPhysicsAsset* PhysAsset = State->EditingPhysicsAsset;
	if (!PhysAsset) return;

	// 스켈레탈 메시 가져오기
	USkeletalMesh* Mesh = nullptr;
	if (State->PreviewActor)
	{
		if (USkeletalMeshComponent* MeshComp = State->PreviewActor->GetSkeletalMeshComponent())
		{
			Mesh = MeshComp->GetSkeletalMesh();
		}
	}
	if (!Mesh) return;

	// 옵션 설정
	FPhysicsAssetCreateOptions Options;
	Options.AngularMode = State->ToolAngularMode;
	Options.SwingLimit = State->ToolSwingLimit;
	Options.TwistLimit = State->ToolTwistLimit;

	// 유틸리티 함수 사용
	FPhysicsAssetUtils::CreateConstraintsForBodies(PhysAsset, Mesh, Options);
}

void SPhysicsAssetEditorWindow::RebuildBoneTMCache()
{
	PhysicsAssetEditorState* State = GetActivePhysicsState();
	if (!State) return;

	State->CachedBoneTM.Empty();

	UPhysicsAsset* PhysAsset = State->EditingPhysicsAsset;
	if (!PhysAsset) return;

	// 스켈레탈 메시 컴포넌트에서 본 Transform 가져오기
	USkeletalMeshComponent* MeshComp = nullptr;
	USkeletalMesh* Mesh = nullptr;
	if (State->PreviewActor)
	{
		MeshComp = State->PreviewActor->GetSkeletalMeshComponent();
		if (MeshComp)
		{
			Mesh = MeshComp->GetSkeletalMesh();
		}
	}

	const FSkeleton* Skeleton = Mesh ? Mesh->GetSkeleton() : nullptr;
	if (!Skeleton || !MeshComp) return;

	// 각 BodySetup의 본에 대해 BoneTM 캐싱
	int32 BodyCount = PhysAsset->GetBodySetupCount();
	for (int32 BodyIdx = 0; BodyIdx < BodyCount; ++BodyIdx)
	{
		USkeletalBodySetup* Body = PhysAsset->GetBodySetup(BodyIdx);
		if (!Body) continue;

		auto it = Skeleton->BoneNameToIndex.find(Body->BoneName.ToString());
		if (it != Skeleton->BoneNameToIndex.end())
		{
			int32 BoneIndex = it->second;
			if (BoneIndex >= 0 && BoneIndex < (int32)Skeleton->Bones.Num())
			{
				// GetBoneWorldTransform으로 회전 포함한 전체 트랜스폼 획득
				FTransform BoneTM = MeshComp->GetBoneWorldTransform(BoneIndex);
				BoneTM.Rotation.Normalize();
				State->CachedBoneTM.Add(BodyIdx, BoneTM);
			}
		}
	}

	State->bBoneTMCacheDirty = false;
}

void SPhysicsAssetEditorWindow::RebuildUnselectedBodyLines()
{
	PhysicsAssetEditorState* State = GetActivePhysicsState();
	if (!State || !State->PDI) return;

	// 비선택 바디 라인 클리어
	State->PDI->Clear();

	UPhysicsAsset* PhysAsset = State->EditingPhysicsAsset;
	if (!PhysAsset) return;

	const FLinearColor UnselectedColor(0.0f, 1.0f, 0.0f, 1.0f);    // 초록색 (비연결)
	const FLinearColor ConnectedColor(0.3f, 0.5f, 1.0f, 1.0f);     // 파란색 (연결됨)
	const float CmToM = 0.01f;
	const float Default = 1.0f;

	// 선택된 바디와 Constraint로 연결된 바디 인덱스 수집
	TSet<int32> ConnectedBodyIndices;
	if (State->SelectedBodyIndex >= 0)
	{
		USkeletalBodySetup* SelectedBody = PhysAsset->GetBodySetup(State->SelectedBodyIndex);
		if (SelectedBody)
		{
			FName SelectedBoneName = SelectedBody->BoneName;

			// 모든 Constraint를 순회하여 연결된 바디 찾기
			int32 ConstraintCount = PhysAsset->GetConstraintCount();
			for (int32 ConstraintIdx = 0; ConstraintIdx < ConstraintCount; ++ConstraintIdx)
			{
				UPhysicsConstraintTemplate* Constraint = PhysAsset->ConstraintSetup[ConstraintIdx];
				if (!Constraint) continue;

				FName Bone1 = Constraint->GetBone1Name();
				FName Bone2 = Constraint->GetBone2Name();

				// 선택된 바디의 본과 연결된 Constraint인지 확인
				if (Bone1 == SelectedBoneName)
				{
					// Bone2에 해당하는 바디 인덱스 찾기
					int32 ConnectedIdx = PhysAsset->FindBodySetupIndex(Bone2);
					if (ConnectedIdx >= 0)
					{
						ConnectedBodyIndices.Add(ConnectedIdx);
					}
				}
				else if (Bone2 == SelectedBoneName)
				{
					// Bone1에 해당하는 바디 인덱스 찾기
					int32 ConnectedIdx = PhysAsset->FindBodySetupIndex(Bone1);
					if (ConnectedIdx >= 0)
					{
						ConnectedBodyIndices.Add(ConnectedIdx);
					}
				}
			}
		}
	}

	// 모든 바디 렌더링 (선택된 바디도 포함 - 선택 라인이 위에 덧그려짐)
	int32 BodyCount = PhysAsset->GetBodySetupCount();
	for (int32 BodyIdx = 0; BodyIdx < BodyCount; ++BodyIdx)
	{
		USkeletalBodySetup* Body = PhysAsset->GetBodySetup(BodyIdx);
		if (!Body) continue;

		// 캐싱된 BoneTM 사용
		FTransform BoneTM;
		if (FTransform* CachedTM = State->CachedBoneTM.Find(BodyIdx))
		{
			BoneTM = *CachedTM;
		}

		// 연결된 바디인지에 따라 색상 선택
		const FLinearColor& DrawColor = ConnectedBodyIndices.Contains(BodyIdx) ? ConnectedColor : UnselectedColor;

		// Sphere Elements
		for (const FKSphereElem& Elem : Body->AggGeom.SphereElems)
		{
			Elem.DrawElemWire(State->PDI, BoneTM, CmToM, DrawColor);
		}

		// Box Elements
		for (const FKBoxElem& Elem : Body->AggGeom.BoxElems)
		{
			Elem.DrawElemWire(State->PDI, BoneTM, Default, DrawColor);
		}

		// Capsule (Sphyl) Elements
		for (const FKSphylElem& Elem : Body->AggGeom.SphylElems)
		{
			Elem.DrawElemWire(State->PDI, BoneTM, CmToM, DrawColor);
		}
	}

	State->bAllBodyLinesDirty = false;
}

void SPhysicsAssetEditorWindow::RebuildSelectedBodyLines()
{
	PhysicsAssetEditorState* State = GetActivePhysicsState();
	if (!State || !State->SelectedPDI) return;

	// 선택 바디 라인 클리어
	State->SelectedPDI->Clear();

	// 선택된 바디가 없으면 종료
	if (State->SelectedBodyIndex < 0)
	{
		State->bSelectedBodyLineDirty = false;
		return;
	}

	UPhysicsAsset* PhysAsset = State->EditingPhysicsAsset;
	if (!PhysAsset) return;

	USkeletalBodySetup* Body = PhysAsset->GetBodySetup(State->SelectedBodyIndex);
	if (!Body)
	{
		State->bSelectedBodyLineDirty = false;
		return;
	}

	const FLinearColor SelectedColor(1.0f, 0.8f, 0.0f, 1.0f);  // 노란색
	const float CmToM = 0.01f;
	const float Default = 1.0f;

	// 캐싱된 BoneTM 사용
	FTransform BoneTM;
	if (FTransform* CachedTM = State->CachedBoneTM.Find(State->SelectedBodyIndex))
	{
		BoneTM = *CachedTM;
	}

	// Sphere Elements
	for (const FKSphereElem& Elem : Body->AggGeom.SphereElems)
	{
		Elem.DrawElemWire(State->SelectedPDI, BoneTM, CmToM, SelectedColor);
	}

	// Box Elements
	for (const FKBoxElem& Elem : Body->AggGeom.BoxElems)
	{
		Elem.DrawElemWire(State->SelectedPDI, BoneTM, Default, SelectedColor);
	}

	// Capsule (Sphyl) Elements
	for (const FKSphylElem& Elem : Body->AggGeom.SphylElems)
	{
		Elem.DrawElemWire(State->SelectedPDI, BoneTM, CmToM, SelectedColor);
	}

	State->bSelectedBodyLineDirty = false;
}

// 단일 컨스트레인트의 시각화 라인을 그리는 헬퍼 함수
static void DrawConstraintVisualization(
	FPrimitiveDrawInterface* PDI,
	const FConstraintInstance& Instance,
	USkeletalMeshComponent* MeshComp,
	const FSkeleton* Skeleton,
	const FLinearColor& SwingColor,
	const FLinearColor& TwistColor)
{
	const float ConeLength = 0.1f;  // 콘 길이 (미터)
	const int32 Segments = 32;      // 콘 세그먼트 수

	// 자식 본 (Bone2) 위치에서 Constraint 시각화
	FName ChildBoneName = Instance.ConstraintBone2;
	auto it = Skeleton->BoneNameToIndex.find(ChildBoneName.ToString());
	if (it == Skeleton->BoneNameToIndex.end()) return;

	int32 ChildBoneIndex = it->second;
	FTransform ChildBoneTM = MeshComp->GetBoneWorldTransform(ChildBoneIndex);
	ChildBoneTM.Rotation.Normalize();

	FVector Origin = ChildBoneTM.Translation;

	// 부모 본 방향 계산 (Twist 축)
	FVector TwistAxis = FVector(1, 0, 0);  // 기본 X축
	FName ParentBoneName = Instance.ConstraintBone1;
	auto parentIt = Skeleton->BoneNameToIndex.find(ParentBoneName.ToString());
	if (parentIt != Skeleton->BoneNameToIndex.end())
	{
		int32 ParentBoneIndex = parentIt->second;
		FTransform ParentBoneTM = MeshComp->GetBoneWorldTransform(ParentBoneIndex);
		FVector ToChild = Origin - ParentBoneTM.Translation;
		if (ToChild.Size() > 0.001f)
		{
			TwistAxis = ToChild.GetNormalized();
		}
	}

	// 로컬 축 계산 (Swing1 = Y축 기반, Swing2 = Z축 기반)
	FVector Swing1Axis, Swing2Axis;
	if (FMath::Abs(TwistAxis.Z) < 0.999f)
	{
		Swing1Axis = FVector::Cross(FVector(0, 0, 1), TwistAxis).GetNormalized();
	}
	else
	{
		Swing1Axis = FVector::Cross(FVector(1, 0, 0), TwistAxis).GetNormalized();
	}
	Swing2Axis = FVector::Cross(TwistAxis, Swing1Axis).GetNormalized();

	// === Swing 콘 그리기 ===
	float Swing1Rad = DegreesToRadians(Instance.Swing1LimitAngle);
	float Swing2Rad = DegreesToRadians(Instance.Swing2LimitAngle);

	if (Instance.AngularSwing1Motion != EAngularConstraintMotion::Locked ||
		Instance.AngularSwing2Motion != EAngularConstraintMotion::Locked)
	{
		// 콘 윤곽선 (타원형)
		TArray<FVector> ConePoints;
		for (int32 i = 0; i <= Segments; ++i)
		{
			float Angle = 2.0f * PI * i / (float)Segments;
			float SwingAngle1 = Swing1Rad * cosf(Angle);
			float SwingAngle2 = Swing2Rad * sinf(Angle);
			float TotalSwing = FMath::Sqrt(SwingAngle1 * SwingAngle1 + SwingAngle2 * SwingAngle2);

			// 콘 표면 점 계산
			float CosTotalSwing = cosf(TotalSwing);
			float SinTotalSwing = sinf(TotalSwing);
			float CosAngle = cosf(Angle);
			float SinAngle = sinf(Angle);

			FVector SwingDir = Swing1Axis * CosAngle + Swing2Axis * SinAngle;
			FVector Dir = TwistAxis * CosTotalSwing + SwingDir * SinTotalSwing;
			Dir.Normalize();

			FVector Point = Origin + Dir * ConeLength;
			ConePoints.Add(Point);
		}

		// 콘 테두리 라인
		for (int32 i = 0; i < ConePoints.Num() - 1; ++i)
		{
			PDI->DrawLine(ConePoints[i], ConePoints[i + 1], SwingColor);
		}

		// 원점에서 콘 테두리로 연결선
		for (int32 i = 0; i < 8; ++i)
		{
			int32 Idx = i * Segments / 8;
			if (Idx < ConePoints.Num())
			{
				PDI->DrawLine(Origin, ConePoints[Idx], SwingColor);
			}
		}
	}

	// === Twist 아크 그리기 ===
	if (Instance.AngularTwistMotion != EAngularConstraintMotion::Locked)
	{
		float TwistRad = DegreesToRadians(Instance.TwistLimitAngle);
		float ArcRadius = ConeLength * 0.8f;  // 아크 반지름

		// Twist 아크 (콘 끝 부분에 표시)
		FVector ArcCenter = Origin + TwistAxis * (ConeLength * 0.01f);
		int32 ArcSegments = 24;

		TArray<FVector> ArcPoints;
		for (int32 i = 0; i <= ArcSegments; ++i)
		{
			float t = (float)i / (float)ArcSegments;
			float Angle = -TwistRad + 2.0f * TwistRad * t;

			float CosAngle = cosf(Angle);
			float SinAngle = sinf(Angle);
			FVector ArcDir = Swing1Axis * CosAngle + Swing2Axis * SinAngle;
			FVector Point = ArcCenter + ArcDir * ArcRadius;
			ArcPoints.Add(Point);
		}

		// 아크 라인
		for (int32 i = 0; i < ArcPoints.Num() - 1; ++i)
		{
			PDI->DrawLine(ArcPoints[i], ArcPoints[i + 1], TwistColor);
		}

		// 아크 중심에서 연결선 (4개)
		if (ArcPoints.Num() > 0)
		{
			int32 NumPoints = ArcPoints.Num();
			PDI->DrawLine(ArcCenter, ArcPoints[0], TwistColor);
			PDI->DrawLine(ArcCenter, ArcPoints[NumPoints / 3], TwistColor);
			PDI->DrawLine(ArcCenter, ArcPoints[NumPoints * 2 / 3], TwistColor);
			PDI->DrawLine(ArcCenter, ArcPoints[NumPoints - 1], TwistColor);
		}
	}
}

void SPhysicsAssetEditorWindow::RebuildUnselectedConstraintLines()
{
	PhysicsAssetEditorState* State = GetActivePhysicsState();
	if (!State || !State->ConstraintPDI) return;

	// 비선택 Constraint 라인 클리어
	State->ConstraintPDI->Clear();

	UPhysicsAsset* PhysAsset = State->EditingPhysicsAsset;
	if (!PhysAsset) return;

	// 스켈레탈 메시 정보
	USkeletalMeshComponent* MeshComp = nullptr;
	USkeletalMesh* Mesh = nullptr;
	if (State->PreviewActor)
	{
		MeshComp = State->PreviewActor->GetSkeletalMeshComponent();
		if (MeshComp)
		{
			Mesh = MeshComp->GetSkeletalMesh();
		}
	}
	const FSkeleton* Skeleton = Mesh ? Mesh->GetSkeleton() : nullptr;
	if (!Skeleton || !MeshComp) return;

	const FLinearColor SwingColor(1.0f, 0.0f, 0.0f, 1.0f);   // 빨간색 (Swing)
	const FLinearColor TwistColor(0.0f, 0.0f, 1.0f, 1.0f);   // 파란색 (Twist)

	// 모든 Constraint 렌더링 (선택된 Constraint도 포함 - 선택 라인이 위에 덧그려짐)
	int32 ConstraintCount = PhysAsset->GetConstraintCount();
	for (int32 ConstraintIdx = 0; ConstraintIdx < ConstraintCount; ++ConstraintIdx)
	{
		UPhysicsConstraintTemplate* Constraint = PhysAsset->ConstraintSetup[ConstraintIdx];
		if (!Constraint) continue;

		DrawConstraintVisualization(
			State->ConstraintPDI,
			Constraint->DefaultInstance,
			MeshComp,
			Skeleton,
			SwingColor,
			TwistColor
		);
	}

	State->bAllConstraintLinesDirty = false;
}

void SPhysicsAssetEditorWindow::RebuildSelectedConstraintLines()
{
	PhysicsAssetEditorState* State = GetActivePhysicsState();
	if (!State || !State->SelectedConstraintPDI) return;

	// 선택 Constraint 라인 클리어
	State->SelectedConstraintPDI->Clear();

	UPhysicsAsset* PhysAsset = State->EditingPhysicsAsset;
	if (!PhysAsset) return;

	// 선택된 컨스트레인트가 없으면 리턴
	if (State->SelectedConstraintIndex < 0 ||
		State->SelectedConstraintIndex >= PhysAsset->GetConstraintCount())
	{
		State->bSelectedConstraintLineDirty = false;
		return;
	}

	// 스켈레탈 메시 정보
	USkeletalMeshComponent* MeshComp = nullptr;
	USkeletalMesh* Mesh = nullptr;
	if (State->PreviewActor)
	{
		MeshComp = State->PreviewActor->GetSkeletalMeshComponent();
		if (MeshComp)
		{
			Mesh = MeshComp->GetSkeletalMesh();
		}
	}
	const FSkeleton* Skeleton = Mesh ? Mesh->GetSkeleton() : nullptr;
	if (!Skeleton || !MeshComp) return;

	// 선택된 컨스트레인트용 밝은 색상
	const FLinearColor SwingColor(1.0f, 0.5f, 0.0f, 1.0f);   // 주황색 (Swing)
	const FLinearColor TwistColor(0.0f, 1.0f, 1.0f, 1.0f);   // 시안색 (Twist)

	UPhysicsConstraintTemplate* Constraint = PhysAsset->ConstraintSetup[State->SelectedConstraintIndex];
	if (Constraint)
	{
		DrawConstraintVisualization(
			State->SelectedConstraintPDI,
			Constraint->DefaultInstance,
			MeshComp,
			Skeleton,
			SwingColor,
			TwistColor
		);
	}

	State->bSelectedConstraintLineDirty = false;
}

void SPhysicsAssetEditorWindow::SavePhysicsAsset()
{
	PhysicsAssetEditorState* State = GetActivePhysicsState();
	if (!State || !State->EditingPhysicsAsset) return;

	if (State->CurrentFilePath.empty())
	{
		SavePhysicsAssetAs();
		return;
	}

	// 저장 전 SkeletalMeshPath 동기화 (Load 시 메시 복원에 필요)
	State->EditingPhysicsAsset->SkeletalMeshPath = State->SkeletalMeshPath;

	PhysicsAssetEditorBootstrap::SavePhysicsAsset(State->EditingPhysicsAsset, State->CurrentFilePath);
	State->bIsDirty = false;
}

void SPhysicsAssetEditorWindow::SavePhysicsAssetAs()
{
	PhysicsAssetEditorState* State = GetActivePhysicsState();
	if (!State || !State->EditingPhysicsAsset) return;

	std::filesystem::path SavePath = FPlatformProcess::OpenSaveFileDialog(
		L"Data/PhysicsAssets",
		L"physicsasset",
		L"Physics Asset Files",
		L""
	);

	if (!SavePath.empty())
	{
		FString FilePath = SavePath.string();
		State->CurrentFilePath = FilePath;

		// 저장 전 SkeletalMeshPath 동기화 (Load 시 메시 복원에 필요)
		State->EditingPhysicsAsset->SkeletalMeshPath = State->SkeletalMeshPath;

		PhysicsAssetEditorBootstrap::SavePhysicsAsset(State->EditingPhysicsAsset, FilePath);
		State->bIsDirty = false;

		// 리소스 캐시 무효화 (새 에셋이 PropertyRenderer 목록에 나타나도록)
		UPropertyRenderer::ClearResourcesCache();
	}
}

void SPhysicsAssetEditorWindow::LoadPhysicsAsset()
{
	std::filesystem::path LoadPath = FPlatformProcess::OpenLoadFileDialog(
		L"Data/PhysicsAssets",
		L"physicsasset",
		L"Physics Asset Files"
	);

	if (!LoadPath.empty())
	{
		FString FilePath = LoadPath.string();
		UEditorAssetPreviewContext* NewContext = NewObject<UEditorAssetPreviewContext>();
		NewContext->ViewerType = EViewerType::PhysicsAsset;
		NewContext->AssetPath = FilePath;
		OpenOrFocusTab(NewContext);
	}
}
