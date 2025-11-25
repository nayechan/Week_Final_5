#include "pch.h"
#include "SParticleEditorWindow.h"
#include "SlateManager.h"
#include "Source/Runtime/Engine/Viewer/ParticleEditorBootstrap.h"
#include "Source/Runtime/Engine/Viewer/EditorAssetPreviewContext.h"
#include "PlatformProcess.h"
#include "ParticleSystem.h"
#include "ParticleEmitter.h"
#include "ParticleLODLevel.h"
#include "ParticleModule.h"
#include "ParticleSystemComponent.h"
#include "FViewport.h"
#include "FViewportClient.h"
#include "Source/Runtime/Engine/Components/LineComponent.h"
#include "Modules/ParticleModuleLifetime.h"
#include "Modules/ParticleModuleSize.h"
#include "Modules/ParticleModuleVelocity.h"
#include "Modules/ParticleModuleColor.h"
#include "Modules/ParticleModuleTypeDataSprite.h"
#include "Modules/ParticleModuleTypeDataMesh.h"
#include "Modules/ParticleModuleTypeDataRibbon.h"
#include "Modules/ParticleModuleTypeDataBeam.h"
#include "Modules/ParticleModuleRequired.h"
#include "Modules/ParticleModuleSpawn.h"
#include "Modules/ParticleModuleAcceleration.h"
#include "Modules/ParticleModuleLocation.h"
#include "Modules/ParticleModuleRotation.h"
#include "Modules/ParticleModuleMeshRotation.h"
#include "Modules/ParticleModuleRotationRate.h"
#include "Modules/ParticleModuleSizeScaleBySpeed.h"
#include "Material.h"
#include "StaticMesh.h"
#include "ResourceManager.h"
#include "Shader.h"
#include "Widgets/PropertyRenderer.h"

SParticleEditorWindow::SParticleEditorWindow()
{
	CenterRect = FRect(0, 0, 0, 0);
	LeftPanelRatio = 0.30f;  // 35% 왼쪽 (뷰포트 + 디테일)
	RightPanelRatio = 0.6f;  // 60% 오른쪽 (이미터 패널)
	bHasBottomPanel = true;
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

	// 탭 정리 (OriginAxisLineComponent는 탭별 State가 소유하므로 따로 정리 불필요)
	for (int i = 0; i < Tabs.Num(); ++i)
	{
		ViewerState* State = Tabs[i];
		ParticleEditorBootstrap::DestroyViewerState(State);
	}
	Tabs.Empty();
	ActiveState = nullptr;
}

