#include "pch.h"
#include <objbase.h>

#ifdef _EDITOR
#include "EditorEngine.h"
#endif

#ifdef _GAME
#include "GameEngine.h"
#endif

#include "PlatformCrashHandler.h"
#include "DebugUtils.h"
#include <exception>

#if defined(_MSC_VER) && defined(_DEBUG)
#   define _CRTDBG_MAP_ALLOC
#   include <cstdlib>
#   include <crtdbg.h>
#endif

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
    // COM 라이브러리 초기화 (STA: Single-Threaded Apartment)
    // WIC(Windows Imaging Component)와 같은 COM 기반 API 사용에 필수적
    if (FAILED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED)))
    {
        MessageBox(nullptr, L"Failed to initialize COM library.", L"Error", MB_OK | MB_ICONERROR);
        return -1;
    }

    // 심볼 서버 자동 설정 (가장 먼저 호출)
    // 별도 설정 없이 덤프 파일 분석 가능
    FDebugUtils::InitializeSymbolServer();

    // 크래시 핸들러 초기화 (모든 예외를 캐치하여 MiniDump 생성)
    FPlatformCrashHandler::InitializeCrashHandler();

#if defined(_MSC_VER) && defined(_DEBUG)
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
    _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_DEBUG);
    _CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_DEBUG);
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_DEBUG);
    _CrtSetBreakAlloc(0);
#endif

    try
    {
        if (!GEngine.Startup(hInstance))
            return -1;

        GEngine.MainLoop();
        GEngine.Shutdown();
    }
    catch (const std::exception& e)
    {
        // C++ 표준 예외 처리 (std::runtime_error, std::logic_error 등)
        wchar_t msg[512];
        // e.what()은 char*이므로 변환 필요
        MultiByteToWideChar(CP_UTF8, 0, e.what(), -1, msg, 256);
        wchar_t fullMsg[512];
        swprintf_s(fullMsg, 512, L"C++ Exception caught:\n\n%s\n\nGenerating MiniDump...", msg);
        MessageBoxW(nullptr, fullMsg, L"C++ Exception", MB_OK | MB_ICONERROR);

        // MiniDump 생성
        FPlatformCrashHandler::GenerateMiniDump();
        CoUninitialize(); // COM 해제
        return -1;
    }
    catch (...)
    {
        // 알 수 없는 예외 처리 (throw int, throw custom_type 등)
        MessageBoxW(nullptr,
            L"Unknown C++ exception caught!\n\nGenerating MiniDump...",
            L"Unknown Exception",
            MB_OK | MB_ICONERROR);

        // MiniDump 생성
        FPlatformCrashHandler::GenerateMiniDump();
        CoUninitialize(); // COM 해제
        return -1;
    }

    CoUninitialize(); // 정상 종료 시 COM 해제
    return 0;
}
