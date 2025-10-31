#include "pch.h"
#include "PlatformProcess.h"
#include <windows.h>
#include <shellapi.h>

void FPlatformProcess::OpenFileInDefaultEditor(const FString& RelativePath)
{
    // 1. 상대 경로를 기반으로 절대 경로를 생성
    //    (현재 작업 디렉토리 기준)
    fs::path AbsolutePath;
    try
    {
        AbsolutePath = fs::absolute(RelativePath);
    }
    catch (const fs::filesystem_error& e)
    {
        // 경로 변환 실패 처리 (예: GConsole->LogError(...))
        MessageBoxA(NULL, "경로 변환에 실패했습니다.", "Error", MB_OK | MB_ICONERROR);
        return;
    }

    // 2. 변환된 절대 경로를 사용
    HINSTANCE hInst = ShellExecuteA(
        NULL,
        "open",
        AbsolutePath.string().c_str(), // .string()으로 std::string을 얻어 c_str() 호출
        NULL,
        NULL,
        SW_SHOWNORMAL
    );

    if ((INT_PTR)hInst <= 32)
    {
        MessageBoxA(NULL, "파일 열기에 실패했습니다.", "Error", MB_OK | MB_ICONERROR);
    }
}