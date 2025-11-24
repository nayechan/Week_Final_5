#include "pch.h"
#include "SParticleEditorWindow.h"
#include "SlateManager.h"
#include "Source/Runtime/Engine/Viewer/ParticleEditorBootstrap.h"
#include "Source/Runtime/Engine/Viewer/EditorAssetPreviewContext.h"
#include "ParticleSystem.h"
#include "ParticleEmitter.h"
#include "ParticleLODLevel.h"
#include "ParticleModule.h"
#include "ParticleSystemComponent.h"
#include "FViewport.h"
#include "FViewportClient.h"
#include "Source/Runtime/Engine/Components/LineComponent.h"

SParticleEditorWindow::SParticleEditorWindow()
{
	CenterRect = FRect(0, 0, 0, 0);
	LeftPanelRatio = 0.2f;   // 20% 왼쪽 (뷰포트 + 디테일)
	RightPanelRatio = 0.6f;  // 60% 오른쪽 (이미터 패널)
	bHasBottomPanel = true;

	// 원점축 LineComponent 생성
	OriginAxisLineComponent = NewObject<ULineComponent>();
	OriginAxisLineComponent->SetAlwaysOnTop(true); // 항상 위에 표시
	CreateOriginAxisLines();
}

SParticleEditorWindow::~SParticleEditorWindow()
{
	// 툴바 아이콘 정리
	if (IconSave)
	{
		DeleteObject(IconSave);
		IconSave = nullptr;
	}
	if (IconLoad)
	{
		DeleteObject(IconLoad);
		IconLoad = nullptr;
	}
	if (IconRestart)
	{
		DeleteObject(IconRestart);
		IconRestart = nullptr;
	}
	if (IconBounds)
	{
		DeleteObject(IconBounds);
		IconBounds = nullptr;
	}
	if (IconOriginAxis)
	{
		DeleteObject(IconOriginAxis);
		IconOriginAxis = nullptr;
	}
	if (IconBackgroundColor)
	{
		DeleteObject(IconBackgroundColor);
		IconBackgroundColor = nullptr;
	}
	if (IconLODFirst)
	{
		DeleteObject(IconLODFirst);
		IconLODFirst = nullptr;
	}
	if (IconLODPrev)
	{
		DeleteObject(IconLODPrev);
		IconLODPrev = nullptr;
	}
	if (IconLODInsertBefore)
	{
		DeleteObject(IconLODInsertBefore);
		IconLODInsertBefore = nullptr;
	}
	if (IconLODInsertAfter)
	{
		DeleteObject(IconLODInsertAfter);
		IconLODInsertAfter = nullptr;
	}
	if (IconLODDelete)
	{
		DeleteObject(IconLODDelete);
		IconLODDelete = nullptr;
	}
	if (IconLODNext)
	{
		DeleteObject(IconLODNext);
		IconLODNext = nullptr;
	}
	if (IconLODLast)
	{
		DeleteObject(IconLODLast);
		IconLODLast = nullptr;
	}

	// LineComponent 정리
	if (OriginAxisLineComponent)
	{
		DeleteObject(OriginAxisLineComponent);
		OriginAxisLineComponent = nullptr;
	}

	// 탭 정리
	for (int i = 0; i < Tabs.Num(); ++i)
	{
		ViewerState* State = Tabs[i];
		ParticleEditorBootstrap::DestroyViewerState(State);
	}
	Tabs.Empty();
	ActiveState = nullptr;
}

void SParticleEditorWindow::OnRender()
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
		RenderTabsAndToolbar(EViewerType::Particle);

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
		leftWidth = ImClamp(leftWidth, 200.f, totalWidth - 300.f);

		// 좌측 열 Render (뷰포트 + 디테일)
		ImGui::BeginChild("LeftColumn", ImVec2(leftWidth, 0), false);
		{
			RenderLeftViewportArea(totalHeight);
			RenderLeftDetailsArea();
		}
		ImGui::EndChild();

		// 수직 스플리터 (좌우 열 간)
		ImGui::SameLine();
		ImGui::Button("##VSplitter", ImVec2(4.f, -1));
		if (ImGui::IsItemActive())
		{
			float delta = ImGui::GetIO().MouseDelta.x;
			LeftPanelRatio += delta / totalWidth;
			LeftPanelRatio = ImClamp(LeftPanelRatio, 0.15f, 0.7f);
		}
		if (ImGui::IsItemHovered())
			ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);

		ImGui::SameLine();

		// 우측 열 Render (이미터 패널 + 커브 에디터)
		ImGui::BeginChild("RightColumn", ImVec2(0, 0), false);
		{
			RenderRightEmitterArea(totalHeight);
			RenderRightCurveArea();
		}
		ImGui::EndChild();
	}
	ImGui::End();
}

