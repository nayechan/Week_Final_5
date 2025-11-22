<#
.SYNOPSIS
    Mundi 엔진 심볼 서버 인덱싱 및 배포 스크립트

.DESCRIPTION
    빌드 후 PDB 파일에 Git 소스 정보를 인덱싱하고,
    symstore를 통해 심볼 서버에 배포합니다.

    실행 단계:
    1. PDB 파일 존재 확인
    2. Git 정보 수집 (커밋, 브랜치, 원격 URL)
    3. srctool.exe로 소스 파일 목록 추출
    4. SourceIndex.py로 소스 서버 스트림 생성
    5. pdbstr.exe로 PDB에 소스 인덱싱 정보 삽입
    6. symstore.exe로 심볼 서버에 PDB 배포

.PARAMETER Configuration
    빌드 구성 (Debug, Release, Debug_StandAlone, Release_StandAlone)

.PARAMETER SymbolServerPath
    심볼 서버 UNC 경로 (기본값: 설정 파일에서 읽기)

.PARAMETER GitRepoRoot
    Git 저장소 루트 경로 (기본값: 자동 감지)

.PARAMETER WinDbgKitPath
    Windows Debugging Tools 경로 (기본값: 설정 파일에서 읽기)

.PARAMETER SkipSourceIndex
    소스 인덱싱 건너뛰기 (PDB만 배포)

.EXAMPLE
    .\IndexAndPublish.ps1 -Configuration Release

.EXAMPLE
    .\IndexAndPublish.ps1 -Configuration Debug -SkipSourceIndex

.NOTES
    요구사항:
    - Windows Debugging Tools for Windows (srctool.exe, pdbstr.exe, symstore.exe)
    - Python 3.x
    - Git
#>

param(
    [Parameter(Mandatory=$true)]
    [ValidateSet("Debug", "Release", "Debug_StandAlone", "Release_StandAlone")]
    [string]$Configuration,

    [string]$SymbolServerPath = "",

    [string]$GitRepoRoot = "",

    [string]$WinDbgKitPath = "",

    [switch]$SkipSourceIndex = $false
)

$ErrorActionPreference = "Stop"

# ===== 스크립트 위치 및 프로젝트 루트 경로 =====
$ScriptDir = $PSScriptRoot
# ScriptDir: Mundi\BuildTools\SymbolServer
# -> Mundi\BuildTools -> Mundi -> Krafton_TechLab_Week12 (솔루션 루트)
$ProjectRoot = Split-Path (Split-Path (Split-Path $ScriptDir -Parent) -Parent) -Parent
$ConfigFile = Join-Path $ScriptDir "SymbolServerConfig.json"

Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
Write-Host " Mundi Symbol Server Publisher" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

# ===== 1. 설정 파일 로드 =====
Write-Host "[Config] Loading configuration..." -ForegroundColor Yellow

if (-not (Test-Path $ConfigFile)) {
    Write-Error "Configuration file not found: $ConfigFile"
    exit 1
}

try {
    $Config = Get-Content $ConfigFile -Raw | ConvertFrom-Json
    Write-Host "[Config] Configuration loaded successfully" -ForegroundColor Green
}
catch {
    Write-Error "Failed to parse configuration file: $_"
    exit 1
}

# ===== 2. 경로 설정 =====
if ([string]::IsNullOrEmpty($GitRepoRoot)) {
    # Git 저장소 루트 자동 감지
    Push-Location $ProjectRoot
    try {
        $GitRepoRoot = git rev-parse --show-toplevel 2>$null
        if ($LASTEXITCODE -ne 0) {
            $GitRepoRoot = $ProjectRoot
        }
        else {
            # Git bash 경로를 Windows 경로로 변환
            $GitRepoRoot = $GitRepoRoot -replace '/', '\'
        }
    }
    finally {
        Pop-Location
    }
}

if ([string]::IsNullOrEmpty($SymbolServerPath)) {
    $SymbolServerPath = $Config.symbolServer.path
}

if ([string]::IsNullOrEmpty($WinDbgKitPath)) {
    $WinDbgKitPath = $Config.winDbgKit.path
}

$BinariesDir = Join-Path $ProjectRoot "Binaries\$Configuration"
$ExePath = Join-Path $BinariesDir "Mundi.exe"
$PdbPath = Join-Path $BinariesDir "Mundi.pdb"

