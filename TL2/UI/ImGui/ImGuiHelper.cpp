#include "pch.h"
#include "ImGuiHelper.h"
#include "../../ImGui/imgui.h"
#include "../../ImGui/imgui_impl_dx11.h"
#include "../../ImGui/imgui_impl_win32.h"
#include "../../Renderer.h"

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, uint32 msg, WPARAM wParam, LPARAM lParam);

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
	ImGui_ImplWin32_Init(InWindowHandle);

	ImGuiIO& IO = ImGui::GetIO();
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
	ImGui_ImplWin32_Init(InWindowHandle);

	// TODO (동민, 한글) utf-8 설정을 푼다면 그냥 한글을 넣지 마세요.
	// ImGui는 utf-8을 기본으로 사용하기 때문에 그것에 맞춰 바꿔야 합니다.
	ImGuiIO& IO = ImGui::GetIO();
	ImFontConfig Cfg;
	/*
		오버 샘플은 폰트 비트맵을 만들 때 더 좋은 해상도로 래스터라이즈했다가
		목표 크기로 다운샘플해서 가장자리 품질을 높이는 슈퍼 샘플링 개념이다.
		따라서 베이크 시간/메모리 비용이 아깝다면 내릴 것
	*/
	Cfg.OversampleH = Cfg.OversampleV = 2;
	IO.Fonts->AddFontFromFileTTF("Fonts/malgun.ttf", 18.0f, &Cfg, IO.Fonts->GetGlyphRangesKorean());

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