void SParticleEditorWindow::OnUpdate(float DeltaSeconds)
{
	SViewerWindow::OnUpdate(DeltaSeconds);
}

void SParticleEditorWindow::PreRenderViewportUpdate()
{
	ParticleEditorState* State = GetActiveParticleState();
	if (State)
	{
		// 배경색 설정 (ViewportClient에 전달)
		if (State->Client)
		{
			State->Client->SetBackgroundColor(State->BackgroundColor);
		}

		// 원점축 LineComponent 처리
		if (State->PreviewActor && OriginAxisLineComponent)
		{
			// LineComponent가 아직 PreviewActor에 연결되지 않았으면 연결
			if (OriginAxisLineComponent->GetOwner() != State->PreviewActor)
			{
				State->PreviewActor->AddOwnedComponent(OriginAxisLineComponent);
				OriginAxisLineComponent->RegisterComponent(State->World);
			}

			// bShowOriginAxis 플래그에 따라 가시성 제어
			OriginAxisLineComponent->SetLineVisible(State->bShowOriginAxis);
		}
	}
}

void SParticleEditorWindow::LoadToolbarIcons()
{
	if (!Device) return;

	// 툴바 아이콘 로드
	IconSave = NewObject<UTexture>();
	IconSave->Load(GDataDir + "/Icon/Toolbar_Save.png", Device);

	IconLoad = NewObject<UTexture>();
	IconLoad->Load(GDataDir + "/Icon/Toolbar_Load.png", Device);

	IconRestart = NewObject<UTexture>();
	IconRestart->Load(GDataDir + "/Icon/Toolbar_Restart.png", Device);

	IconBounds = NewObject<UTexture>();
	IconBounds->Load(GDataDir + "/Icon/Toolbar_Bounds.png", Device);

	IconOriginAxis = NewObject<UTexture>();
	IconOriginAxis->Load(GDataDir + "/Icon/Toolbar_OriginAxis.png", Device);

	IconBackgroundColor = NewObject<UTexture>();
	IconBackgroundColor->Load(GDataDir + "/Icon/Toolbar_BackgroundColor.png", Device);

	IconLODFirst = NewObject<UTexture>();
	IconLODFirst->Load(GDataDir + "/Icon/Toolbar_LODFirst.png", Device);

	IconLODPrev = NewObject<UTexture>();
	IconLODPrev->Load(GDataDir + "/Icon/Toolbar_LODPrev.png", Device);

	IconLODInsertBefore = NewObject<UTexture>();
	IconLODInsertBefore->Load(GDataDir + "/Icon/Toolbar_LODInsertBefore.png", Device);

	IconLODInsertAfter = NewObject<UTexture>();
	IconLODInsertAfter->Load(GDataDir + "/Icon/Toolbar_LODInsertAfter.png", Device);

	IconLODDelete = NewObject<UTexture>();
	IconLODDelete->Load(GDataDir + "/Icon/Toolbar_LODDelete.png", Device);

	IconLODNext = NewObject<UTexture>();
	IconLODNext->Load(GDataDir + "/Icon/Toolbar_LODNext.png", Device);

	IconLODLast = NewObject<UTexture>();
	IconLODLast->Load(GDataDir + "/Icon/Toolbar_LODLast.png", Device);
}

ViewerState* SParticleEditorWindow::CreateViewerState(const char* Name, UEditorAssetPreviewContext* Context)
{
	return ParticleEditorBootstrap::CreateViewerState(Name, World, Device, Context);
}

void SParticleEditorWindow::DestroyViewerState(ViewerState*& State)
{
	ParticleEditorBootstrap::DestroyViewerState(State);
}

