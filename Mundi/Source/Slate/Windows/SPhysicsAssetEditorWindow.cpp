#include "pch.h"
#include "SPhysicsAssetEditorWindow.h"
#include "SlateManager.h"
#include "Source/Runtime/Engine/Viewer/EditorAssetPreviewContext.h"
#include "Source/Runtime/Engine/Viewer/PhysicsAssetEditorBootstrap.h"
#include "PlatformProcess.h"
#include "PhysicsAsset.h"
#include "SkeletalBodySetup.h"
#include "PhysicsConstraintTemplate.h"
#include "FViewport.h"
#include "FViewportClient.h"
#include "Source/Runtime/Engine/Components/LineComponent.h"
#include "Source/Runtime/Engine/GameFramework/SkeletalMeshActor.h"
#include "SkeletalMesh.h"
#include "ResourceManager.h"
#include "Texture.h"
#include "Widgets/PropertyRenderer.h"

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
	// 툴바 아이콘 정리
	if (IconSave)
	{
		DeleteObject(IconSave);
		IconSave = nullptr;
	}
	if (IconSaveAs)
	{
		DeleteObject(IconSaveAs);
		IconSaveAs = nullptr;
	}
	if (IconLoad)
	{
		DeleteObject(IconLoad);
		IconLoad = nullptr;
	}
	if (IconPlay)
	{
		DeleteObject(IconPlay);
		IconPlay = nullptr;
	}
	if (IconStop)
	{
		DeleteObject(IconStop);
		IconStop = nullptr;
	}
	if (IconReset)
	{
		DeleteObject(IconReset);
		IconReset = nullptr;
	}

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
	IconReset = UResourceManager::GetInstance().Load<UTexture>(GDataDir + "/Icon/Viewport_Toolbar_Rotate.png");
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

	// 이미 열린 탭이 있는지 확인
	for (int i = 0; i < Tabs.Num(); ++i)
	{
		PhysicsAssetEditorState* State = static_cast<PhysicsAssetEditorState*>(Tabs[i]);
		if (State->CurrentFilePath == Context->AssetPath)
		{
			ActiveTabIndex = i;
			ActiveState = Tabs[i];
			return;
		}
	}

	// 새 탭 생성
	FString TabName = ExtractFileNameFromPath(Context->AssetPath);
	ViewerState* NewState = CreateViewerState(TabName.c_str(), Context);
	Tabs.Add(NewState);
	ActiveTabIndex = Tabs.Num() - 1;
	ActiveState = NewState;
}

void SPhysicsAssetEditorWindow::OnUpdate(float DeltaSeconds)
{
	SViewerWindow::OnUpdate(DeltaSeconds);

	PhysicsAssetEditorState* State = GetActivePhysicsState();
	if (!State) return;

	// 시뮬레이션 업데이트
	if (State->bIsSimulating)
	{
		// TODO: Phase 10에서 구현
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
	}

	if (ImGui::Begin(UniqueTitle, &bIsOpen, flags))
	{
		// hover/focus 상태 캡처
		bIsWindowHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows);
		bIsWindowFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);

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

		ImGuiTabItemFlags tabFlags = 0;
		if (i == ActiveTabIndex)
			tabFlags |= ImGuiTabItemFlags_SetSelected;

		if (ImGui::BeginTabItem(TabId, &open, tabFlags))
		{
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

	// + 버튼으로 새 탭 추가
	if (ImGui::TabItemButton("+", ImGuiTabItemFlags_Trailing))
	{
		OpenNewTab("New Physics Asset");
	}

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
		ImGui::Text("Hierarchy");
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
		ImGui::Text("Graph");
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
		ImGui::Text("Details");
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
		ImGui::Text("Tools");
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

	// TODO: Phase 6에서 구현
	ImGui::TextDisabled("(Bone hierarchy will be shown here)");
}

void SPhysicsAssetEditorWindow::RenderGraphPanel()
{
	PhysicsAssetEditorState* State = GetActivePhysicsState();
	if (!State) return;

	// TODO: Phase 11에서 구현
	ImGui::TextDisabled("(Node graph will be shown here)");
}

void SPhysicsAssetEditorWindow::RenderDetailsPanel()
{
	PhysicsAssetEditorState* State = GetActivePhysicsState();
	if (!State) return;

	// TODO: Phase 7에서 구현
	ImGui::TextDisabled("(Property details will be shown here)");
}

void SPhysicsAssetEditorWindow::RenderToolPanel()
{
	PhysicsAssetEditorState* State = GetActivePhysicsState();
	if (!State) return;

	// TODO: Phase 8에서 구현
	ImGui::TextDisabled("(Body generation tools will be shown here)");
}

void SPhysicsAssetEditorWindow::StartSimulation()
{
	PhysicsAssetEditorState* State = GetActivePhysicsState();
	if (!State) return;

	// TODO: Phase 10에서 구현
	State->bIsSimulating = true;
}

void SPhysicsAssetEditorWindow::StopSimulation()
{
	PhysicsAssetEditorState* State = GetActivePhysicsState();
	if (!State) return;

	// TODO: Phase 10에서 구현
	State->bIsSimulating = false;
}

void SPhysicsAssetEditorWindow::ResetPose()
{
	PhysicsAssetEditorState* State = GetActivePhysicsState();
	if (!State) return;

	// TODO: Phase 10에서 구현
}

void SPhysicsAssetEditorWindow::CreateAllBodies(int32 ShapeType)
{
	// TODO: Phase 8에서 구현
}

void SPhysicsAssetEditorWindow::RemoveAllBodies()
{
	// TODO: Phase 8에서 구현
}

void SPhysicsAssetEditorWindow::AutoCreateConstraints()
{
	// TODO: Phase 8에서 구현
}

void SPhysicsAssetEditorWindow::RebuildBodyShapeLines()
{
	// TODO: Phase 9에서 구현
}

void SPhysicsAssetEditorWindow::RebuildConstraintLines()
{
	// TODO: Phase 9에서 구현
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
		PhysicsAssetEditorBootstrap::SavePhysicsAsset(State->EditingPhysicsAsset, FilePath);
		State->bIsDirty = false;
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
