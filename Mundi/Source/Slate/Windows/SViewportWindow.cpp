#include "pch.h"
#include "SViewportWindow.h"
#include "World.h"
#include "ImGui/imgui.h"
#include "USlateManager.h"

#include "FViewport.h"
#include "FViewportClient.h"
#include "Texture.h"
#include "Gizmo/GizmoActor.h"

#include "CameraComponent.h"
#include "CameraActor.h"

extern float CLIENTWIDTH;
extern float CLIENTHEIGHT;

SViewportWindow::SViewportWindow()
{
	ViewportType = EViewportType::Perspective;
	bIsActive = false;
	bIsMouseDown = false;
}

SViewportWindow::~SViewportWindow()
{
	if (Viewport)
	{
		delete Viewport;
		Viewport = nullptr;
	}

	if (ViewportClient)
	{
		delete ViewportClient;
		ViewportClient = nullptr;
	}

	IconSelect = nullptr;
	IconMove = nullptr;
	IconRotate = nullptr;
	IconScale = nullptr;
	IconWorldSpace = nullptr;
	IconLocalSpace = nullptr;
}

bool SViewportWindow::Initialize(float StartX, float StartY, float Width, float Height, UWorld* World, ID3D11Device* Device, EViewportType InViewportType)
{
	ViewportType = InViewportType;

	// 이름 설정
	switch (ViewportType)
	{
	case EViewportType::Perspective:		ViewportName = "원근"; break;
	case EViewportType::Orthographic_Front: ViewportName = "정면"; break;
	case EViewportType::Orthographic_Left:  ViewportName = "왼쪽"; break;
	case EViewportType::Orthographic_Top:   ViewportName = "상단"; break;
	case EViewportType::Orthographic_Back:	ViewportName = "후면"; break;
	case EViewportType::Orthographic_Right:  ViewportName = "오른쪽"; break;
	case EViewportType::Orthographic_Bottom:   ViewportName = "하단"; break;
	}

	// FViewport 생성
	Viewport = new FViewport();
	if (!Viewport->Initialize(StartX, StartY, Width, Height, Device))
	{
		delete Viewport;
		Viewport = nullptr;
		return false;
	}

	// FViewportClient 생성
	ViewportClient = new FViewportClient();
	ViewportClient->SetViewportType(ViewportType);
	ViewportClient->SetWorld(World); // 전역 월드 연결 (이미 있다고 가정)

	// 양방향 연결
	Viewport->SetViewportClient(ViewportClient);

	// 툴바 아이콘 로드
	LoadToolbarIcons(Device);

	return true;
}

void SViewportWindow::OnRender()
{
	// Slate(UI)만 처리하고 렌더는 FViewport에 위임
	RenderToolbar();

	if (Viewport)
		Viewport->Render();
}

void SViewportWindow::OnUpdate(float DeltaSeconds)
{
	if (!Viewport)
		return;

	if (!Viewport) return;

	// 툴바 높이만큼 뷰포트 영역 조정

	uint32 NewStartX = static_cast<uint32>(Rect.Left);
	uint32 NewStartY = static_cast<uint32>(Rect.Top);
	uint32 NewWidth = static_cast<uint32>(Rect.Right - Rect.Left);
	uint32 NewHeight = static_cast<uint32>(Rect.Bottom - Rect.Top);

	Viewport->Resize(NewStartX, NewStartY, NewWidth, NewHeight);
	ViewportClient->Tick(DeltaSeconds);
}

void SViewportWindow::OnMouseMove(FVector2D MousePos)
{
	if (!Viewport) return;

	// 툴바 영역 아래에서만 마우스 이벤트 처리
	FVector2D LocalPos = MousePos - FVector2D(Rect.Left, Rect.Top);
	Viewport->ProcessMouseMove((int32)LocalPos.X, (int32)LocalPos.Y);
}

void SViewportWindow::OnMouseDown(FVector2D MousePos, uint32 Button)
{
	if (!Viewport) return;

	// 툴바 영역 아래에서만 마우스 이벤트 처리s
	bIsMouseDown = true;
	FVector2D LocalPos = MousePos - FVector2D(Rect.Left, Rect.Top);
	Viewport->ProcessMouseButtonDown((int32)LocalPos.X, (int32)LocalPos.Y, Button);

}

void SViewportWindow::OnMouseUp(FVector2D MousePos, uint32 Button)
{
	if (!Viewport) return;

	bIsMouseDown = false;
	FVector2D LocalPos = MousePos - FVector2D(Rect.Left, Rect.Top);
	Viewport->ProcessMouseButtonUp((int32)LocalPos.X, (int32)LocalPos.Y, Button);
}