# 설정 파일의 tools 경로 사용 (srcsrv 하위 폴더 지원)
$SrctoolExe = Join-Path $WinDbgKitPath $Config.winDbgKit.tools.srctool
$PdbstrExe = Join-Path $WinDbgKitPath $Config.winDbgKit.tools.pdbstr
$SymstoreExe = Join-Path $WinDbgKitPath $Config.winDbgKit.tools.symstore
$SourceIndexScript = Join-Path $ScriptDir "SourceIndex.py"

Write-Host "[Paths] Git Repository: $GitRepoRoot" -ForegroundColor White
Write-Host "[Paths] Binaries: $BinariesDir" -ForegroundColor White
Write-Host "[Paths] Symbol Server: $SymbolServerPath" -ForegroundColor White
Write-Host ""

# ===== 3. 사전 검증 =====
Write-Host "[Validation] Checking prerequisites..." -ForegroundColor Yellow

# PDB 파일 확인
if (-not (Test-Path $PdbPath)) {
    Write-Error "PDB file not found: $PdbPath"
    exit 1
}
Write-Host "[Validation] PDB found: $(Split-Path $PdbPath -Leaf)" -ForegroundColor Green

# EXE 파일 확인
if (-not (Test-Path $ExePath)) {
    Write-Warning "EXE file not found: $ExePath"
}
else {
    Write-Host "[Validation] EXE found: $(Split-Path $ExePath -Leaf)" -ForegroundColor Green
}

# Windows Debugging Tools 확인
if (-not (Test-Path $SrctoolExe)) {
    Write-Error "srctool.exe not found. Install Windows Debugging Tools for Windows.`nExpected path: $SrctoolExe"
    exit 1
}
if (-not (Test-Path $PdbstrExe)) {
    Write-Error "pdbstr.exe not found. Install Windows Debugging Tools for Windows.`nExpected path: $PdbstrExe"
    exit 1
}
if (-not (Test-Path $SymstoreExe)) {
    Write-Error "symstore.exe not found. Install Windows Debugging Tools for Windows.`nExpected path: $SymstoreExe"
    exit 1
}
Write-Host "[Validation] Windows Debugging Tools found" -ForegroundColor Green

# Python 확인
$PythonExe = (Get-Command python -ErrorAction SilentlyContinue).Source
if (-not $PythonExe) {
    Write-Error "Python not found. Install Python 3.x and add to PATH."
    exit 1
}
Write-Host "[Validation] Python found: $PythonExe" -ForegroundColor Green

# 소스 인덱싱 스크립트 확인
if (-not (Test-Path $SourceIndexScript)) {
    Write-Error "SourceIndex.py not found: $SourceIndexScript"
    exit 1
}
Write-Host "[Validation] SourceIndex.py found" -ForegroundColor Green

# 심볼 서버 경로 확인
if ($SymbolServerPath -eq "\\YOUR_SERVER_IP\MundiSymbols\Symbols") {
    Write-Error "Symbol server path is not configured. Please edit SymbolServerConfig.json and set the correct path."
    exit 1
}

Write-Host ""

# ===== 4. Git 정보 수집 =====
Write-Host "[Git] Collecting repository information..." -ForegroundColor Yellow

Push-Location $GitRepoRoot
try {
    # Git 커밋 해시
    $GitCommit = git rev-parse HEAD 2>$null
    if ($LASTEXITCODE -ne 0) {
        Write-Warning "Failed to get Git commit hash. Using 'unknown'."
        $GitCommit = "unknown"
    }

    # Git 브랜치
    $GitBranch = git rev-parse --abbrev-ref HEAD 2>$null
    if ($LASTEXITCODE -ne 0) {
        Write-Warning "Failed to get Git branch. Using 'unknown'."
        $GitBranch = "unknown"
    }

    # Git remote URL
    $GitRemote = git remote get-url origin 2>$null
    if ($LASTEXITCODE -ne 0) {
        Write-Warning "Failed to get Git remote URL. Using 'local'."
        $GitRemote = "local"
    }

    Write-Host "[Git] Commit:  $GitCommit" -ForegroundColor Green
    Write-Host "[Git] Branch:  $GitBranch" -ForegroundColor Green
    Write-Host "[Git] Remote:  $GitRemote" -ForegroundColor Green
}
finally {
    Pop-Location
}

Write-Host ""

