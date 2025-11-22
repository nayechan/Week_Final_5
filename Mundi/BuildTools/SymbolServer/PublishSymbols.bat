@echo off
REM ============================================================================
REM Mundi 심볼 서버 수동 배포 스크립트
REM ============================================================================
REM
REM 사용법:
REM   PublishSymbols.bat [Configuration]
REM
REM 예시:
REM   PublishSymbols.bat Release
REM   PublishSymbols.bat Debug
REM   PublishSymbols.bat Debug_StandAlone
REM   PublishSymbols.bat Release_StandAlone
REM
REM Configuration이 지정되지 않으면 Release를 기본값으로 사용합니다.
REM ============================================================================

setlocal EnableDelayedExpansion

REM 빌드 구성 파라미터 처리
set CONFIGURATION=%1
if "%CONFIGURATION%"=="" (
    set CONFIGURATION=Release
    echo [INFO] Configuration not specified. Using default: Release
    echo.
)

REM 유효한 구성인지 확인
set VALID_CONFIG=0
if /I "%CONFIGURATION%"=="Debug" set VALID_CONFIG=1
if /I "%CONFIGURATION%"=="Release" set VALID_CONFIG=1
if /I "%CONFIGURATION%"=="Debug_StandAlone" set VALID_CONFIG=1
if /I "%CONFIGURATION%"=="Release_StandAlone" set VALID_CONFIG=1

if %VALID_CONFIG%==0 (
    echo [ERROR] Invalid configuration: %CONFIGURATION%
    echo.
    echo Valid configurations:
    echo   - Debug
    echo   - Release
    echo   - Debug_StandAlone
    echo   - Release_StandAlone
    echo.
    pause
    exit /b 1
)

REM 스크립트 디렉토리 경로
set SCRIPT_DIR=%~dp0
set SCRIPT_DIR=%SCRIPT_DIR:~0,-1%

REM PowerShell 스크립트 경로
set PS_SCRIPT=%SCRIPT_DIR%\IndexAndPublish.ps1

REM PowerShell 스크립트 존재 확인
if not exist "%PS_SCRIPT%" (
    echo [ERROR] PowerShell script not found: %PS_SCRIPT%
    echo.
    pause
    exit /b 1
)

echo ============================================================================
echo  Mundi Symbol Server Manual Publisher
echo ============================================================================
echo.
echo Configuration: %CONFIGURATION%
echo Script:        %PS_SCRIPT%
echo.
echo Press any key to start publishing symbols...
pause >nul
echo.

REM PowerShell 스크립트 실행
powershell.exe -ExecutionPolicy Bypass -File "%PS_SCRIPT%" -Configuration "%CONFIGURATION%"

REM 실행 결과 확인
if %ERRORLEVEL% EQU 0 (
    echo.
    echo ============================================================================
    echo  SUCCESS: Symbols published successfully
    echo ============================================================================
    echo.
) else (
    echo.
    echo ============================================================================
    echo  ERROR: Symbol publishing failed with exit code %ERRORLEVEL%
    echo ============================================================================
    echo.
)

pause
exit /b %ERRORLEVEL%