void SViewportWindow::SetVClientWorld(UWorld* InWorld)
{
	if (ViewportClient && InWorld)
	{
		ViewportClient->SetWorld(InWorld);
	}
}

void SViewportWindow::RenderToolbar()
{
	if (!Viewport) return;

	// 툴바 영역 크기
	float ToolbarHeight = 35.0f;
	ImVec2 ToolbarPosition(Rect.Left, Rect.Top);
	ImVec2 ToolbarSize(Rect.Right - Rect.Left, ToolbarHeight);

	// 툴바 위치 지정
	ImGui::SetNextWindowPos(ToolbarPosition);
	ImGui::SetNextWindowSize(ToolbarSize);

	// 뷰포트별 고유한 윈도우 ID
	char WindowId[64];
	sprintf_s(WindowId, "ViewportToolbar_%p", this);

	ImGuiWindowFlags WindowFlags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus;

	if (ImGui::Begin(WindowId, nullptr, WindowFlags))
	{
		// 기즈모 버튼 스타일 설정
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(2, 0));      // 간격 좁히기
		ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);            // 모서리 둥글게
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));        // 배경 투명
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.2f, 0.2f, 0.2f, 0.5f)); // 호버 배경

		// 기즈모 모드 버튼들 렌더링
		RenderGizmoModeButtons();

		// 구분선
		ImGui::TextColored(ImVec4(0.4f, 0.4f, 0.4f, 1.0f), "|");
		ImGui::SameLine();

		// 기즈모 스페이스 버튼 렌더링
		RenderGizmoSpaceButton();

		// 기즈모 버튼 스타일 복원
		ImGui::PopStyleColor(2);
		ImGui::PopStyleVar(2);


		// 1단계: 메인 ViewMode 선택 (Lit, Unlit, Buffer Visualization, Wireframe)
		const char* MainViewModes[] = { "Lit", "Unlit", "Buffer Visualization", "Wireframe" };

		// 현재 ViewMode에서 메인 모드 인덱스 계산
		int CurrentMainMode = 0; // 기본값: Lit
		EViewModeIndex CurrentViewMode = ViewportClient->GetViewModeIndex();
		if (CurrentViewMode == EViewModeIndex::VMI_Unlit)
		{
			CurrentMainMode = 1;
		}
		else if (CurrentViewMode == EViewModeIndex::VMI_WorldNormal || CurrentViewMode == EViewModeIndex::VMI_SceneDepth)
		{
			CurrentMainMode = 2; // Buffer Visualization
			// 현재 BufferVis 서브모드도 동기화
			if (CurrentViewMode == EViewModeIndex::VMI_SceneDepth)
				CurrentBufferVisSubMode = 0;
			else if (CurrentViewMode == EViewModeIndex::VMI_WorldNormal)
				CurrentBufferVisSubMode = 1;
		}
		else if (CurrentViewMode == EViewModeIndex::VMI_Wireframe)
		{
			CurrentMainMode = 3;
		}
		else if (CurrentViewMode == EViewModeIndex::VMI_SceneDepth)
		{
			CurrentMainMode = 4;
		}
		else // Lit 계열 (Gouraud, Lambert, Phong)
		{
			CurrentMainMode = 0;
			// 현재 Lit 서브모드도 동기화
			if (CurrentViewMode == EViewModeIndex::VMI_Lit)
				CurrentLitSubMode = 0;
			else if (CurrentViewMode == EViewModeIndex::VMI_Lit_Gouraud)
				CurrentLitSubMode = 1;
			else if (CurrentViewMode == EViewModeIndex::VMI_Lit_Lambert)
				CurrentLitSubMode = 2;
			else if (CurrentViewMode == EViewModeIndex::VMI_Lit_Phong)
				CurrentLitSubMode = 3;
		}

		ImGui::SameLine();
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 2));
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6, 0));
		ImGui::SetNextItemWidth(80.0f);
		bool MainModeChanged = ImGui::Combo("##MainViewMode", &CurrentMainMode, MainViewModes, IM_ARRAYSIZE(MainViewModes));

		// 2단계: Lit 서브모드 선택 (Lit 선택 시에만 표시)
		if (CurrentMainMode == 0) // Lit 선택됨
		{
			ImGui::SameLine();
			const char* LitSubModes[] = { "Default(Phong)", "Gouraud", "Lambert", "Phong" };
			ImGui::SetNextItemWidth(80.0f);
			bool SubModeChanged = ImGui::Combo("##LitSubMode", &CurrentLitSubMode, LitSubModes, IM_ARRAYSIZE(LitSubModes));

			if (SubModeChanged && ViewportClient)
			{
				switch (CurrentLitSubMode)
				{
				case 0: ViewportClient->SetViewModeIndex(EViewModeIndex::VMI_Lit); break;
				case 1: ViewportClient->SetViewModeIndex(EViewModeIndex::VMI_Lit_Gouraud); break;
				case 2: ViewportClient->SetViewModeIndex(EViewModeIndex::VMI_Lit_Lambert); break;
				case 3: ViewportClient->SetViewModeIndex(EViewModeIndex::VMI_Lit_Phong); break;
				}
			}
		}

		// 2단계: Buffer Visualization 서브모드 선택 (Buffer Visualization 선택 시에만 표시)
		if (CurrentMainMode == 2) // Buffer Visualization 선택됨
		{
			ImGui::SameLine();
			const char* bufferVisSubModes[] = { "SceneDepth", "WorldNormal" };
			ImGui::SetNextItemWidth(100.0f);
			bool subModeChanged = ImGui::Combo("##BufferVisSubMode", &CurrentBufferVisSubMode, bufferVisSubModes, IM_ARRAYSIZE(bufferVisSubModes));

			if (subModeChanged && ViewportClient)
			{
				switch (CurrentBufferVisSubMode)
				{
				case 0: ViewportClient->SetViewModeIndex(EViewModeIndex::VMI_SceneDepth); break;
				case 1: ViewportClient->SetViewModeIndex(EViewModeIndex::VMI_WorldNormal); break;
				}
			}
		}

		ImGui::PopStyleVar(2);

		// 디버그 ShowFlag 토글 버튼들 (ViewMode와 독립적)
		if (ViewportClient && ViewportClient->GetWorld())
		{
			ImGui::SameLine();
			ImGui::Text("|"); // 구분선
			ImGui::SameLine();

			URenderSettings& RenderSettings = ViewportClient->GetWorld()->GetRenderSettings();

			// Tile Culling Debug
			bool bTileCullingDebug = RenderSettings.IsShowFlagEnabled(EEngineShowFlags::SF_TileCullingDebug);
			if (ImGui::Checkbox("TileCull", &bTileCullingDebug))
			{
				RenderSettings.ToggleShowFlag(EEngineShowFlags::SF_TileCullingDebug);
			}

			ImGui::SameLine();

			// BVH Debug
			bool bBVHDebug = RenderSettings.IsShowFlagEnabled(EEngineShowFlags::SF_BVHDebug);
			if (ImGui::Checkbox("BVH", &bBVHDebug))
			{
				RenderSettings.ToggleShowFlag(EEngineShowFlags::SF_BVHDebug);
			}

			ImGui::SameLine();

			// Grid
			bool bGrid = RenderSettings.IsShowFlagEnabled(EEngineShowFlags::SF_Grid);
			if (ImGui::Checkbox("Grid", &bGrid))
			{
				RenderSettings.ToggleShowFlag(EEngineShowFlags::SF_Grid);
			}

			ImGui::SameLine();

			// Bounding Boxes
			bool bBoundingBoxes = RenderSettings.IsShowFlagEnabled(EEngineShowFlags::SF_BoundingBoxes);
			if (ImGui::Checkbox("Bounds", &bBoundingBoxes))
			{
				RenderSettings.ToggleShowFlag(EEngineShowFlags::SF_BoundingBoxes);
			}
		}

		// 메인 모드 변경 시 처리
		if (MainModeChanged && ViewportClient)
		{
			switch (CurrentMainMode)
			{
			case 0: // Lit - 현재 선택된 서브모드 적용
				switch (CurrentLitSubMode)
				{
				case 0: ViewportClient->SetViewModeIndex(EViewModeIndex::VMI_Lit); break;
				case 1: ViewportClient->SetViewModeIndex(EViewModeIndex::VMI_Lit_Gouraud); break;
				case 2: ViewportClient->SetViewModeIndex(EViewModeIndex::VMI_Lit_Lambert); break;
				case 3: ViewportClient->SetViewModeIndex(EViewModeIndex::VMI_Lit_Phong); break;
				}
				break;
			case 1: ViewportClient->SetViewModeIndex(EViewModeIndex::VMI_Unlit); break;
			case 2: // Buffer Visualization - 현재 선택된 서브모드 적용
				switch (CurrentBufferVisSubMode)
				{
				case 0: ViewportClient->SetViewModeIndex(EViewModeIndex::VMI_SceneDepth); break;
				case 1: ViewportClient->SetViewModeIndex(EViewModeIndex::VMI_WorldNormal); break;
				}
				break;
			case 3: ViewportClient->SetViewModeIndex(EViewModeIndex::VMI_Wireframe); break;
			}
		}
		ImGui::SameLine();
		RenderCameraOptionDropdownMenu();

		ImGui::SameLine(0, 20.0f);
		const ImVec2 ButtonSize(60, 30);
		if (ImGui::Button("Switch##ToThis", ButtonSize))
		{
			SLATE.SwitchPanel(this);
		}
	}
	ImGui::End();
}