# ===== 5. 소스 파일 목록 추출 (srctool.exe) =====
if (-not $SkipSourceIndex) {
    Write-Host "[srctool] Extracting source file list from PDB..." -ForegroundColor Yellow

    $SourceListFile = Join-Path $env:TEMP "mundi_sources_$Configuration.txt"

    try {
        & $SrctoolExe -r $PdbPath | Out-File -Encoding ASCII -FilePath $SourceListFile

        if ($LASTEXITCODE -ne 0) {
            Write-Warning "srctool.exe returned non-zero exit code: $LASTEXITCODE"
        }

        if (-not (Test-Path $SourceListFile)) {
            Write-Error "Failed to extract source list"
            exit 1
        }

        $SourceCount = (Get-Content $SourceListFile | Where-Object { $_.Trim() -ne "" }).Count
        Write-Host "[srctool] Found $SourceCount source files" -ForegroundColor Green
    }
    catch {
        Write-Error "srctool.exe failed: $_"
        exit 1
    }

    Write-Host ""

    # ===== 6. 소스 인덱싱 스트림 생성 (SourceIndex.py) =====
    Write-Host "[SourceIndex] Generating source indexing stream..." -ForegroundColor Yellow

    $StreamFile = Join-Path $env:TEMP "mundi_srcsrv_$Configuration.txt"

    try {
        & python $SourceIndexScript `
            --source-list $SourceListFile `
            --git-repo $GitRepoRoot `
            --git-commit $GitCommit `
            --git-remote $GitRemote `
            --output $StreamFile

        if ($LASTEXITCODE -ne 0) {
            Write-Error "SourceIndex.py failed with exit code $LASTEXITCODE"
            exit 1
        }

        if (-not (Test-Path $StreamFile)) {
            Write-Error "Failed to generate source index stream"
            exit 1
        }

        Write-Host "[SourceIndex] Stream file generated: $StreamFile" -ForegroundColor Green
    }
    catch {
        Write-Error "SourceIndex.py execution failed: $_"
        exit 1
    }

    Write-Host ""

    # ===== 7. PDB에 소스 인덱싱 정보 삽입 (pdbstr.exe) =====
    Write-Host "[pdbstr] Writing source index stream to PDB..." -ForegroundColor Yellow

    try {
        & $PdbstrExe -w -p:$PdbPath -i:$StreamFile -s:srcsrv

        if ($LASTEXITCODE -ne 0) {
            Write-Error "pdbstr.exe failed with exit code $LASTEXITCODE"
            exit 1
        }

        Write-Host "[pdbstr] Source indexing complete" -ForegroundColor Green
    }
    catch {
        Write-Error "pdbstr.exe execution failed: $_"
        exit 1
    }

    # 임시 파일 정리
    Remove-Item $SourceListFile -Force -ErrorAction SilentlyContinue
    Remove-Item $StreamFile -Force -ErrorAction SilentlyContinue
}
else {
    Write-Host "[SourceIndex] Skipped (SkipSourceIndex flag set)" -ForegroundColor Gray
}

Write-Host ""

# ===== 8. 심볼 서버에 배포 (symstore.exe) =====
Write-Host "[symstore] Publishing symbols to server..." -ForegroundColor Yellow

$Version = "$Configuration-$GitBranch-$(Get-Date -Format 'yyyyMMdd-HHmmss')"
$Comment = "Mundi $Configuration | Commit: $GitCommit | Branch: $GitBranch"
$Product = $Config.symbolServer.product

Write-Host "[symstore] Product: $Product" -ForegroundColor White
Write-Host "[symstore] Version: $Version" -ForegroundColor White
Write-Host "[symstore] Comment: $Comment" -ForegroundColor White

try {
    # /r: 재귀적으로 파일 추가
    # /f: 추가할 파일 또는 디렉토리
    # /s: 심볼 스토어 경로
    # /t: 제품 이름
    # /v: 버전 문자열
    # /c: 주석
    & $SymstoreExe add /r /f $BinariesDir\*.pdb /s $SymbolServerPath /t $Product /v $Version /c $Comment

    if ($LASTEXITCODE -ne 0) {
        Write-Error "symstore.exe failed with exit code $LASTEXITCODE"
        exit 1
    }

    Write-Host "[symstore] Symbols published successfully" -ForegroundColor Green
}
catch {
    Write-Error "symstore.exe execution failed: $_"
    exit 1
}

Write-Host ""

# ===== 9. 완료 메시지 =====
Write-Host "========================================" -ForegroundColor Cyan
Write-Host " Symbol Server Publish Complete" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "Configuration:  $Configuration" -ForegroundColor White
Write-Host "PDB File:       $PdbPath" -ForegroundColor White
Write-Host "Symbol Server:  $SymbolServerPath" -ForegroundColor White
Write-Host "Git Commit:     $GitCommit" -ForegroundColor White
Write-Host "Git Branch:     $GitBranch" -ForegroundColor White
Write-Host "Version:        $Version" -ForegroundColor White
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""
