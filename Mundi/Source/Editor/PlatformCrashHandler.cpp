#include "pch.h"
#include "PlatformCrashHandler.h"
#include "ObjectFactory.h"
#include <dbghelp.h>
#include <time.h>
#include <stdio.h>
#include <random>

#pragma comment(lib, "dbghelp.lib")

bool FPlatformCrashHandler::bContinuousCrashMode = false;

void FPlatformCrashHandler::InitializeCrashHandler()
{
    // 전역 예외 필터 등록
    SetUnhandledExceptionFilter(UnhandledExceptionFilter);
}

bool FPlatformCrashHandler::GenerateMiniDump()
{
    // 현재 스레드 정보를 사용하여 덤프 생성
    wchar_t dumpPath[MAX_PATH];
    GetDumpFilePath(dumpPath, MAX_PATH);

    // 예외 정보 없이 현재 상태 덤프
    bool success = WriteMiniDump(nullptr, dumpPath);

    if (success)
    {
        char msg[512];
        sprintf_s(msg, "MiniDump created: %ws", dumpPath);
        MessageBoxA(nullptr, msg, "MiniDump Generated", MB_OK | MB_ICONINFORMATION);
    }
    else
    {
        MessageBoxA(nullptr, "Failed to create MiniDump", "Error", MB_OK | MB_ICONERROR);
    }

    return success;
}

void FPlatformCrashHandler::CauseIntentionalCrash()
{
    // TODO - 랜덤 크래시가 아닌 그저 테스트용이라면
    // 주석처리된 것을 이용하면 됩니다.(발제의 예시 코드)
    /*
    // RaiseException을 사용하여 의도적인 예외 발생
    // 이렇게 하면 콜스택이 명확하게 보존됩니다
    RaiseException(
        EXCEPTION_ACCESS_VIOLATION,  // 예외 코드
        EXCEPTION_NONCONTINUABLE,     // 플래그
        0,                            // 인자 개수
        nullptr                       // 인자 배열
    );
    */
    // 연속 크래시 모드 활성화
    bContinuousCrashMode = true;
}

void FPlatformCrashHandler::TickCrashMode()
{
    if (!bContinuousCrashMode)
        return;

    if (GUObjectArray.empty())
        return;

    // 매 프레임마다 랜덤 객체를 삭제하여 빠르게 크래시
    static std::random_device rd;
    static std::mt19937 gen(rd());
    // 랜덤 객체 삭제
    std::uniform_int_distribution<size_t> dist(0, GUObjectArray.size() - 1);
    size_t randomIndex = dist(gen);
    UObject* targetObject = GUObjectArray[randomIndex];
    if (targetObject)
    {
        // 매 프레임마다 랜덤한 UObject 삭제
        // 금방 크래시가 발생하며, 매번 다른 CallStack이 나옴
        ObjectFactory::DeleteObject(targetObject);
    }
}

LONG WINAPI FPlatformCrashHandler::UnhandledExceptionFilter(EXCEPTION_POINTERS* ExceptionInfo)
{
    // 크래시 발생 시 자동으로 MiniDump 생성
    wchar_t dumpPath[MAX_PATH];
    GetDumpFilePath(dumpPath, MAX_PATH);

    bool success = WriteMiniDump(ExceptionInfo, dumpPath);

    if (success)
    {
        char msg[512];
        sprintf_s(msg, "Application crashed!\n\nMiniDump saved to:\n%ws\n\nPlease send this file to developers.", dumpPath);
        MessageBoxA(nullptr, msg, "Crash Detected", MB_OK | MB_ICONERROR);
    }
    else
    {
        MessageBoxA(nullptr, "Application crashed!\n\nFailed to create MiniDump.", "Crash Detected", MB_OK | MB_ICONERROR);
    }

    // EXCEPTION_EXECUTE_HANDLER: 예외를 처리하고 프로그램 종료
    return EXCEPTION_EXECUTE_HANDLER;
}

bool FPlatformCrashHandler::WriteMiniDump(EXCEPTION_POINTERS* ExceptionInfo, const wchar_t* FilePath)
{
    // 덤프 파일 생성
    HANDLE hFile = CreateFileW(
        FilePath,
        GENERIC_WRITE,
        0,
        nullptr,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr
    );

    if (hFile == INVALID_HANDLE_VALUE)
    {
        return false;
    }

    // 예외 정보 구조체 준비
    MINIDUMP_EXCEPTION_INFORMATION exceptionInfo;
    MINIDUMP_EXCEPTION_INFORMATION* pExceptionInfo = nullptr;

    if (ExceptionInfo != nullptr)
    {
        exceptionInfo.ThreadId = GetCurrentThreadId();
        exceptionInfo.ExceptionPointers = ExceptionInfo;
        exceptionInfo.ClientPointers = FALSE;
        pExceptionInfo = &exceptionInfo;
    }

    // MiniDump 작성
    // MiniDumpWithDataSegs: 전역 변수 포함
    // MiniDumpWithHandleData: 핸들 정보 포함
    // MiniDumpWithThreadInfo: 스레드 정보 포함
    MINIDUMP_TYPE dumpType = (MINIDUMP_TYPE)(
        MiniDumpWithDataSegs |
        MiniDumpWithHandleData |
        MiniDumpWithThreadInfo
    );

    BOOL result = MiniDumpWriteDump(
        GetCurrentProcess(),
        GetCurrentProcessId(),
        hFile,
        dumpType,
        pExceptionInfo,
        nullptr,
        nullptr
    );

    CloseHandle(hFile);

    return result == TRUE;
}

void FPlatformCrashHandler::GetDumpFilePath(wchar_t* OutPath, size_t PathSize)
{
    // 현재 실행 파일 경로 가져오기
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);

    // 실행 파일이 있는 디렉토리 추출
    wchar_t dumpDir[MAX_PATH];
    wcscpy_s(dumpDir, MAX_PATH, exePath);
    wchar_t* lastSlash = wcsrchr(dumpDir, L'\\');
    if (lastSlash != nullptr)
    {
        *(lastSlash + 1) = L'\0';  // 디렉토리 경로만 남김
    }

    // 현재 시간 가져오기
    time_t rawTime;
    time(&rawTime);

    struct tm timeInfo;
    localtime_s(&timeInfo, &rawTime);

    // 파일명 생성: "Mundi_Crash_2025-11-16_14-30-25.dmp"
    wchar_t fileName[256];
    swprintf_s(fileName, 256,
        L"Mundi_Crash_%04d-%02d-%02d_%02d-%02d-%02d.dmp",
        timeInfo.tm_year + 1900,
        timeInfo.tm_mon + 1,
        timeInfo.tm_mday,
        timeInfo.tm_hour,
        timeInfo.tm_min,
        timeInfo.tm_sec
    );

    // 전체 경로 조합
    swprintf_s(OutPath, PathSize, L"%s%s", dumpDir, fileName);
}