// 타입 데이터 삭제 시 스프라이트로 복원 (머티리얼 포함)
static void RestoreToSpriteTypeData(UParticleLODLevel* LOD, ParticleEditorState* State)
{
	// 기존 타입 데이터 제거
	if (LOD->TypeDataModule)
	{
		LOD->Modules.Remove(LOD->TypeDataModule);
	}

	// 스프라이트 타입 데이터 생성
	UParticleModuleTypeDataSprite* SpriteTypeData = NewObject<UParticleModuleTypeDataSprite>();
	LOD->TypeDataModule = SpriteTypeData;
	LOD->Modules.Add(SpriteTypeData);

	// 스프라이트용 머티리얼 생성 및 설정
	if (LOD->RequiredModule)
	{
		UMaterial* SpriteMaterial = NewObject<UMaterial>();
		UShader* SpriteShader = UResourceManager::GetInstance().Load<UShader>("Shaders/Particle/ParticleSprite.hlsl");
		SpriteMaterial->SetShader(SpriteShader);

		// 기본 스프라이트 텍스처 설정
		FMaterialInfo MatInfo;
		MatInfo.DiffuseTextureFileName = GDataDir + "/Textures/Particles/OrientParticle.png";
		SpriteMaterial->SetMaterialInfo(MatInfo);
		SpriteMaterial->ResolveTextures();

		LOD->RequiredModule->Material = SpriteMaterial;
	}

	LOD->CacheModuleInfo();
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

		// Delete 키로 선택된 이미터/모듈 삭제
		ParticleEditorState* State = GetActiveParticleState();
		if (bIsWindowFocused && State && ImGui::IsKeyPressed(ImGuiKey_Delete))
		{
			UParticleSystem* System = State->EditingTemplate;
			if (System)
			{
				// 모듈이 선택된 경우
				if (State->SelectedModule && State->SelectedEmitterIndex >= 0)
				{
					UParticleEmitter* Emitter = System->Emitters[State->SelectedEmitterIndex];
					if (Emitter)
					{
						UParticleLODLevel* LOD = Emitter->GetLODLevel(State->CurrentLODLevel);
						if (LOD)
						{
							UParticleModule* Module = State->SelectedModule;

							// 삭제 불가 모듈 체크 (Required, Spawn, 스프라이트 타입데이터)
							bool bCanDelete = true;
							if (Module == LOD->RequiredModule || Module == LOD->SpawnModule)
							{
								bCanDelete = false;
							}
							if (Cast<UParticleModuleTypeDataSprite>(Module))
							{
								bCanDelete = false;
							}

							if (bCanDelete)
							{
								// TypeDataModule인 경우 (메시/리본/빔 삭제 시 스프라이트로 복원)
								if (Module == LOD->TypeDataModule)
								{
									RestoreToSpriteTypeData(LOD, State);
								}
								else
								{
									LOD->Modules.Remove(Module);
									LOD->CacheModuleInfo();
								}

								State->SelectedModule = nullptr;
								State->SelectedModuleIndex = -1;
								State->bIsDirty = true;
								if (State->PreviewComponent)
								{
									State->PreviewComponent->RefreshEmitterInstances();
								}
							}
						}
					}
				}
				// 이미터만 선택된 경우 (모듈 선택 없음)
				else if (State->SelectedEmitterIndex >= 0 && State->SelectedEmitterIndex < System->Emitters.Num())
				{
					System->Emitters.RemoveAt(State->SelectedEmitterIndex);
					State->SelectedEmitterIndex = -1;
					State->SelectedModuleIndex = -1;
					State->SelectedModule = nullptr;
					State->bIsDirty = true;
					if (State->PreviewComponent)
					{
						State->PreviewComponent->RefreshEmitterInstances();
					}
				}
			}
		}

		// 타입데이터 중복 팝업
		if (bShowTypeDataExistsPopup)
		{
			ImGui::OpenPopup("TypeDataExistsPopup");
			bShowTypeDataExistsPopup = false;
		}

		if (ImGui::BeginPopupModal("TypeDataExistsPopup", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
		{
			ImGui::Text("타입데이터 모듈이 이미 존재합니다.\n먼저 제거해주세요.");
			ImGui::Separator();

			if (ImGui::Button("확인", ImVec2(120, 0)))
			{
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
		}
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

		// 원점축 LineComponent 가시성 제어 (각 탭이 자체 컴포넌트 소유)
		if (State->OriginAxisLineComponent)
		{
			State->OriginAxisLineComponent->SetLineVisible(State->bShowOriginAxis);
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
	// OriginAxisLineComponent는 각 탭의 PreviewActor가 소유하므로
	// World 삭제 시 자동으로 정리
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
	ParticleEditorState* State = GetActiveParticleState();
	if (!State || !State->EditingTemplate)
	{
		ImGui::TextDisabled("파티클 시스템이 없습니다");
		return;
	}

	UParticleSystem* System = State->EditingTemplate;

	// 가로 스크롤 영역 시작
	ImGui::BeginChild("EmitterScrollArea", ImVec2(0, 0), false,
		ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_AlwaysHorizontalScrollbar);

	// 이미터 열 너비 상수
	const float EmitterColumnWidth = 200.f;

	// 각 이미터를 열로 렌더링
	for (int32 i = 0; i < System->Emitters.Num(); ++i)
	{
		UParticleEmitter* Emitter = System->Emitters[i];
		if (!Emitter) continue;

		if (i > 0) ImGui::SameLine();

		ImGui::BeginChild(
			("##Emitter" + std::to_string(i)).c_str(),
			ImVec2(EmitterColumnWidth, 0),
			true,
			ImGuiWindowFlags_NoScrollbar
		);
		{
			RenderEmitterColumn(i, Emitter);
		}
		ImGui::EndChild();
	}

	// 빈 영역 좌클릭 → 선택 해제
	if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(0) && !ImGui::IsAnyItemHovered())
	{
		State->SelectedEmitterIndex = -1;
		State->SelectedModuleIndex = -1;
		State->SelectedModule = nullptr;
	}

	// 빈 영역 우클릭 → 새 이미터 추가 메뉴
	if (ImGui::BeginPopupContextWindow("EmptyAreaContextMenu", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
	{
		if (ImGui::MenuItem("새 파티클 스프라이트 이미터"))
		{
			// 기본 템플릿에서 이미터 가져오기
			UParticleSystem* DefaultTemplate = ParticleEditorBootstrap::CreateDefaultParticleTemplate();
			if (DefaultTemplate && DefaultTemplate->Emitters.Num() > 0)
			{
				UParticleEmitter* NewEmitter = DefaultTemplate->Emitters[0];
				System->Emitters.Add(NewEmitter);

				// EmitterInstances 재생성
				if (State->PreviewComponent)
				{
					State->PreviewComponent->RefreshEmitterInstances();
				}
				State->bIsDirty = true;

				UE_LOG("[ParticleEditor] 새 스프라이트 이미터 추가: %d", System->Emitters.Num() - 1);
			}
		}
		ImGui::EndPopup();
	}

	ImGui::EndChild();
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

			// TODO: 파티클 바운드 그리기
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
	ParticleEditorState* State = GetActiveParticleState();
	if (!State)
	{
		ImGui::TextDisabled("파티클 시스템이 로드되지 않았습니다");
		return;
	}

	ImGui::Text("디테일");
	ImGui::Separator();

	if (!State->SelectedModule)
	{
		ImGui::TextDisabled("모듈을 선택하세요");
		return;
	}

	// 모듈 클래스 이름 표시
	UClass* ModuleClass = State->SelectedModule->GetClass();
	const char* DisplayName = ModuleClass->DisplayName;
	ImGui::Text("모듈: %s", DisplayName ? DisplayName : ModuleClass->Name);
	ImGui::Separator();

	// 프로퍼티 렌더링
	ImGui::PushItemWidth(PanelWidth * 0.55f);
	bool bChanged = UPropertyRenderer::RenderAllPropertiesWithInheritance(State->SelectedModule);
	ImGui::PopItemWidth();

	// 변경 시 Dirty 플래그 및 프리뷰 갱신
	if (bChanged)
	{
		State->bIsDirty = true;
		if (State->PreviewComponent)
		{
			State->PreviewComponent->RefreshEmitterInstances();
		}
	}
}

// 일반 모듈 추가 헬퍼 함수
template<typename T>
static void AddModuleToLOD(UParticleLODLevel* LOD, ParticleEditorState* State)
{
	T* NewModule = NewObject<T>();
	LOD->Modules.Add(NewModule);
	LOD->CacheModuleInfo();
	if (State->PreviewComponent)
	{
		State->PreviewComponent->RefreshEmitterInstances();
	}
	State->bIsDirty = true;
}

// 타입 데이터 모듈 설정 헬퍼 함수
// TODO: 타입 데이터 모듈 별로 다시 만들어줘야함.
// 예: SetMeshTypeDataModule()
template<typename T>
static void SetTypeDataModule(UParticleLODLevel* LOD, ParticleEditorState* State)
{
	// 기존 타입 데이터가 있으면 Modules 배열에서 제거
	if (LOD->TypeDataModule)
	{
		LOD->Modules.Remove(LOD->TypeDataModule);
	}

	// 새 타입 데이터 생성 및 설정
	T* NewModule = NewObject<T>();
	LOD->TypeDataModule = NewModule;
	LOD->Modules.Add(NewModule);
	LOD->CacheModuleInfo();
	if (State->PreviewComponent)
	{
		State->PreviewComponent->RefreshEmitterInstances();
	}
	State->bIsDirty = true;
}

// 메시 타입 데이터 모듈 설정 (기본 메시 및 머티리얼 포함)
static void SetMeshTypeDataModule(UParticleLODLevel* LOD, ParticleEditorState* State)
{
	// 기존 타입 데이터가 있으면 Modules 배열에서 제거
	if (LOD->TypeDataModule)
	{
		LOD->Modules.Remove(LOD->TypeDataModule);
	}

	// 메시 타입 데이터 생성
	UParticleModuleTypeDataMesh* MeshTypeData = NewObject<UParticleModuleTypeDataMesh>();

	// 기본 메시 로드
	MeshTypeData->Mesh = UResourceManager::GetInstance().Load<UStaticMesh>(GDataDir + "/cube-tex.obj");

	// 메시 파티클용 머티리얼 생성
	UMaterial* MeshParticleMaterial = NewObject<UMaterial>();
	UShader* MeshShader = UResourceManager::GetInstance().Load<UShader>("Shaders/Particle/ParticleMesh.hlsl");
	MeshParticleMaterial->SetShader(MeshShader);

	// 메시의 내장 머티리얼에서 텍스처 정보 가져오기
	if (MeshTypeData->Mesh)
	{
		const TArray<FGroupInfo>& GroupInfos = MeshTypeData->Mesh->GetMeshGroupInfo();
		if (!GroupInfos.IsEmpty() && !GroupInfos[0].InitialMaterialName.empty())
		{
			UMaterial* MeshMaterial = UResourceManager::GetInstance().Load<UMaterial>(GroupInfos[0].InitialMaterialName);
			if (MeshMaterial)
			{
				MeshParticleMaterial->SetMaterialInfo(MeshMaterial->GetMaterialInfo());
				MeshParticleMaterial->ResolveTextures();
			}
		}
	}

	// RequiredModule에 머티리얼 설정
	if (LOD->RequiredModule)
	{
		LOD->RequiredModule->Material = MeshParticleMaterial;
	}

	// 타입 데이터 설정
	LOD->TypeDataModule = MeshTypeData;
	LOD->Modules.Add(MeshTypeData);
	LOD->CacheModuleInfo();

	if (State->PreviewComponent)
	{
		State->PreviewComponent->RefreshEmitterInstances();
	}
	State->bIsDirty = true;
}

void SParticleEditorWindow::RenderEmitterColumn(int32 EmitterIndex, UParticleEmitter* Emitter)
{
	ParticleEditorState* State = GetActiveParticleState();
	if (!State || !Emitter) return;

	int32 LODIndex = State->CurrentLODLevel;
	UParticleLODLevel* LOD = Emitter->GetLODLevel(LODIndex);
	if (!LOD)
	{
		ImGui::TextDisabled("LOD %d 없음", LODIndex);
		return;
	}

	// 이미터가 선택되었거나 해당 이미터의 모듈이 선택된 경우 하이라이트
	bool bEmitterSelected = (State->SelectedEmitterIndex == EmitterIndex);

	// 헤더 배경색 (선택 시 주황색)
	ImVec4 HeaderBgColor = bEmitterSelected
		? ImVec4(0.8f, 0.5f, 0.2f, 1.0f)
		: ImVec4(0.15f, 0.15f, 0.15f, 1.0f);

	ImGui::PushStyleColor(ImGuiCol_ChildBg, HeaderBgColor);
	ImGui::BeginChild(("##EmitterHeader" + std::to_string(EmitterIndex)).c_str(), ImVec2(0, 70), false);

	// 첫 번째 줄: "Particle Emitter" 텍스트
	ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 5.0f);
	ImGui::Text("Particle Emitter");

	// 두 번째 줄: 체크박스 + 파티클 개수
	ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 5.0f);
	ImGui::BeginGroup();

	// 커스텀 체크박스 (체크/X 토글)
	ImVec2 CheckboxSize(14, 14);
	ImVec2 CheckboxPos = ImGui::GetCursorScreenPos();
	ImDrawList* HeaderDrawList = ImGui::GetWindowDrawList();

	// 체크박스 배경 (밝은 색상)
	ImU32 CheckboxBgColor = LOD->bEnabled ? IM_COL32(0, 255, 0, 255) : IM_COL32(255, 0, 0, 255);
	HeaderDrawList->AddRectFilled(CheckboxPos, ImVec2(CheckboxPos.x + CheckboxSize.x, CheckboxPos.y + CheckboxSize.y), CheckboxBgColor);
	HeaderDrawList->AddRect(CheckboxPos, ImVec2(CheckboxPos.x + CheckboxSize.x, CheckboxPos.y + CheckboxSize.y), IM_COL32(60, 60, 60, 255));

	// 체크 또는 X 표시
	if (LOD->bEnabled)
	{
		// 체크 표시
		HeaderDrawList->AddLine(
			ImVec2(CheckboxPos.x + 3, CheckboxPos.y + 7),
			ImVec2(CheckboxPos.x + 6, CheckboxPos.y + 10),
			IM_COL32(255, 255, 255, 255), 2.0f);
		HeaderDrawList->AddLine(
			ImVec2(CheckboxPos.x + 6, CheckboxPos.y + 10),
			ImVec2(CheckboxPos.x + 11, CheckboxPos.y + 4),
			IM_COL32(255, 255, 255, 255), 2.0f);
	}
	else
	{
		// X 표시
		HeaderDrawList->AddLine(
			ImVec2(CheckboxPos.x + 3, CheckboxPos.y + 3),
			ImVec2(CheckboxPos.x + 11, CheckboxPos.y + 11),
			IM_COL32(255, 255, 255, 255), 2.0f);
		HeaderDrawList->AddLine(
			ImVec2(CheckboxPos.x + 11, CheckboxPos.y + 3),
			ImVec2(CheckboxPos.x + 3, CheckboxPos.y + 11),
			IM_COL32(255, 255, 255, 255), 2.0f);
	}

	// 클릭 감지용 InvisibleButton
	char CheckboxId[32];
	sprintf_s(CheckboxId, "##EmitterEnabled%d", EmitterIndex);
	if (ImGui::InvisibleButton(CheckboxId, CheckboxSize))
	{
		LOD->bEnabled = !LOD->bEnabled;
		State->bIsDirty = true;
	}

	ImGui::SameLine();
	ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 3.0f);  // 체크박스와 높이 정렬

	// 파티클 개수 표시
	int32 ParticleCount = 0;
	if (State->PreviewComponent && EmitterIndex < State->PreviewComponent->EmitterInstances.Num())
	{
		FParticleEmitterInstance* Instance = State->PreviewComponent->EmitterInstances[EmitterIndex];
		if (Instance)
		{
			ParticleCount = Instance->ActiveParticles;
		}
	}
	ImGui::Text("%d", ParticleCount);

	ImGui::EndGroup();

	// 헤더 클릭 감지 (전체 영역)
	ImVec2 HeaderMin = ImGui::GetWindowPos();
	ImVec2 HeaderMax = ImVec2(HeaderMin.x + ImGui::GetWindowWidth(), HeaderMin.y + 70);
	if (ImGui::IsMouseClicked(0) && ImGui::IsMouseHoveringRect(HeaderMin, HeaderMax))
	{
		State->SelectedEmitterIndex = EmitterIndex;
		State->SelectedModuleIndex = -1;
		State->SelectedModule = nullptr;
	}

	// 썸네일 영역 (우측 상단에 배치)
	ImGui::SetCursorPos(ImVec2(ImGui::GetWindowWidth() - 55, 5));
	ImVec2 ThumbnailPos = ImGui::GetCursorScreenPos();
	ImVec2 ThumbnailSize(50, 50);

	// RequiredModule의 Material에서 Diffuse 텍스처 가져오기
	ID3D11ShaderResourceView* DiffuseSRV = nullptr;
	if (LOD->RequiredModule && LOD->RequiredModule->Material)
	{
		UTexture* DiffuseTexture = LOD->RequiredModule->Material->GetTexture(EMaterialTextureSlot::Diffuse);
		if (DiffuseTexture)
		{
			DiffuseSRV = DiffuseTexture->GetShaderResourceView();
		}
	}

	// 썸네일 검은색 배경
	ImDrawList* DrawList = ImGui::GetWindowDrawList();
	DrawList->AddRectFilled(
		ThumbnailPos,
		ImVec2(ThumbnailPos.x + ThumbnailSize.x, ThumbnailPos.y + ThumbnailSize.y),
		IM_COL32(0, 0, 0, 255)
	);

	if (DiffuseSRV)
	{
		// Diffuse 텍스처 표시
		ImGui::Image((void*)DiffuseSRV, ThumbnailSize);
	}
	else
	{
		ImGui::Dummy(ThumbnailSize);
	}

	ImGui::EndChild();
	ImGui::PopStyleColor();

	ImGui::Separator();

	// 스크롤 가능한 모듈 영역
	ImGui::BeginChild(
		("##ModuleList" + std::to_string(EmitterIndex)).c_str(),
		ImVec2(0, 0),
		false,
		ImGuiWindowFlags_None
	);

	int32 ModuleIdx = 0;

	// 모듈을 우선순위에 따라 정렬하여 렌더링
	// 우선순위: TypeData(0) -> Required(1) -> Spawn(2) -> 일반(100)
	TArray<int32> SortedIndices;
	for (int32 i = 0; i < LOD->Modules.Num(); ++i)
	{
		SortedIndices.Add(i);
	}

	// 우선순위 기준 정렬
	std::sort(SortedIndices.begin(), SortedIndices.end(), [&LOD](int32 A, int32 B)
	{
		UParticleModule* ModuleA = LOD->Modules[A];
		UParticleModule* ModuleB = LOD->Modules[B];
		int32 PriorityA = ModuleA ? ModuleA->GetDisplayPriority() : 100;
		int32 PriorityB = ModuleB ? ModuleB->GetDisplayPriority() : 100;
		return PriorityA < PriorityB;
	});

	// 정렬된 순서로 렌더링
	for (int32 SortedIdx : SortedIndices)
	{
		UParticleModule* Module = LOD->Modules[SortedIdx];
		if (Module)
		{
			RenderModuleBlock(EmitterIndex, ModuleIdx++, Module);
		}
	}

	// 모듈 리스트 빈 영역 좌클릭 → 이미터 선택
	if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(0) && !ImGui::IsAnyItemHovered())
	{
		State->SelectedEmitterIndex = EmitterIndex;
		State->SelectedModuleIndex = -1;
		State->SelectedModule = nullptr;
	}

	// 이미터 영역 우클릭 → 모듈 추가/이미터 관리 메뉴
	char ContextMenuId[64];
	sprintf_s(ContextMenuId, "EmitterContextMenu%d", EmitterIndex);

	if (ImGui::BeginPopupContextWindow(ContextMenuId, ImGuiPopupFlags_MouseButtonRight))
	{
		// 이미터 삭제
		if (ImGui::MenuItem("이미터 삭제"))
		{
			UParticleSystem* System = State->EditingTemplate;
			if (System && EmitterIndex < System->Emitters.Num())
			{
				System->Emitters.RemoveAt(EmitterIndex);
				State->SelectedEmitterIndex = -1;
				State->SelectedModuleIndex = -1;
				State->SelectedModule = nullptr;
				State->bIsDirty = true;
				if (State->PreviewComponent)
				{
					State->PreviewComponent->RefreshEmitterInstances();
				}
			}
		}

		ImGui::Separator();

		// 타입 데이터 (스프라이트가 아닌 경우에만 변경 가능)
		// 스프라이트 타입데이터는 기본값이므로 메뉴에 표시하지 않음
		bool bHasNonSpriteTypeData = LOD->TypeDataModule && !Cast<UParticleModuleTypeDataSprite>(LOD->TypeDataModule);
		if (ImGui::BeginMenu("타입 데이터"))
		{
			if (ImGui::MenuItem("메시"))
			{
				if (bHasNonSpriteTypeData)
				{
					bShowTypeDataExistsPopup = true;
				}
				else
				{
					SetMeshTypeDataModule(LOD, State);
				}
			}
			if (ImGui::MenuItem("리본"))
			{
				if (bHasNonSpriteTypeData)
				{
					bShowTypeDataExistsPopup = true;
				}
				else
				{
					SetTypeDataModule<UParticleModuleTypeDataRibbon>(LOD, State);
				}
			}
			if (ImGui::MenuItem("빔"))
			{
				if (bHasNonSpriteTypeData)
				{
					bShowTypeDataExistsPopup = true;
				}
				else
				{
					SetTypeDataModule<UParticleModuleTypeDataBeam>(LOD, State);
				}
			}
			ImGui::EndMenu();
		}

		// 가속
		if (ImGui::BeginMenu("가속"))
		{
			if (ImGui::MenuItem("가속"))
			{
				AddModuleToLOD<UParticleModuleAcceleration>(LOD, State);
			}
			ImGui::EndMenu();
		}

		// 컬러
		if (ImGui::BeginMenu("컬러"))
		{
			if (ImGui::MenuItem("컬러 오버 라이프"))
			{
				AddModuleToLOD<UParticleModuleColor>(LOD, State);
			}
			ImGui::EndMenu();
		}

		// 수명
		if (ImGui::BeginMenu("수명"))
		{
			if (ImGui::MenuItem("수명"))
			{
				AddModuleToLOD<UParticleModuleLifetime>(LOD, State);
			}
			ImGui::EndMenu();
		}

		// 위치
		if (ImGui::BeginMenu("위치"))
		{
			if (ImGui::MenuItem("초기 위치"))
			{
				AddModuleToLOD<UParticleModuleLocation>(LOD, State);
			}
			ImGui::EndMenu();
		}

		// 회전
		if (ImGui::BeginMenu("회전"))
		{
			if (ImGui::MenuItem("초기 회전"))
			{
				AddModuleToLOD<UParticleModuleRotation>(LOD, State);
			}
			if (ImGui::MenuItem("메시 회전"))
			{
				AddModuleToLOD<UParticleModuleMeshRotation>(LOD, State);
			}
			ImGui::EndMenu();
		}

		// 회전 속도
		if (ImGui::BeginMenu("회전 속도"))
		{
			if (ImGui::MenuItem("초기 회전 속도"))
			{
				AddModuleToLOD<UParticleModuleRotationRate>(LOD, State);
			}
			ImGui::EndMenu();
		}

		// 크기
		if (ImGui::BeginMenu("크기"))
		{
			if (ImGui::MenuItem("초기 크기"))
			{
				AddModuleToLOD<UParticleModuleSize>(LOD, State);
			}
			if (ImGui::MenuItem("속도 기준 크기"))
			{
				AddModuleToLOD<UParticleModuleSizeScaleBySpeed>(LOD, State);
			}
			ImGui::EndMenu();
		}

		// 속도
		if (ImGui::BeginMenu("속도"))
		{
			if (ImGui::MenuItem("초기 속도"))
			{
				AddModuleToLOD<UParticleModuleVelocity>(LOD, State);
			}
			ImGui::EndMenu();
		}

		ImGui::EndPopup();
	}

	ImGui::EndChild();
}

void SParticleEditorWindow::RenderModuleBlock(int32 EmitterIdx, int32 ModuleIdx, UParticleModule* Module)
{
	ParticleEditorState* State = GetActiveParticleState();
	if (!State || !Module) return;

	// 모듈 표시 이름 가져오기
	FString DisplayName = Module->GetClass()->DisplayName ? Module->GetClass()->DisplayName : "";
	if (DisplayName.empty())
	{
		DisplayName = Module->GetClass()->Name ? Module->GetClass()->Name : "Unknown";
		// "UParticleModule" 접두사 제거
		const FString Prefix = "UParticleModule";
		if (DisplayName.find(Prefix) == 0)
		{
			DisplayName = DisplayName.substr(Prefix.length());
		}

		// 언리얼 엔진 표준 모듈 이름 매핑
		static const std::unordered_map<FString, FString> ModuleNameMap = {
			{"Color", "컬러 오버 라이프"},
			{"Size", "초기 크기"},
			{"Velocity", "초기 속도"},
			{"Lifetime", "수명"},
			{"Spawn", "스폰"},
			{"Required", "필수"},
			{"TypeDataSprite", "스프라이트 타입데이터"},
			{"TypeDataMesh", "메시 타입데이터"},
			{"TypeDataRibbon", "리본 타입데이터"},
			{"TypeDataBeam", "빔 타입데이터"},
			{"Acceleration", "가속"},
			{"Location", "초기 위치"},
			{"Rotation", "초기 회전"},
			{"MeshRotation", "메시 회전"},
			{"RotationRate", "초기 회전 속도"},
			{"SizeScaleBySpeed", "속도 기준 크기"}
		};

		auto it = ModuleNameMap.find(DisplayName);
		if (it != ModuleNameMap.end())
		{
			DisplayName = it->second;
		}
	}

	// 클래스 이름 (색상 결정용)
	FString ClassName = Module->GetClass()->Name ? Module->GetClass()->Name : "";

	// 선택 상태 확인
	bool bSelected = (State->SelectedEmitterIndex == EmitterIdx &&
					  State->SelectedModule == Module);

	// 모듈 타입에 따른 배경색
	ImVec4 ModuleBgColor;
	if (bSelected)
	{
		ModuleBgColor = ImVec4(0.8f, 0.5f, 0.2f, 1.0f);  // 선택 시 주황색
	}
	else if (ClassName.find("TypeData") != std::string::npos)
	{
		ModuleBgColor = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);  // TypeData: 검정
	}
	else if (ClassName.find("Required") != std::string::npos)
	{
		ModuleBgColor = ImVec4(0.6f, 0.6f, 0.2f, 1.0f);  // Required: 노랑
	}
	else if (ClassName.find("Spawn") != std::string::npos)
	{
		ModuleBgColor = ImVec4(0.8f, 0.3f, 0.3f, 1.0f);  // Spawn: 빨강
	}
	else
	{
		ModuleBgColor = ImVec4(0.25f, 0.25f, 0.25f, 1.0f);  // 기본: 회색
	}

	// 비활성화 시 어둡게
	if (!Module->bEnabled)
	{
		ModuleBgColor.x *= 0.5f;
		ModuleBgColor.y *= 0.5f;
		ModuleBgColor.z *= 0.5f;
	}

	// 모듈 블록 높이
	const float ModuleHeight = 24.0f;

	// TypeData, Required가 아닌 경우에만 체크박스 표시
	bool bShowCheckbox = (ClassName.find("TypeData") == std::string::npos) &&
						 (ClassName.find("Required") == std::string::npos);

	// 모듈 배경 그리기
	ImGui::PushStyleColor(ImGuiCol_ChildBg, ModuleBgColor);
	char ModuleChildId[64];
	sprintf_s(ModuleChildId, "##ModuleBlock_%d_%d", EmitterIdx, ModuleIdx);
	ImGui::BeginChild(ModuleChildId, ImVec2(0, ModuleHeight), false);

	// 모듈 이름
	ImGui::SetCursorPosX(5.0f);
	ImGui::SetCursorPosY((ModuleHeight - ImGui::GetTextLineHeight()) * 0.5f);
	ImGui::Text("%s", DisplayName.c_str());

	// 체크박스 (모듈 블록 내부에서 그리기)
	if (bShowCheckbox)
	{
		float CheckboxX = ImGui::GetWindowWidth() - 38.0f;
		float CheckboxY = (ModuleHeight - 14.0f) * 0.5f;

		ImGui::SetCursorPos(ImVec2(CheckboxX, CheckboxY));

		ImVec2 CheckboxSize(14, 14);
		ImVec2 CheckboxPos = ImGui::GetCursorScreenPos();
		ImDrawList* DrawList = ImGui::GetWindowDrawList();

		ImU32 CheckboxBgColor = Module->bEnabled ? IM_COL32(0, 255, 0, 255) : IM_COL32(255, 0, 0, 255);
		DrawList->AddRectFilled(CheckboxPos, ImVec2(CheckboxPos.x + CheckboxSize.x, CheckboxPos.y + CheckboxSize.y), CheckboxBgColor);
		DrawList->AddRect(CheckboxPos, ImVec2(CheckboxPos.x + CheckboxSize.x, CheckboxPos.y + CheckboxSize.y), IM_COL32(60, 60, 60, 255));

		if (Module->bEnabled)
		{
			// 체크 표시
			DrawList->AddLine(
				ImVec2(CheckboxPos.x + 3, CheckboxPos.y + 7),
				ImVec2(CheckboxPos.x + 6, CheckboxPos.y + 10),
				IM_COL32(255, 255, 255, 255), 2.0f);
			DrawList->AddLine(
				ImVec2(CheckboxPos.x + 6, CheckboxPos.y + 10),
				ImVec2(CheckboxPos.x + 11, CheckboxPos.y + 4),
				IM_COL32(255, 255, 255, 255), 2.0f);
		}
		else
		{
			// X 표시
			DrawList->AddLine(
				ImVec2(CheckboxPos.x + 3, CheckboxPos.y + 3),
				ImVec2(CheckboxPos.x + 11, CheckboxPos.y + 11),
				IM_COL32(255, 255, 255, 255), 2.0f);
			DrawList->AddLine(
				ImVec2(CheckboxPos.x + 11, CheckboxPos.y + 3),
				ImVec2(CheckboxPos.x + 3, CheckboxPos.y + 11),
				IM_COL32(255, 255, 255, 255), 2.0f);
		}

		char CheckboxId[64];
		sprintf_s(CheckboxId, "##ModuleEnabled_%d_%d", EmitterIdx, ModuleIdx);
		if (ImGui::InvisibleButton(CheckboxId, CheckboxSize))
		{
			Module->bEnabled = !Module->bEnabled;
			State->bIsDirty = true;
		}
	}

	// 클릭 감지 (전체 영역)
	// 스프라이트 타입데이터는 선택 불가
	bool bIsSpriteTypeData = Cast<UParticleModuleTypeDataSprite>(Module) != nullptr;
	ImVec2 BlockMin = ImGui::GetWindowPos();
	ImVec2 BlockMax = ImVec2(BlockMin.x + ImGui::GetWindowWidth(), BlockMin.y + ModuleHeight);
	if (ImGui::IsMouseClicked(0) && ImGui::IsMouseHoveringRect(BlockMin, BlockMax) && !bIsSpriteTypeData)
	{
		State->SelectedEmitterIndex = EmitterIdx;
		State->SelectedModuleIndex = ModuleIdx;
		State->SelectedModule = Module;
	}

	ImGui::EndChild();
	ImGui::PopStyleColor();

	// 모듈 우클릭 컨텍스트 메뉴
	char ContextMenuId[64];
	sprintf_s(ContextMenuId, "ModuleContextMenu_%d_%d", EmitterIdx, ModuleIdx);

	if (ImGui::BeginPopupContextItem(ContextMenuId))
	{
		// 삭제 (Required, Spawn, 스프라이트 타입데이터 모듈은 삭제 불가)
		bool bCanDelete = true;
		UParticleLODLevel* LOD = nullptr;
		if (State->EditingTemplate && EmitterIdx < State->EditingTemplate->Emitters.Num())
		{
			UParticleEmitter* Emitter = State->EditingTemplate->Emitters[EmitterIdx];
			if (Emitter)
			{
				LOD = Emitter->GetLODLevel(State->CurrentLODLevel);
				if (LOD)
				{
					// Required, Spawn 모듈은 삭제 불가
					if (Module == LOD->RequiredModule || Module == LOD->SpawnModule)
					{
						bCanDelete = false;
					}
					// 스프라이트 타입데이터는 삭제 불가 (기본 타입이므로)
					if (Cast<UParticleModuleTypeDataSprite>(Module))
					{
						bCanDelete = false;
					}
				}
			}
		}

		if (bCanDelete)
		{
			if (ImGui::MenuItem("모듈 삭제"))
			{
				if (LOD)
				{
					// TypeDataModule인 경우 (메시/리본/빔 삭제 시 스프라이트로 복원)
					if (Module == LOD->TypeDataModule)
					{
						RestoreToSpriteTypeData(LOD, State);
					}
					else
					{
						// 일반 모듈 배열에서 삭제
						LOD->Modules.Remove(Module);
						LOD->CacheModuleInfo();
					}

					State->SelectedModule = nullptr;
					State->SelectedModuleIndex = -1;
					State->bIsDirty = true;
					if (State->PreviewComponent)
					{
						State->PreviewComponent->RefreshEmitterInstances();
					}
				}
			}
		}
		else
		{
			ImGui::TextDisabled("(삭제 불가)");
		}

		ImGui::EndPopup();
	}
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

void SParticleEditorWindow::SaveParticleSystem()
{
	ParticleEditorState* State = GetActiveParticleState();
	if (!State)
	{
		UE_LOG("[SParticleEditorWindow] SaveParticleSystem: 활성 상태가 없습니다");
		return;
	}

	if (!State->EditingTemplate)
	{
		UE_LOG("[SParticleEditorWindow] SaveParticleSystem: 편집 중인 템플릿이 없습니다");
		return;
	}

	// 파일 경로가 없으면 SaveAs 호출
	if (State->CurrentFilePath.empty())
	{
		SaveParticleSystemAs();
		return;
	}

	// 파일로 저장
	if (ParticleEditorBootstrap::SaveParticleSystem(State->EditingTemplate, State->CurrentFilePath))
	{
		// 저장 성공 - Dirty 플래그 해제
		State->bIsDirty = false;
		UE_LOG("[SParticleEditorWindow] 파티클 시스템 저장 완료: %s", State->CurrentFilePath.c_str());
	}
	else
	{
		// 저장 실패
		UE_LOG("[SParticleEditorWindow] 파티클 시스템 저장 실패: %s", State->CurrentFilePath.c_str());
	}
}

void SParticleEditorWindow::SaveParticleSystemAs()
{
	ParticleEditorState* State = GetActiveParticleState();
	if (!State)
	{
		UE_LOG("[SParticleEditorWindow] SaveParticleSystemAs: 활성 상태가 없습니다");
		return;
	}

	if (!State->EditingTemplate)
	{
		UE_LOG("[SParticleEditorWindow] SaveParticleSystemAs: 편집 중인 템플릿이 없습니다");
		return;
	}

	// 저장 파일 다이얼로그 열기
	std::filesystem::path SavePath = FPlatformProcess::OpenSaveFileDialog(
		L"Data/Particles",       // 기본 디렉토리
		L"particle",             // 기본 확장자
		L"Particle Files",       // 파일 타입 설명
		L"NewParticleSystem"     // 기본 파일명
	);

	// 사용자가 취소한 경우
	if (SavePath.empty())
	{
		UE_LOG("[SParticleEditorWindow] SaveParticleSystemAs: 사용자가 취소했습니다");
		return;
	}

	// std::filesystem::path를 FString으로 변환
	FString SavePathStr = SavePath.string();

	// 파일로 저장
	if (ParticleEditorBootstrap::SaveParticleSystem(State->EditingTemplate, SavePathStr))
	{
		// 저장 성공 - 파일 경로 설정 및 Dirty 플래그 해제
		State->CurrentFilePath = SavePathStr;
		State->bIsDirty = false;
		UE_LOG("[SParticleEditorWindow] 파티클 시스템 저장 완료: %s", SavePathStr.c_str());
	}
	else
	{
		// 저장 실패
		UE_LOG("[SParticleEditorWindow] 파티클 시스템 저장 실패: %s", SavePathStr.c_str());
	}
}

void SParticleEditorWindow::LoadParticleSystem()
{
	ParticleEditorState* State = GetActiveParticleState();
	if (!State)
	{
		UE_LOG("[SParticleEditorWindow] LoadParticleSystem: 활성 상태가 없습니다");
		return;
	}

	// 불러오기 파일 다이얼로그 열기
	std::filesystem::path LoadPath = FPlatformProcess::OpenLoadFileDialog(
		L"Data/Particles",   // 기본 디렉토리
		L"particle",         // 확장자 필터
		L"Particle Files"    // 파일 타입 설명
	);

	// 사용자가 취소한 경우
	if (LoadPath.empty())
	{
		UE_LOG("[SParticleEditorWindow] LoadParticleSystem: 사용자가 취소했습니다");
		return;
	}

	// std::filesystem::path를 FString으로 변환
	FString LoadPathStr = LoadPath.string();

	// 파일에서 파티클 시스템 로드
	UParticleSystem* LoadedSystem = ParticleEditorBootstrap::LoadParticleSystem(LoadPathStr);
	if (!LoadedSystem)
	{
		UE_LOG("[SParticleEditorWindow] 파티클 시스템 로드 실패: %s", LoadPathStr.c_str());
		return;
	}

	// 기존 템플릿 교체
	State->EditingTemplate = LoadedSystem;

	// PreviewComponent에 새 템플릿 설정
	if (State->PreviewComponent)
	{
		State->PreviewComponent->SetTemplate(LoadedSystem);
	}

	// 파일 경로 설정 및 Dirty 플래그 해제
	State->CurrentFilePath = LoadPathStr;
	State->bIsDirty = false;

	// 선택 상태 초기화
	State->SelectedEmitterIndex = -1;
	State->SelectedModuleIndex = -1;
	State->SelectedModule = nullptr;

	UE_LOG("[SParticleEditorWindow] 파티클 시스템 로드 완료: %s", LoadPathStr.c_str());
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

void SParticleEditorWindow::OpenOrFocusTab(UEditorAssetPreviewContext* Context)
{
	// Context가 없거나 AssetPath가 비어있으면 새 탭 열기
	if (!Context || Context->AssetPath.empty())
	{
		char label[64];
		sprintf_s(label, "Particle Editor %d", Tabs.Num() + 1);
		OpenNewTab(label);
		return;
	}

	// 기존 탭 중 동일한 파일 경로를 가진 탭 검색
	for (int i = 0; i < Tabs.Num(); ++i)
	{
		ParticleEditorState* State = static_cast<ParticleEditorState*>(Tabs[i]);
		if (State && State->CurrentFilePath == Context->AssetPath)
		{
			// 기존 탭으로 전환
			ActiveTabIndex = i;
			ActiveState = Tabs[i];
			UE_LOG("[SParticleEditorWindow] 기존 탭 발견: %s", Context->AssetPath.c_str());
			return;
		}
	}

	// 기존 탭이 없으면 새 탭 생성
	UE_LOG("[SParticleEditorWindow] 새 탭 생성: %s", Context->AssetPath.c_str());

	// 파일 경로에서 파일 이름 추출
	FString AssetName;
	const size_t lastSlash = Context->AssetPath.find_last_of("/\\");
	if (lastSlash != FString::npos)
		AssetName = Context->AssetPath.substr(lastSlash + 1);
	else
		AssetName = Context->AssetPath;

	// 확장자 제거
	const size_t dotPos = AssetName.find_last_of('.');
	if (dotPos != FString::npos)
		AssetName = AssetName.substr(0, dotPos);
	ViewerState* NewState = CreateViewerState(AssetName.c_str(), Context);

	if (NewState)
	{
		Tabs.Add(NewState);
		ActiveTabIndex = Tabs.Num() - 1;
		ActiveState = NewState;

		// 파티클 시스템 로드
		ParticleEditorState* ParticleState = static_cast<ParticleEditorState*>(NewState);
		UParticleSystem* LoadedSystem = ParticleEditorBootstrap::LoadParticleSystem(Context->AssetPath);
		if (LoadedSystem)
		{
			ParticleState->EditingTemplate = LoadedSystem;
			ParticleState->CurrentFilePath = Context->AssetPath;
			ParticleState->bIsDirty = false;

			// PreviewComponent에 새 템플릿 설정
			if (ParticleState->PreviewComponent)
			{
				ParticleState->PreviewComponent->SetTemplate(LoadedSystem);
			}

			UE_LOG("[SParticleEditorWindow] 파티클 시스템 로드 완료: %s", Context->AssetPath.c_str());
		}
		else
		{
			UE_LOG("[SParticleEditorWindow] 파티클 시스템 로드 실패: %s", Context->AssetPath.c_str());
		}
	}
}
