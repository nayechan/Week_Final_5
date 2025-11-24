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

SParticleEditorWindow::SParticleEditorWindow()
{
	CenterRect = FRect(0, 0, 0, 0);
	LeftPanelRatio = 0.2f;   // 20% 왼쪽 (뷰포트 + 디테일)
	RightPanelRatio = 0.6f;  // 60% 오른쪽 (이미터 패널)
	bHasBottomPanel = true;
}

SParticleEditorWindow::~SParticleEditorWindow()
{
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
		Rect.Left = pos.x; Rect.Top = pos.y; Rect.Right = pos.x + size.x; Rect.Bottom = pos.y + size.y;
		Rect.UpdateMinMax();

		// 툴바 렌더링
		RenderToolbar();

		ImVec2 contentAvail = ImGui::GetContentRegionAvail();
		float totalWidth = contentAvail.x;
		float totalHeight = contentAvail.y;

		// 하단 패널 높이 (최소/최대 제한)
		BottomPanelHeight = ImClamp(BottomPanelHeight, 100.f, totalHeight - 200.f);
		float topHeight = totalHeight - BottomPanelHeight - 4.f;

		// ========== 상단 영역 (뷰포트 + 디테일 | 이미터 패널) ==========
		ImGui::BeginChild("TopArea", ImVec2(0, topHeight), false);
		{
			// 좌우 패널 너비 계산
			float leftWidth = totalWidth * LeftPanelRatio;
			leftWidth = ImClamp(leftWidth, 200.f, totalWidth - 300.f);

			// 왼쪽: 뷰포트 + 디테일 (상하 분할)
			ImGui::BeginChild("LeftArea", ImVec2(leftWidth, 0), true);
			{
				RenderLeftPanel(leftWidth);
			}
			ImGui::EndChild();

			// 수직 스플리터 (좌우 패널 간)
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

			// 오른쪽: 이미터 패널
			ImGui::BeginChild("EmittersPanel", ImVec2(0, 0), true);
			{
				RenderRightPanel();
			}
			ImGui::EndChild();
		}
		ImGui::EndChild();

		// 수평 스플리터 (상하 패널 간)
		ImGui::Button("##HSplitter", ImVec2(-1, 4.f));
		if (ImGui::IsItemActive())
		{
			float delta = ImGui::GetIO().MouseDelta.y;
			BottomPanelHeight -= delta;
		}
		if (ImGui::IsItemHovered())
			ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);

		// ========== 하단 영역 (커브 에디터) ==========
		ImGui::BeginChild("BottomArea", ImVec2(0, 0), true);
		{
			RenderBottomPanel();
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
	// 파티클 시스템 업데이트는 ParticleSystemComponent에서 자동으로 처리됨
	// 필요시 추가 업데이트 로직 구현
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
	ParticleEditorState* State = GetActiveParticleState();
	if (!State) return;

	float totalHeight = ImGui::GetContentRegionAvail().y;

	// 뷰포트 높이 계산 (최소/최대 제한)
	float ViewportHeight = totalHeight * ViewportDetailsRatio;
	ViewportHeight = ImClamp(ViewportHeight, 150.f, totalHeight - 150.f);

	// 뷰포트 영역
	ImGui::BeginChild("ViewportArea", ImVec2(0, ViewportHeight), true);
	{
		ImVec2 viewportAvail = ImGui::GetContentRegionAvail();
		RenderViewportArea(viewportAvail.x, viewportAvail.y);
	}
	ImGui::EndChild();

	// 수평 스플리터 (뷰포트-디테일 간)
	ImGui::Button("##VDSplitter", ImVec2(-1, 4.f));
	if (ImGui::IsItemActive())
	{
		float delta = ImGui::GetIO().MouseDelta.y;
		ViewportDetailsRatio += delta / totalHeight;
		ViewportDetailsRatio = ImClamp(ViewportDetailsRatio, 0.3f, 0.8f);
	}
	if (ImGui::IsItemHovered())
		ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);

	// 디테일 영역
	ImGui::BeginChild("DetailsArea", ImVec2(0, 0), true);
	{
		RenderDetailsPanel(PanelWidth);
	}
	ImGui::EndChild();
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
	// 툴바 (3단계에서 구현)
	ImGui::Text("툴바 (3단계에서 구현)");
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