void SParticleEditorWindow::RenderTabsAndToolbar(EViewerType CurrentViewerType)
{
	// 탭바만 렌더링 (뷰어 전환 버튼 제외)
	if (!ImGui::BeginTabBar("ParticleEditorTabs",
		ImGuiTabBarFlags_AutoSelectNewTabs | ImGuiTabBarFlags_Reorderable))
		return;

	// 탭 렌더링
	for (int i = 0; i < Tabs.Num(); ++i)
	{
		ViewerState* State = Tabs[i];
		bool open = true;
		if (ImGui::BeginTabItem(State->Name.ToString().c_str(), &open))
		{
			ActiveTabIndex = i;
			ActiveState = State;
			ImGui::EndTabItem();
		}
		if (!open)
		{
			CloseTab(i);
			ImGui::EndTabBar();
			return;
		}
	}

	// + 버튼 (새 탭 추가)
	if (ImGui::TabItemButton("+", ImGuiTabItemFlags_Trailing))
	{
		int maxViewerNum = 0;
		for (int i = 0; i < Tabs.Num(); ++i)
		{
			const FString& tabName = Tabs[i]->Name.ToString();
			const char* prefix = "Particle Editor ";
			if (strncmp(tabName.c_str(), prefix, strlen(prefix)) == 0)
			{
				const char* numberPart = tabName.c_str() + strlen(prefix);
				int num = atoi(numberPart);
				if (num > maxViewerNum)
				{
					maxViewerNum = num;
				}
			}
		}

		char label[64];
		sprintf_s(label, "Particle Editor %d", maxViewerNum + 1);
		OpenNewTab(label);
	}
	ImGui::EndTabBar();
}

void SParticleEditorWindow::RenderLeftPanel(float PanelWidth)
{
	// 더 이상 사용되지 않음 (새 레이아웃에서는 OnRender에서 직접 처리)
}

void SParticleEditorWindow::RenderRightPanel()
{
	// 이미터 패널 (나중 단계에서 구현)
	ImGui::Text("이미터 패널 (4단계에서 구현)");
}

void SParticleEditorWindow::RenderBottomPanel()
{
	// 커브 에디터 (나중 단계에서 구현)
	ImGui::Text("커브 에디터 (6단계에서 구현)");
}

