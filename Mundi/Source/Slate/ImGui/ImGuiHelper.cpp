#include "pch.h"
#include "ImGuiHelper.h"
#include "ImGui/imgui.h"
#include "ImGui/imgui_impl_dx11.h"
#include "ImGui/imgui_impl_win32.h"
#include "ImGui/imnodes.h"
#include "Renderer.h"

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, uint32 msg, WPARAM wParam, LPARAM lParam);

IMPLEMENT_CLASS(UImGuiHelper)

UImGuiHelper::UImGuiHelper() = default;

UImGuiHelper::~UImGuiHelper()
{
	Release();
}

/**
 * @brief ImGui 초기화 함수
 */
void UImGuiHelper::Initialize(HWND InWindowHandle)
{
	if (bIsInitialized)
	{
		return;
	}

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImNodes::CreateContext();

	// 커스텀 다크 테마 적용
	SetupCustomTheme();

	ImGui_ImplWin32_Init(InWindowHandle);

    ImGuiIO& IO = ImGui::GetIO();
    // Restrict window moving to title bar only to avoid dragging content moving entire windows
    IO.ConfigWindowsMoveFromTitleBarOnly = true;
	// Use default ImGui font
	// IO.Fonts->AddFontDefault();

	// Note: This overload needs device and context to be passed explicitly
	// Use the new Initialize overload instead
	bIsInitialized = true;
}

/**
 * @brief ImGui 초기화 함수 (디바이스와 컨텍스트 포함)
 */
void UImGuiHelper::Initialize(HWND InWindowHandle, ID3D11Device* InDevice, ID3D11DeviceContext* InDeviceContext)
{
	if (bIsInitialized)
	{
		return;
	}

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImNodes::CreateContext();

	// 커스텀 다크 테마 적용
	SetupCustomTheme();

	ImGui_ImplWin32_Init(InWindowHandle);

	// TODO (동민, 한글) utf-8 설정을 푼다면 그냥 한글을 넣지 마세요.
	// ImGui는 utf-8을 기본으로 사용하기 때문에 그것에 맞춰 바꿔야 합니다.
    ImGuiIO& IO = ImGui::GetIO();
    // Restrict window moving to title bar only to avoid dragging content moving entire windows
    IO.ConfigWindowsMoveFromTitleBarOnly = true;
	ImFontConfig Cfg;
	/*
		오버 샘플은 폰트 비트맵을 만들 때 더 좋은 해상도로 래스터라이즈했다가
		목표 크기로 다운샘플해서 가장자리 품질을 높이는 슈퍼 샘플링 개념이다.
		따라서 베이크 시간/메모리 비용이 아깝다면 내릴 것
	*/
	Cfg.OversampleH = Cfg.OversampleV = 2;
	FString FontPath = GDataDir + "/UI/Fonts/malgun.ttf";
	IO.Fonts->AddFontFromFileTTF(FontPath.c_str(), 18.0f, &Cfg, IO.Fonts->GetGlyphRangesKorean());

	// Use default ImGui font
	// IO.Fonts->AddFontDefault();

	// Initialize ImGui DirectX11 backend with provided device and context
	ImGui_ImplDX11_Init(InDevice, InDeviceContext);

	bIsInitialized = true;
}

/**
 * @brief ImGui 자원 해제 함수
 */
void UImGuiHelper::Release()
{
	if (!bIsInitialized)
	{
		return;
	}

	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImNodes::DestroyContext();
	ImGui::DestroyContext();

	bIsInitialized = false;
}

/**
 * @brief ImGui 새 프레임 시작
 */
void UImGuiHelper::BeginFrame() const
{
	if (!bIsInitialized)
	{
		return;
	}

	// GetInstance New Tick
	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
}

/**
 * @brief ImGui 렌더링 종료 및 출력
 */
void UImGuiHelper::EndFrame() const
{
	if (!bIsInitialized)
	{
		return;
	}

	// Render ImGui
	ImGui::Render();
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}

/**
 * @brief WndProc Handler 래핑 함수
 * @return ImGui 자체 함수 반환
 */
LRESULT UImGuiHelper::WndProcHandler(HWND hWnd, uint32 msg, WPARAM wParam, LPARAM lParam)
{
	return ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam);
}