void SViewportWindow::LoadToolbarIcons(ID3D11Device* Device)
{
	if (!Device) return;

	// 기즈모 아이콘 텍스처 생성 및 로드
	IconSelect = NewObject<UTexture>();
	IconSelect->Load("Data/Icon/Viewport_Toolbar_Select.png", Device);

	IconMove = NewObject<UTexture>();
	IconMove->Load("Data/Icon/Viewport_Toolbar_Move.png", Device);

	IconRotate = NewObject<UTexture>();
	IconRotate->Load("Data/Icon/Viewport_Toolbar_Rotate.png", Device);

	IconScale = NewObject<UTexture>();
	IconScale->Load("Data/Icon/Viewport_Toolbar_Scale.png", Device);

	IconWorldSpace = NewObject<UTexture>();
	IconWorldSpace->Load("Data/Icon/Viewport_Toolbar_WorldSpace.png", Device);

	IconLocalSpace = NewObject<UTexture>();
	IconLocalSpace->Load("Data/Icon/Viewport_Toolbar_LocalSpace.png", Device);

	// 뷰포트 모드 아이콘 텍스처 로드
	IconCamera = NewObject<UTexture>();
	IconCamera->Load("Data/Icon/Viewport_Mode_Camera.png", Device);

	IconPerspective = NewObject<UTexture>();
	IconPerspective->Load("Data/Icon/Viewport_Mode_Perspective.png", Device);

	IconTop = NewObject<UTexture>();
	IconTop->Load("Data/Icon/Viewport_Mode_Top.png", Device);

	IconBottom = NewObject<UTexture>();
	IconBottom->Load("Data/Icon/Viewport_Mode_Bottom.png", Device);

	IconLeft = NewObject<UTexture>();
	IconLeft->Load("Data/Icon/Viewport_Mode_Left.png", Device);

	IconRight = NewObject<UTexture>();
	IconRight->Load("Data/Icon/Viewport_Mode_Right.png", Device);

	IconFront = NewObject<UTexture>();
	IconFront->Load("Data/Icon/Viewport_Mode_Front.png", Device);

	IconBack = NewObject<UTexture>();
	IconBack->Load("Data/Icon/Viewport_Mode_Back.png", Device);

	// 뷰포트 설정 아이콘 텍스처 로드
	IconSpeed = NewObject<UTexture>();
	IconSpeed->Load("Data/Icon/Viewport_Mode_Camera.png", Device);

	IconFOV = NewObject<UTexture>();
	IconFOV->Load("Data/Icon/Viewport_Setting_FOV.png", Device);

	IconNearClip = NewObject<UTexture>();
	IconNearClip->Load("Data/Icon/Viewport_Setting_NearClip.png", Device);

	IconFarClip = NewObject<UTexture>();
	IconFarClip->Load("Data/Icon/Viewport_Setting_FarClip.png", Device);
}