void SParticleEditorWindow::RenderToolbar()
{
	ParticleEditorState* State = GetActiveParticleState();
	if (!State) return;

	const float IconSize = 36.f;
	const ImVec2 IconSizeVec(IconSize, IconSize);
	const float ToolbarHeight = 48.f;

	// 툴바 영역을 BeginChild로 감싸서 수직 구분선이 전체 높이를 차지하도록 함
	ImGui::BeginChild("##ToolbarArea", ImVec2(0, ToolbarHeight + 6.f), false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

	// 버튼 스타일
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 4));
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6, 0));
	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.2f, 0.2f, 0.2f, 0.5f));
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.3f, 0.3f, 0.3f, 0.7f));

	// 수직 정렬을 위한 커서 위치 조정
	float cursorY = (ToolbarHeight - IconSize) * 0.5f;
	ImGui::SetCursorPosY(cursorY);

	// ========== 파일 작업 ==========
	// Save 버튼
	if (IconSave && IconSave->GetShaderResourceView())
	{
		if (ImGui::ImageButton("##SavePS", (void*)IconSave->GetShaderResourceView(), IconSizeVec))
		{
			SaveParticleSystem();
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("파티클 시스템 저장");
	}

	ImGui::SameLine();

	// Load 버튼
	if (IconLoad && IconLoad->GetShaderResourceView())
	{
		if (ImGui::ImageButton("##LoadPS", (void*)IconLoad->GetShaderResourceView(), IconSizeVec))
		{
			LoadParticleSystem();
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("파티클 시스템 불러오기");
	}

	// 구분선
	ImGui::SameLine();
	ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
	ImGui::SameLine();

	// ========== 시뮬레이션 제어 ==========
	// Restart Simulation 버튼
	if (IconRestart && IconRestart->GetShaderResourceView())
	{
		if (ImGui::ImageButton("##RestartSim", (void*)IconRestart->GetShaderResourceView(), IconSizeVec))
		{
			if (State->PreviewComponent)
			{
				State->PreviewComponent->ResetParticles();
			}
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("시뮬레이션 재시작");
	}

	// 구분선
	ImGui::SameLine();
	ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
	ImGui::SameLine();

	// ========== 표시 옵션 ==========
	// Bounds 버튼
	if (IconBounds && IconBounds->GetShaderResourceView())
	{
		// 활성 상태에 따른 스타일 설정
		ImVec4 bgColor = State->bShowBounds
			? ImVec4(0.15f, 0.35f, 0.45f, 0.8f)  // 어두운 청록 배경
			: ImVec4(0, 0, 0, 0);                 // 투명
		ImVec4 tint = State->bShowBounds
			? ImVec4(0.6f, 1.0f, 1.0f, 1.0f)     // 밝은 청록 틴트
			: ImVec4(0.7f, 0.7f, 0.7f, 1.0f);    // 회색 (비활성)

		if (ImGui::ImageButton("##Bounds", (void*)IconBounds->GetShaderResourceView(), IconSizeVec, ImVec2(0, 0), ImVec2(1, 1), bgColor, tint))
		{
			State->bShowBounds = !State->bShowBounds;
		}

		// 활성 상태일 때 테두리 추가
		if (State->bShowBounds)
		{
			ImDrawList* draw_list = ImGui::GetWindowDrawList();
			ImVec2 p_min = ImGui::GetItemRectMin();
			ImVec2 p_max = ImGui::GetItemRectMax();
			ImU32 borderColor = IM_COL32(102, 204, 255, 200);
			draw_list->AddRect(p_min, p_max, borderColor, 3.0f, 0, 2.5f);
		}

		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("파티클 시스템 경계 표시");
	}

	ImGui::SameLine();

	// Origin Axis 버튼
	if (IconOriginAxis && IconOriginAxis->GetShaderResourceView())
	{
		// 활성 상태에 따른 스타일 설정
		ImVec4 bgColor = State->bShowOriginAxis
			? ImVec4(0.15f, 0.35f, 0.45f, 0.8f)  // 어두운 청록 배경
			: ImVec4(0, 0, 0, 0);                 // 투명
		ImVec4 tint = State->bShowOriginAxis
			? ImVec4(0.6f, 1.0f, 1.0f, 1.0f)     // 밝은 청록 틴트
			: ImVec4(0.7f, 0.7f, 0.7f, 1.0f);    // 회색 (비활성)

		if (ImGui::ImageButton("##OriginAxis", (void*)IconOriginAxis->GetShaderResourceView(), IconSizeVec, ImVec2(0, 0), ImVec2(1, 1), bgColor, tint))
		{
			State->bShowOriginAxis = !State->bShowOriginAxis;
		}

		// 활성 상태일 때 테두리 추가
		if (State->bShowOriginAxis)
		{
			ImDrawList* draw_list = ImGui::GetWindowDrawList();
			ImVec2 p_min = ImGui::GetItemRectMin();
			ImVec2 p_max = ImGui::GetItemRectMax();
			ImU32 borderColor = IM_COL32(102, 204, 255, 200);
			draw_list->AddRect(p_min, p_max, borderColor, 3.0f, 0, 2.5f);
		}

		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("원점 축 표시");
	}

	ImGui::SameLine();

	// Background Color 버튼
	if (IconBackgroundColor && IconBackgroundColor->GetShaderResourceView())
	{
		if (ImGui::ImageButton("##BgColor", (void*)IconBackgroundColor->GetShaderResourceView(), IconSizeVec))
		{
			ImGui::OpenPopup("BackgroundColorPopup");
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("배경색 설정");

		// 배경색 팝업
		if (ImGui::BeginPopup("BackgroundColorPopup"))
		{
			float color[3] = { State->BackgroundColor.R, State->BackgroundColor.G, State->BackgroundColor.B };
			if (ImGui::ColorPicker3("배경색", color))
			{
				State->BackgroundColor.R = color[0];
				State->BackgroundColor.G = color[1];
				State->BackgroundColor.B = color[2];
			}
			ImGui::EndPopup();
		}
	}

	// 구분선
	ImGui::SameLine();
	ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
	ImGui::SameLine();

	// LOD 네비게이션
	// 파티클 시스템의 실제 LOD 개수 가져오기
	int32 MaxLODIndex = 0;
	if (State->EditingTemplate && !State->EditingTemplate->Emitters.IsEmpty())
	{
		UParticleEmitter* Emitter = State->EditingTemplate->Emitters[0];
		MaxLODIndex = Emitter->LODLevels.Num() - 1;
		if (MaxLODIndex < 0) MaxLODIndex = 0;
	}

	// 최하 LOD 버튼
	if (IconLODFirst && IconLODFirst->GetShaderResourceView())
	{
		if (ImGui::ImageButton("##LODFirst", (void*)IconLODFirst->GetShaderResourceView(), IconSizeVec))
		{
			State->CurrentLODLevel = 0;
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("최하 LOD (LOD 0)");
	}

	ImGui::SameLine();

	// 하위 LOD 버튼
	if (IconLODPrev && IconLODPrev->GetShaderResourceView())
	{
		bool bCanGoPrev = State->CurrentLODLevel > 0;
		ImGui::BeginDisabled(!bCanGoPrev);
		if (ImGui::ImageButton("##LODPrev", (void*)IconLODPrev->GetShaderResourceView(), IconSizeVec))
		{
			if (State->CurrentLODLevel > 0)
				State->CurrentLODLevel--;
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("하위 LOD");
		ImGui::EndDisabled();
	}

	// 구분선
	ImGui::SameLine();
	ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
	ImGui::SameLine();

	// 현재 LOD 드롭다운 메뉴 (수직 중앙 정렬)
	// 드롭다운의 기본 프레임 높이를 고려하여 중앙 정렬
	float comboFrameHeight = ImGui::GetFrameHeight();  // 드롭다운 메뉴의 높이
	float lodCursorY = (ToolbarHeight - comboFrameHeight) * 0.5f;

	ImGui::SetCursorPosY(lodCursorY);
	ImGui::AlignTextToFramePadding();  // 텍스트 수직 정렬
	ImGui::Text("LOD:");

	ImGui::SameLine();
	ImGui::SetCursorPosY(lodCursorY);  // 드롭다운도 같은 Y 위치로
	ImGui::SetNextItemWidth(60.f);

	// LOD 레벨 목록을 드롭다운으로 표시
	if (State->EditingTemplate && !State->EditingTemplate->Emitters.IsEmpty())
	{
		UParticleEmitter* Emitter = State->EditingTemplate->Emitters[0];
		int32 NumLODs = Emitter->LODLevels.Num();

		if (NumLODs > 0)
		{
			// 현재 LOD 범위 체크
			State->CurrentLODLevel = ImClamp(State->CurrentLODLevel, 0, NumLODs - 1);

			// 드롭다운 표시 텍스트
			char CurrentLODText[32];
			sprintf_s(CurrentLODText, "%d", State->CurrentLODLevel);

			if (ImGui::BeginCombo("##CurrentLOD", CurrentLODText))
			{
				for (int32 i = 0; i < NumLODs; i++)
				{
					bool bIsSelected = (State->CurrentLODLevel == i);
					char LODText[32];
					sprintf_s(LODText, "%d", i);

					if (ImGui::Selectable(LODText, bIsSelected))
					{
						State->CurrentLODLevel = i;
					}

					if (bIsSelected)
						ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}
		}
		else
		{
			// LOD가 없는 경우
			ImGui::BeginDisabled(true);
			if (ImGui::BeginCombo("##CurrentLOD", "없음"))
			{
				ImGui::EndCombo();
			}
			ImGui::EndDisabled();
		}
	}
	else
	{
		// 파티클 시스템이 없는 경우
		ImGui::BeginDisabled(true);
		if (ImGui::BeginCombo("##CurrentLOD", "없음"))
		{
			ImGui::EndCombo();
		}
		ImGui::EndDisabled();
	}

	// 구분선
	ImGui::SameLine();
	ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
	ImGui::SameLine();

	// 상위 LOD 버튼
	if (IconLODNext && IconLODNext->GetShaderResourceView())
	{
		bool bCanGoNext = State->CurrentLODLevel < MaxLODIndex;
		ImGui::BeginDisabled(!bCanGoNext);
		if (ImGui::ImageButton("##LODNext", (void*)IconLODNext->GetShaderResourceView(), IconSizeVec))
		{
			if (State->CurrentLODLevel < MaxLODIndex)
				State->CurrentLODLevel++;
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("상위 LOD");
		ImGui::EndDisabled();
	}

	ImGui::SameLine();

	// 최상 LOD 버튼
	if (IconLODLast && IconLODLast->GetShaderResourceView())
	{
		if (ImGui::ImageButton("##LODLast", (void*)IconLODLast->GetShaderResourceView(), IconSizeVec))
		{
			State->CurrentLODLevel = MaxLODIndex;
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("최상 LOD (LOD %d)", MaxLODIndex);
	}

	// 구분선
	ImGui::SameLine();
	ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
	ImGui::SameLine();

	// LOD 앞에 추가 버튼
	if (IconLODInsertBefore && IconLODInsertBefore->GetShaderResourceView())
	{
		if (ImGui::ImageButton("##LODInsertBefore", (void*)IconLODInsertBefore->GetShaderResourceView(), IconSizeVec))
		{
			// TODO: 현재 LOD 앞에 새 LOD 레벨 추가 로직
			UE_LOG("Insert LOD before current level %d", State->CurrentLODLevel);
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("현재 LOD 앞에 새 LOD 추가");
	}

	ImGui::SameLine();

	// LOD 뒤로 추가 버튼
	if (IconLODInsertAfter && IconLODInsertAfter->GetShaderResourceView())
	{
		if (ImGui::ImageButton("##LODInsertAfter", (void*)IconLODInsertAfter->GetShaderResourceView(), IconSizeVec))
		{
			// TODO: 현재 LOD 뒤에 새 LOD 레벨 추가 로직
			UE_LOG("Insert LOD after current level %d", State->CurrentLODLevel);
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("현재 LOD 뒤에 새 LOD 추가");
	}

	ImGui::SameLine();

	// LOD 삭제 버튼 (맨 오른쪽으로 이동)
	if (IconLODDelete && IconLODDelete->GetShaderResourceView())
	{
		if (ImGui::ImageButton("##LODDelete", (void*)IconLODDelete->GetShaderResourceView(), IconSizeVec))
		{
			// TODO: 현재 LOD 레벨 삭제 로직
			UE_LOG("Delete LOD level %d", State->CurrentLODLevel);
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("현재 LOD 삭제");
	}

	ImGui::PopStyleColor(3);
	ImGui::PopStyleVar(2);

	ImGui::EndChild();  // ToolbarArea 종료

	ImGui::Separator();
}

void SParticleEditorWindow::RenderDetailsPanel(float PanelWidth)
{
	// 디테일 패널 (5단계에서 구현)
	ImGui::Text("디테일");
	ImGui::Separator();
	ImGui::TextDisabled("모듈을 선택하세요 (5단계에서 구현)");
}

void SParticleEditorWindow::RenderEmitterColumn(int32 EmitterIndex, UParticleEmitter* Emitter)
{
	// 이미터 열 렌더링 (4단계에서 구현)
}

void SParticleEditorWindow::RenderModuleBlock(int32 EmitterIdx, int32 ModuleIdx, UParticleModule* Module)
{
	// 모듈 블록 렌더링 (4단계에서 구현)
}

void SParticleEditorWindow::RenderViewportArea(float width, float height)
{
	ParticleEditorState* State = GetActiveParticleState();
	if (!State || !State->Viewport)
	{
		ImGui::TextDisabled("뷰포트 없음");
		return;
	}

	// CenterRect 업데이트 (뷰포트 영역)
	ImVec2 Pos = ImGui::GetCursorScreenPos();
	CenterRect.Left = Pos.x;
	CenterRect.Top = Pos.y;
	CenterRect.Right = Pos.x + width;
	CenterRect.Bottom = Pos.y + height;
	CenterRect.UpdateMinMax();

	// 뷰포트 업데이트 및 렌더링
	PreRenderViewportUpdate();
	OnRenderViewport();

	// 렌더링된 텍스처 표시
	ID3D11ShaderResourceView* ViewportSRV = State->Viewport->GetSRV();
	if (ViewportSRV)
	{
		ImVec2 ViewportSize(width, height);
		ImVec2 uv0(0, 0);
		ImVec2 uv1(1, 1);

		ImGui::Image((void*)ViewportSRV, ViewportSize, uv0, uv1);

		// Hover 상태 업데이트
		bool bHovered = ImGui::IsItemHovered();
		State->Viewport->SetViewportHovered(bHovered);
	}
}

void SParticleEditorWindow::CreateNewParticleSystem()
{
	// 7단계에서 구현
}

void SParticleEditorWindow::SaveParticleSystem()
{
	// 7단계에서 구현
}

void SParticleEditorWindow::SaveParticleSystemAs()
{
	// 7단계에서 구현
}

void SParticleEditorWindow::LoadParticleSystem()
{
	// 7단계에서 구현
}

void SParticleEditorWindow::RenderLeftViewportArea(float totalHeight)
{
	// 좌측 상단 높이 제한
	LeftViewportHeight = ImClamp(LeftViewportHeight, 100.f, totalHeight - 100.f);

	// 좌측 상단: 뷰포트
	ImGui::BeginChild("ViewportArea", ImVec2(0, LeftViewportHeight), true);
	{
		ImVec2 viewportSize = ImGui::GetContentRegionAvail();
		RenderViewportArea(viewportSize.x, viewportSize.y);
	}
	ImGui::EndChild();

	// 수평 스플리터 (뷰포트 - 디테일 간)
	ImGui::Button("##LeftHSplitter", ImVec2(-1, 4.f));
	if (ImGui::IsItemActive())
	{
		float delta = ImGui::GetIO().MouseDelta.y;
		LeftViewportHeight += delta;
	}
	if (ImGui::IsItemHovered())
		ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
}

void SParticleEditorWindow::RenderLeftDetailsArea()
{
	// 좌측 하단: 디테일 패널
	ImGui::BeginChild("DetailsPanel", ImVec2(0, 0), true);
	{
		float leftWidth = ImGui::GetContentRegionAvail().x;
		RenderDetailsPanel(leftWidth);
	}
	ImGui::EndChild();
}

void SParticleEditorWindow::RenderRightEmitterArea(float totalHeight)
{
	// 우측 상단 높이 제한
	RightEmitterHeight = ImClamp(RightEmitterHeight, 100.f, totalHeight - 100.f);

	// 우측 상단: 이미터 패널
	ImGui::BeginChild("EmittersPanel", ImVec2(0, RightEmitterHeight), true);
	{
		RenderRightPanel();
	}
	ImGui::EndChild();

	// 수평 스플리터 (이미터 - 커브 간)
	ImGui::Button("##RightHSplitter", ImVec2(-1, 4.f));
	if (ImGui::IsItemActive())
	{
		float delta = ImGui::GetIO().MouseDelta.y;
		RightEmitterHeight += delta;
	}
	if (ImGui::IsItemHovered())
		ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
}

void SParticleEditorWindow::RenderRightCurveArea()
{
	// 우측 하단: 커브 에디터
	ImGui::BeginChild("CurveEditorPanel", ImVec2(0, 0), true);
	{
		RenderBottomPanel();
	}
	ImGui::EndChild();
}

void SParticleEditorWindow::CreateOriginAxisLines()
{
	if (!OriginAxisLineComponent) return;

	// 기존 라인 제거
	OriginAxisLineComponent->ClearLines();

	// 축 길이 설정
	const float AxisLength = 10.0f;
	const FVector Origin = FVector(0.0f, 0.0f, 0.0f);

	// X축 - 빨강
	OriginAxisLineComponent->AddLine(
		Origin,
		Origin + FVector(AxisLength, 0.0f, 0.0f),
		FVector4(0.796f, 0.086f, 0.105f, 1.0f)
	);

	// Y축 - 초록
	OriginAxisLineComponent->AddLine(
		Origin,
		Origin + FVector(0.0f, AxisLength, 0.0f),
		FVector4(0.125f, 0.714f, 0.113f, 1.0f)
	);

	// Z축 - 파랑
	OriginAxisLineComponent->AddLine(
		Origin,
		Origin + FVector(0.0f, 0.0f, AxisLength),
		FVector4(0.054f, 0.155f, 0.527f, 1.0f)
	);
}
