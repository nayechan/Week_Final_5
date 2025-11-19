#include "pch.h"
#include "EditorEngine.h"
#include "PlatformCrashHandler.h"
#include <exception>

#if defined(_MSC_VER) && defined(_DEBUG)
#   define _CRTDBG_MAP_ALLOC
#   include <cstdlib>
#   include <crtdbg.h>
#endif

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{   
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
        char msg[512];
        sprintf_s(msg, "C++ Exception caught:\n\n%s\n\nGenerating MiniDump...", e.what());
        MessageBoxA(nullptr, msg, "C++ Exception", MB_OK | MB_ICONERROR);

        // MiniDump 생성
        FPlatformCrashHandler::GenerateMiniDump();
        return -1;
    }
    catch (...)
    {
        // 알 수 없는 예외 처리 (throw int, throw custom_type 등)
        MessageBoxA(nullptr,
            "Unknown C++ exception caught!\n\nGenerating MiniDump...",
            "Unknown Exception",
            MB_OK | MB_ICONERROR);

        // MiniDump 생성
        FPlatformCrashHandler::GenerateMiniDump();
        return -1;
    }

    return 0;
}