void SViewportWindow::RenderGizmoModeButtons()
{
	const ImVec2 IconSize(14, 14);

	// GizmoActor에서 직접 현재 모드 가져오기
	EGizmoMode CurrentGizmoMode = EGizmoMode::Select;
	AGizmoActor* GizmoActor = nullptr;
	if (ViewportClient && ViewportClient->GetWorld())
	{
		GizmoActor = ViewportClient->GetWorld()->GetGizmoActor();
		if (GizmoActor)
		{
			CurrentGizmoMode = GizmoActor->GetMode();
		}
	}

	// Select 버튼
	bool bIsSelectActive = (CurrentGizmoMode == EGizmoMode::Select);
	ImVec4 SelectTintColor = bIsSelectActive ? ImVec4(0.3f, 0.6f, 1.0f, 1.0f) : ImVec4(1, 1, 1, 1);

	if (IconSelect && IconSelect->GetShaderResourceView())
	{
		if (ImGui::ImageButton("##SelectBtn", (void*)IconSelect->GetShaderResourceView(), IconSize,
			ImVec2(0, 0), ImVec2(1, 1), ImVec4(0, 0, 0, 0), SelectTintColor))
		{
			if (GizmoActor)
			{
				GizmoActor->SetMode(EGizmoMode::Select);
			}
		}
	}
	else
	{
		if (ImGui::Button("Select", ImVec2(60, 0)))
		{
			if (GizmoActor)
			{
				GizmoActor->SetMode(EGizmoMode::Select);
			}
		}
	}

	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("오브젝트를 선택합니다. [Q]");
	}
	ImGui::SameLine();

	// Move 버튼
	bool bIsMoveActive = (CurrentGizmoMode == EGizmoMode::Translate);
	ImVec4 MoveTintColor = bIsMoveActive ? ImVec4(0.3f, 0.6f, 1.0f, 1.0f) : ImVec4(1, 1, 1, 1);

	if (IconMove && IconMove->GetShaderResourceView())
	{
		if (ImGui::ImageButton("##MoveBtn", (void*)IconMove->GetShaderResourceView(), IconSize,
			ImVec2(0, 0), ImVec2(1, 1), ImVec4(0, 0, 0, 0), MoveTintColor))
		{
			if (GizmoActor)
			{
				GizmoActor->SetMode(EGizmoMode::Translate);
			}
		}
	}
	else
	{
		if (ImGui::Button("Move", ImVec2(60, 0)))
		{
			if (GizmoActor)
			{
				GizmoActor->SetMode(EGizmoMode::Translate);
			}
		}
	}

	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("오브젝트를 선택하고 이동시킵니다. [W]");
	}
	ImGui::SameLine();

	// Rotate 버튼
	bool bIsRotateActive = (CurrentGizmoMode == EGizmoMode::Rotate);
	ImVec4 RotateTintColor = bIsRotateActive ? ImVec4(0.3f, 0.6f, 1.0f, 1.0f) : ImVec4(1, 1, 1, 1);

	if (IconRotate && IconRotate->GetShaderResourceView())
	{
		if (ImGui::ImageButton("##RotateBtn", (void*)IconRotate->GetShaderResourceView(), IconSize,
			ImVec2(0, 0), ImVec2(1, 1), ImVec4(0, 0, 0, 0), RotateTintColor))
		{
			if (GizmoActor)
			{
				GizmoActor->SetMode(EGizmoMode::Rotate);
			}
		}
	}
	else
	{
		if (ImGui::Button("Rotate", ImVec2(60, 0)))
		{
			if (GizmoActor)
			{
				GizmoActor->SetMode(EGizmoMode::Rotate);
			}
		}
	}

	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("오브젝트를 선택하고 회전시킵니다. [E]");
	}
	ImGui::SameLine();

	// Scale 버튼
	bool bIsScaleActive = (CurrentGizmoMode == EGizmoMode::Scale);
	ImVec4 ScaleTintColor = bIsScaleActive ? ImVec4(0.3f, 0.6f, 1.0f, 1.0f) : ImVec4(1, 1, 1, 1);

	if (IconScale && IconScale->GetShaderResourceView())
	{
		if (ImGui::ImageButton("##ScaleBtn", (void*)IconScale->GetShaderResourceView(), IconSize,
			ImVec2(0, 0), ImVec2(1, 1), ImVec4(0, 0, 0, 0), ScaleTintColor))
		{
			if (GizmoActor)
			{
				GizmoActor->SetMode(EGizmoMode::Scale);
			}
		}
	}
	else
	{
		if (ImGui::Button("Scale", ImVec2(60, 0)))
		{
			if (GizmoActor)
			{
				GizmoActor->SetMode(EGizmoMode::Scale);
			}
		}
	}

	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("오브젝트를 선택하고 스케일을 조절합니다. [R]");
	}

	ImGui::SameLine();
}