void UImGuiHelper::SetupCustomTheme()
{
	ImGuiStyle& Style = ImGui::GetStyle();

	// 모서리 둥글기 설정
	Style.WindowRounding = 0.0f;     // 윈도우
	Style.FrameRounding = 4.0f;      // 버튼, 입력 필드
	Style.GrabRounding = 4.0f;       // 슬라이더 핸들
	Style.TabRounding = 4.0f;        // 탭
	Style.ScrollbarRounding = 9.0f;  // 스크롤바
	Style.ChildRounding = 4.0f;      // 자식 윈도우
	Style.PopupRounding = 4.0f;      // 팝업

	// ===== 테두리 두께 설정 =====
	Style.WindowBorderSize = 1.0f;
	Style.FrameBorderSize = 0.0f;
	Style.PopupBorderSize = 1.0f;
	Style.TabBorderSize = 0.0f;

	// ===== 색상 팔레트 (Monochrome Dark Theme) =====
	ImVec4* Colors = Style.Colors;

	// 배경 색상
	Colors[ImGuiCol_WindowBg] = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);		// 메인 윈도우 배경
	Colors[ImGuiCol_ChildBg] = ImVec4(0.12f, 0.12f, 0.12f, 1.00f);		// 자식 윈도우 배경
	Colors[ImGuiCol_PopupBg] = ImVec4(0.08f, 0.08f, 0.08f, 0.98f);		// 팝업 배경

	// 테두리
	Colors[ImGuiCol_Border] = ImVec4(0.30f, 0.30f, 0.30f, 0.50f);			// 테두리
	Colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);		// 테두리 그림자 (투명)

	// 프레임 배경 (입력 필드, 콤보박스 등)
	Colors[ImGuiCol_FrameBg] = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);			// 기본
	Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);	// 마우스 오버
	Colors[ImGuiCol_FrameBgActive] = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);	// 활성화

	// 타이틀바 (그라데이션 느낌)
	Colors[ImGuiCol_TitleBg] = ImVec4(0.08f, 0.08f, 0.08f, 1.00f);			// 비활성 타이틀바
	Colors[ImGuiCol_TitleBgActive] = ImVec4(0.08f, 0.08f, 0.08f, 1.00f);	// 활성 타이틀바
	Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.08f, 0.08f, 0.08f, 0.75f);	// 접힌 타이틀바

	// 메뉴바
	Colors[ImGuiCol_MenuBarBg] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);

	// 스크롤바
	Colors[ImGuiCol_ScrollbarBg] = ImVec4(0.02f, 0.02f, 0.02f, 0.53f);
	Colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.31f, 0.31f, 0.31f, 1.00f);
	Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.41f, 0.41f, 0.41f, 1.00f);
	Colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.51f, 0.51f, 0.51f, 1.00f);

	// 체크박스
	Colors[ImGuiCol_CheckMark] = ImVec4(0.85f, 0.85f, 0.85f, 1.00f);

	// 슬라이더
	Colors[ImGuiCol_SliderGrab] = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
	Colors[ImGuiCol_SliderGrabActive] = ImVec4(0.61f, 0.61f, 0.61f, 1.00f);

	// 버튼
	Colors[ImGuiCol_Button] = ImVec4(0.25f, 0.25f, 0.25f, 0.80f);			// 기본 버튼
	Colors[ImGuiCol_ButtonHovered] = ImVec4(0.35f, 0.35f, 0.35f, 0.90f);	// 마우스 오버
	Colors[ImGuiCol_ButtonActive] = ImVec4(0.45f, 0.45f, 0.45f, 1.00f);		// 클릭 시

	// 헤더 (트리 노드, 콤보박스 등)
	Colors[ImGuiCol_Header] = ImVec4(0.22f, 0.22f, 0.22f, 0.80f);			// 기본
	Colors[ImGuiCol_HeaderHovered] = ImVec4(0.28f, 0.28f, 0.28f, 0.85f);	// 마우스 오버
	Colors[ImGuiCol_HeaderActive] = ImVec4(0.35f, 0.35f, 0.35f, 1.00f);		// 활성 (에메랄드)

	// 분리선
	Colors[ImGuiCol_Separator] = ImVec4(0.43f, 0.43f, 0.43f, 0.50f);
	Colors[ImGuiCol_SeparatorHovered] = ImVec4(0.70f, 0.70f, 0.70f, 0.78f);
	Colors[ImGuiCol_SeparatorActive] = ImVec4(0.80f, 0.80f, 0.80f, 1.00f);

	// 크기 조절 그립
	Colors[ImGuiCol_ResizeGrip] = ImVec4(0.30f, 0.30f, 0.30f, 0.50f);
	Colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.40f, 0.40f, 0.40f, 0.70f);
	Colors[ImGuiCol_ResizeGripActive] = ImVec4(0.50f, 0.50f, 0.50f, 0.95f);

	// 탭
	Colors[ImGuiCol_Tab] = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);					// 비활성 탭
	Colors[ImGuiCol_TabHovered] = ImVec4(0.35f, 0.35f, 0.35f, 0.80f);			// 마우스 오버
	Colors[ImGuiCol_TabActive] = ImVec4(0.28f, 0.28f, 0.28f, 1.00f);			// 활성 탭
	Colors[ImGuiCol_TabUnfocused] = ImVec4(0.15f, 0.15f, 0.15f, 1.00f);			// 포커스 없는 탭
	Colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);	// 포커스 없지만 활성

	// 도킹 (DockSpace) - Docking 브랜치에서만 사용 가능
	#ifdef IMGUI_HAS_DOCK
	Colors[ImGuiCol_DockingPreview] = ImVec4(0.40f, 0.40f, 0.40f, 0.70f);	// 도킹 미리보기
	Colors[ImGuiCol_DockingEmptyBg] = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);	// 빈 도킹 공간
	#endif

	// 테이블
	Colors[ImGuiCol_TableHeaderBg] = ImVec4(0.19f, 0.19f, 0.20f, 1.00f);
	Colors[ImGuiCol_TableBorderStrong] = ImVec4(0.31f, 0.31f, 0.35f, 1.00f);
	Colors[ImGuiCol_TableBorderLight] = ImVec4(0.23f, 0.23f, 0.25f, 1.00f);
	Colors[ImGuiCol_TableRowBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
	Colors[ImGuiCol_TableRowBgAlt] = ImVec4(1.00f, 1.00f, 1.00f, 0.06f);

	// 텍스트 선택
	Colors[ImGuiCol_TextSelectedBg] = ImVec4(0.50f, 0.50f, 0.50f, 0.35f);

	// 드래그 앤 드롭 타겟
	Colors[ImGuiCol_DragDropTarget] = ImVec4(0.70f, 0.70f, 0.70f, 0.90f);

	// 네비게이션 하이라이트
	Colors[ImGuiCol_NavHighlight] = ImVec4(0.60f, 0.60f, 0.60f, 1.00f);
	Colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
	Colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);

	// 모달 윈도우 딤 배경
	Colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.65f);

	// 텍스트 색상
	Colors[ImGuiCol_Text] = ImVec4(0.95f, 0.95f, 0.95f, 1.00f);			// 기본 텍스트
	Colors[ImGuiCol_TextDisabled] = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);	// 비활성 텍스트
}
