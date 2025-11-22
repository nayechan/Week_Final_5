#pragma once

/**
 * 디버깅 유틸리티 클래스
 *
 * 심볼 서버 자동 설정, 크래시 덤프 분석 지원 등
 * 디버깅 관련 유틸리티 기능을 제공합니다.
 */
class FDebugUtils
{
public:
	/**
	 * 심볼 서버 자동 설정
	 *
	 * Windows 레지스트리와 환경 변수를 설정하여 Visual Studio, WinDbg 등의 디버거가
	 * 자동으로 심볼 서버에서 PDB를 다운로드하도록 합니다.
	 *
	 * 설정 방법:
	 * 1. 레지스트리 (HKEY_CURRENT_USER\Environment)에 영구 환경 변수 저장
	 * 2. 현재 프로세스의 환경 변수 설정
	 *
	 * 설정되는 환경 변수:
	 * - _NT_SYMBOL_PATH: 심볼 서버 경로 (로컬 캐시 + 네트워크 서버 + MS 서버)
	 * - _NT_SOURCE_PATH: 소스 서버 지원
	 *
	 * 효과:
	 * - 모든 IDE(Visual Studio, Rider, VSCode)에서 심볼 서버 사용 가능
	 * - 덤프 파일 디버깅 시 자동으로 PDB 다운로드
	 * - 수동 Visual Studio 설정 불필요
	 *
	 * 주의:
	 * - 프로그램 시작 시 한 번만 호출하면 됩니다
	 * - 이미 실행 중인 프로세스(예: Visual Studio)는 재시작 필요
	 * - 새로 시작되는 모든 프로세스에 자동 적용됩니다
	 */
	static void InitializeSymbolServer();

private:
	// 생성자 숨김 (정적 클래스)
	FDebugUtils() = delete;
	~FDebugUtils() = delete;
};