void SViewportWindow::RenderGizmoSpaceButton()
{
	const ImVec2 IconSize(14, 14);

	// GizmoActor에서 직접 현재 스페이스 가져오기
	EGizmoSpace CurrentGizmoSpace = EGizmoSpace::World;
	AGizmoActor* GizmoActor = nullptr;
	if (ViewportClient && ViewportClient->GetWorld())
	{
		GizmoActor = ViewportClient->GetWorld()->GetGizmoActor();
		if (GizmoActor)
		{
			CurrentGizmoSpace = GizmoActor->GetSpace();
		}
	}

	// 현재 스페이스에 따라 적절한 아이콘 표시
	bool bIsWorldSpace = (CurrentGizmoSpace == EGizmoSpace::World);
	UTexture* CurrentIcon = bIsWorldSpace ? IconWorldSpace : IconLocalSpace;
	const char* TooltipText = bIsWorldSpace ? "월드 스페이스 좌표 [Tab]" : "로컬 스페이스 좌표 [Tab]";

	// 선택 상태 tint (월드/로컬 모두 동일하게 흰색)
	ImVec4 TintColor = ImVec4(1, 1, 1, 1);

	if (CurrentIcon && CurrentIcon->GetShaderResourceView())
	{
		if (ImGui::ImageButton("##GizmoSpaceBtn", (void*)CurrentIcon->GetShaderResourceView(), IconSize,
			ImVec2(0, 0), ImVec2(1, 1), ImVec4(0, 0, 0, 0), TintColor))
		{
			// 버튼 클릭 시 스페이스 전환
			if (GizmoActor)
			{
				EGizmoSpace NewSpace = bIsWorldSpace ? EGizmoSpace::Local : EGizmoSpace::World;
				GizmoActor->SetSpace(NewSpace);
			}
		}
	}
	else
	{
		// 아이콘이 없는 경우 텍스트 버튼
		const char* ButtonText = bIsWorldSpace ? "World" : "Local";
		if (ImGui::Button(ButtonText, ImVec2(60, 0)))
		{
			if (GizmoActor)
			{
				EGizmoSpace NewSpace = bIsWorldSpace ? EGizmoSpace::Local : EGizmoSpace::World;
				GizmoActor->SetSpace(NewSpace);
			}
		}
	}

	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("%s", TooltipText);
	}

	ImGui::SameLine();
}

