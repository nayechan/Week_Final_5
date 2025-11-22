#include "pch.h"
#include "DebugUtils.h"
#include <cstdlib>
#include <cstdio>

#ifdef _WIN32
#include <Windows.h>
#endif

void FDebugUtils::InitializeSymbolServer()
{
#ifdef _WIN32
	// 심볼 서버 경로 설정
	// 형식: SRV*[로컬캐시]*[심볼서버1]*[심볼서버2]*...
	const wchar_t* symbolPath =
		L"SRV*"
		L"\\\\172.21.11.114\\MundiSymbols\\Symbols";

	const wchar_t* sourcePath = L"SRV*";

	// 레지스트리에 사용자 환경 변수 설정
	// HKEY_CURRENT_USER\Environment에 저장하여 모든 프로세스에서 사용 가능
	HKEY hKey;
	LONG result = RegOpenKeyExW(
		HKEY_CURRENT_USER,
		L"Environment",
		0,
		KEY_SET_VALUE,
		&hKey
	);

	if (result == ERROR_SUCCESS)
	{
		// _NT_SYMBOL_PATH 설정
		RegSetValueExW(
			hKey,
			L"_NT_SYMBOL_PATH",
			0,
			REG_SZ,
			(const BYTE*)symbolPath,
			(DWORD)((wcslen(symbolPath) + 1) * sizeof(wchar_t))
		);

		// _NT_SOURCE_PATH 설정
		RegSetValueExW(
			hKey,
			L"_NT_SOURCE_PATH",
			0,
			REG_SZ,
			(const BYTE*)sourcePath,
			(DWORD)((wcslen(sourcePath) + 1) * sizeof(wchar_t))
		);

		RegCloseKey(hKey);

		// 시스템에 환경 변수 변경 알림
		// 이미 실행 중인 프로세스는 영향받지 않지만, 새로 시작되는 프로세스는 적용됨
		SendMessageTimeoutW(
			HWND_BROADCAST,
			WM_SETTINGCHANGE,
			0,
			(LPARAM)L"Environment",
			SMTO_ABORTIFHUNG,
			5000,
			nullptr
		);

#ifdef _DEBUG
		// 디버그 빌드에서만 로그 출력
		UE_LOG("[DebugUtils] Symbol server initialized\n");
		UE_LOG("  Registry key: HKEY_CURRENT_USER\\Environment\n");
		UE_LOG("  _NT_SYMBOL_PATH = %ls\n", symbolPath);
		UE_LOG("  _NT_SOURCE_PATH = %ls\n", sourcePath);
		UE_LOG("  Note: Restart Visual Studio to apply changes\n");
#endif
	}
	else
	{
		UE_LOG("[DebugUtils] Failed to open registry key (error code: %ld)\n", result);
	}

	// 현재 프로세스의 환경 변수도 설정 (호환성)
	_wputenv_s(L"_NT_SYMBOL_PATH", symbolPath);
	_wputenv_s(L"_NT_SOURCE_PATH", sourcePath);

#endif // _WIN32
}