void SViewportWindow::RenderCameraOptionDropdownMenu()
{
	ImVec2 cursorPos = ImGui::GetCursorPos();
	ImGui::SetCursorPosY(cursorPos.y - 2.0f);

	const float ButtonWidth = 60.0f;
	float Avail = ImGui::GetContentRegionAvail().x;

	const ImVec2 IconSize(17, 17);

	// 드롭다운 버튼 텍스트 준비
	char ButtonText[64];
	sprintf_s(ButtonText, "%s %s", ViewportName.ToString().c_str(), "▼");

	// 텍스트 길이에 맞게 버튼 너비 계산
	ImVec2 TextSize = ImGui::CalcTextSize(ButtonText);
	const float Padding = 8.0f; // 좌우 여백
	const float DropdownWidth = IconSize.x + 4.0f + TextSize.x + Padding * 2.0f;
	const float ButtonSpacing = 15.0f;
	if (Avail > (ButtonWidth + DropdownWidth + ButtonSpacing))
	{
		ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (Avail - ButtonWidth - DropdownWidth - ButtonSpacing));
	}

	// 기즈모 버튼 스타일 적용
	ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.2f, 0.2f, 0.5f));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.3f, 0.3f, 0.5f));
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.4f, 0.4f, 0.4f, 0.6f));

	// 통합된 드롭다운 버튼 (카메라 아이콘 + 현재 모드명 + 화살표)
	ImVec2 ButtonSize(DropdownWidth, ImGui::GetFrameHeight());
	ImVec2 CursorPos = ImGui::GetCursorPos();

	// 버튼 클릭 영역
	if (ImGui::Button("##ViewportModeBtn", ButtonSize))
	{
		ImGui::OpenPopup("ViewportModePopup");
	}

	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("카메라 옵션");
	}

	// 버튼 위에 내용 렌더링 (아이콘 + 텍스트, 가운데 정렬)
	float ContentWidth = IconSize.x + 4.0f + TextSize.x;
	float ContentStartX = CursorPos.x + (ButtonSize.x - ContentWidth) * 0.5f;
	ImVec2 ContentCursor = ImVec2(ContentStartX, CursorPos.y + (ButtonSize.y - IconSize.y) * 0.5f);
	ImGui::SetCursorPos(ContentCursor);

	// 현재 뷰포트 모드에 따라 아이콘 선택
	UTexture* CurrentModeIcon = nullptr;
	switch (ViewportType)
	{
	case EViewportType::Perspective:
		CurrentModeIcon = IconCamera;
		break;
	case EViewportType::Orthographic_Top:
		CurrentModeIcon = IconTop;
		break;
	case EViewportType::Orthographic_Bottom:
		CurrentModeIcon = IconBottom;
		break;
	case EViewportType::Orthographic_Left:
		CurrentModeIcon = IconLeft;
		break;
	case EViewportType::Orthographic_Right:
		CurrentModeIcon = IconRight;
		break;
	case EViewportType::Orthographic_Front:
		CurrentModeIcon = IconFront;
		break;
	case EViewportType::Orthographic_Back:
		CurrentModeIcon = IconBack;
		break;
	default:
		CurrentModeIcon = IconCamera;
		break;
	}

	if (CurrentModeIcon && CurrentModeIcon->GetShaderResourceView())
	{
		ImGui::Image((void*)CurrentModeIcon->GetShaderResourceView(), IconSize);
		ImGui::SameLine(0, 4);
	}

	ImGui::Text("%s", ButtonText);

	ImGui::PopStyleColor(3);
	ImGui::PopStyleVar(1);

	// ===== 뷰포트 모드 드롭다운 팝업 =====
	if (ImGui::BeginPopup("ViewportModePopup", ImGuiWindowFlags_NoMove))
	{
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 4));
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));

		// 선택된 항목의 파란 배경 제거
		ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0, 0, 0, 0));
		ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.2f, 0.2f, 0.2f, 0.5f));
		ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.3f, 0.3f, 0.3f, 0.6f));

		// --- 섹션 1: 원근 ---
		ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "원근");
		ImGui::Separator();

		bool bIsPerspective = (ViewportType == EViewportType::Perspective);
		const char* RadioIcon = bIsPerspective ? "●" : "○";

		// 원근 모드 선택 항목 (라디오 버튼 + 아이콘 + 텍스트 통합)
		ImVec2 SelectableSize(180, 20);
		ImVec2 SelectableCursorPos = ImGui::GetCursorPos();

		if (ImGui::Selectable("##Perspective", bIsPerspective, 0, SelectableSize))
		{
			ViewportType = EViewportType::Perspective;
			ViewportName = "원근";
			if (ViewportClient)
			{
				ViewportClient->SetViewportType(ViewportType);
				ViewportClient->SetupCameraMode();
			}
			ImGui::CloseCurrentPopup();
		}

		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("뷰포트를 원근 보기로 전환합니다.");
		}

		// Selectable 위에 내용 렌더링
		ImVec2 ContentPos = ImVec2(SelectableCursorPos.x + 4, SelectableCursorPos.y + (SelectableSize.y - IconSize.y) * 0.5f);
		ImGui::SetCursorPos(ContentPos);

		ImGui::Text("%s", RadioIcon);
		ImGui::SameLine(0, 4);

		if (IconPerspective && IconPerspective->GetShaderResourceView())
		{
			ImGui::Image((void*)IconPerspective->GetShaderResourceView(), IconSize);
			ImGui::SameLine(0, 4);
		}

		ImGui::Text("원근");

		// --- 섹션 2: 직교 ---
		ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "직교");
		ImGui::Separator();

		// 직교 모드 목록
		struct ViewportModeEntry {
			EViewportType type;
			const char* koreanName;
			UTexture** icon;
			const char* tooltip;
		};

		ViewportModeEntry orthographicModes[] = {
			{ EViewportType::Orthographic_Top, "상단", &IconTop, "뷰포트를 상단 보기로 전환합니다." },
			{ EViewportType::Orthographic_Bottom, "하단", &IconBottom, "뷰포트를 하단 보기로 전환합니다." },
			{ EViewportType::Orthographic_Left, "왼쪽", &IconLeft, "뷰포트를 왼쪽 보기로 전환합니다." },
			{ EViewportType::Orthographic_Right, "오른쪽", &IconRight, "뷰포트를 오른쪽 보기로 전환합니다." },
			{ EViewportType::Orthographic_Front, "정면", &IconFront, "뷰포트를 정면 보기로 전환합니다." },
			{ EViewportType::Orthographic_Back, "후면", &IconBack, "뷰포트를 후면 보기로 전환합니다." }
		};

		for (int i = 0; i < 6; i++)
		{
			const auto& mode = orthographicModes[i];
			bool bIsSelected = (ViewportType == mode.type);
			const char* RadioIcon = bIsSelected ? "●" : "○";

			// 직교 모드 선택 항목 (라디오 버튼 + 아이콘 + 텍스트 통합)
			char SelectableID[32];
			sprintf_s(SelectableID, "##Ortho%d", i);

			ImVec2 OrthoSelectableCursorPos = ImGui::GetCursorPos();

			if (ImGui::Selectable(SelectableID, bIsSelected, 0, SelectableSize))
			{
				ViewportType = mode.type;
				ViewportName = mode.koreanName;
				if (ViewportClient)
				{
					ViewportClient->SetViewportType(ViewportType);
					ViewportClient->SetupCameraMode();
				}
				ImGui::CloseCurrentPopup();
			}

			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("%s", mode.tooltip);
			}

			// Selectable 위에 내용 렌더링
			ImVec2 OrthoContentPos = ImVec2(OrthoSelectableCursorPos.x + 4, OrthoSelectableCursorPos.y + (SelectableSize.y - IconSize.y) * 0.5f);
			ImGui::SetCursorPos(OrthoContentPos);

			ImGui::Text("%s", RadioIcon);
			ImGui::SameLine(0, 4);

			if (*mode.icon && (*mode.icon)->GetShaderResourceView())
			{
				ImGui::Image((void*)(*mode.icon)->GetShaderResourceView(), IconSize);
				ImGui::SameLine(0, 4);
			}

			ImGui::Text("%s", mode.koreanName);
		}

		// --- 섹션 3: 이동 ---
		ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "이동");
		ImGui::Separator();

		ACameraActor* Camera = ViewportClient ? ViewportClient->GetCamera() : nullptr;
		if (Camera)
		{
			if (IconSpeed && IconSpeed->GetShaderResourceView())
			{
				ImGui::Image((void*)IconSpeed->GetShaderResourceView(), IconSize);
				ImGui::SameLine();
			}
			ImGui::Text("카메라 이동 속도");

			float speed = Camera->GetCameraSpeed();
			ImGui::SetNextItemWidth(180);
			if (ImGui::SliderFloat("##CameraSpeed", &speed, 1.0f, 100.0f, "%.1f"))
			{
				Camera->SetCameraSpeed(speed);
			}
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("WASD 키로 카메라를 이동할 때의 속도 (1-100)");
			}
		}

		// --- 섹션 4: 뷰 ---
		ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "뷰");
		ImGui::Separator();

		if (Camera && Camera->GetCameraComponent())
		{
			UCameraComponent* camComp = Camera->GetCameraComponent();

			// FOV
			if (IconFOV && IconFOV->GetShaderResourceView())
			{
				ImGui::Image((void*)IconFOV->GetShaderResourceView(), IconSize);
				ImGui::SameLine();
			}
			ImGui::Text("필드 오브 뷰");

			float fov = camComp->GetFOV();
			ImGui::SetNextItemWidth(180);
			if (ImGui::SliderFloat("##FOV", &fov, 30.0f, 120.0f, "%.1f"))
			{
				camComp->SetFOV(fov);
			}
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("카메라 시야각 (30-120도)\n값이 클수록 넓은 범위가 보입니다");
			}

			// 근평면
			if (IconNearClip && IconNearClip->GetShaderResourceView())
			{
				ImGui::Image((void*)IconNearClip->GetShaderResourceView(), IconSize);
				ImGui::SameLine();
			}
			ImGui::Text("근평면");

			float nearClip = camComp->GetNearClip();
			ImGui::SetNextItemWidth(180);
			if (ImGui::SliderFloat("##NearClip", &nearClip, 0.01f, 10.0f, "%.2f"))
			{
				camComp->SetClipPlanes(nearClip, camComp->GetFarClip());
			}
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("카메라에서 가장 가까운 렌더링 거리 (0.01-10)\n이 값보다 가까운 오브젝트는 보이지 않습니다");
			}

			// 원평면
			if (IconFarClip && IconFarClip->GetShaderResourceView())
			{
				ImGui::Image((void*)IconFarClip->GetShaderResourceView(), IconSize);
				ImGui::SameLine();
			}
			ImGui::Text("원평면");

			float farClip = camComp->GetFarClip();
			ImGui::SetNextItemWidth(180);
			if (ImGui::SliderFloat("##FarClip", &farClip, 100.0f, 10000.0f, "%.0f"))
			{
				camComp->SetClipPlanes(camComp->GetNearClip(), farClip);
			}
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("카메라에서 가장 먼 렌더링 거리 (100-10000)\n이 값보다 먼 오브젝트는 보이지 않습니다");
			}
		}

		ImGui::PopStyleColor(3);
		ImGui::PopStyleVar(2);
		ImGui::EndPopup();
	}
}