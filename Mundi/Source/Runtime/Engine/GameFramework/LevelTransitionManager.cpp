#include "pch.h"
#include "LevelTransitionManager.h"
#include "World.h"
#include "InputManager.h"
#include <filesystem>

IMPLEMENT_CLASS(ALevelTransitionManager)

ALevelTransitionManager::ALevelTransitionManager()
{
    ObjectName = "LevelTransitionManager";
    bCanEverTick = true;  // Tick 활성화 (Lua 스크립트 Update 호출을 위해)
}

ALevelTransitionManager::~ALevelTransitionManager()
{
    UE_LOG("[info] LevelTransitionManager: Destroyed");
}

void ALevelTransitionManager::BeginPlay()
{
    Super::BeginPlay();

    UE_LOG("[info] LevelTransitionManager: BeginPlay - Resetting transition state for new PIE session.");
    TransitionState = ELevelTransitionState::Idle;
    bPendingTransition = false;
    PendingLevelPath.clear();
}

void ALevelTransitionManager::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
}

// ════════════════════════════════════════════════════════════════════════
// 레벨 전환 API

void ALevelTransitionManager::TransitionToLevel(const FWideString& LevelPath)
{
    UE_LOG("[info] LevelTransitionManager: TransitionToLevel called with path: %ls", LevelPath.c_str());

    // 이미 전환 중이면 무시
    if (IsTransitioning() || bPendingTransition)
    {
        UE_LOG("[warning] LevelTransitionManager: Already transitioning, ignoring request");
        return;
    }

    // 경로가 비어있으면 에러
    if (LevelPath.empty())
    {
        UE_LOG("[error] LevelTransitionManager: Level path is empty");
        return;
    }

    // 파일이 존재하지 않으면 에러
    if (!DoesLevelFileExist(LevelPath))
    {
        UE_LOG("[error] LevelTransitionManager: Level file does not exist: %ls", LevelPath.c_str());
        return;
    }

    // 2. 지연 전환 예약 (다음 프레임 초에 실행 - 현재 Tick 완료 보장)
    UE_LOG("[info] LevelTransitionManager: Transition scheduled to: %ls", LevelPath.c_str());
    bPendingTransition = true;
    PendingLevelPath = LevelPath;
    TransitionState = ELevelTransitionState::Transitioning;
}

void ALevelTransitionManager::TransitionToNextLevel()
{
    UE_LOG("[info] LevelTransitionManager: TransitionToNextLevel called");

    if (NextScenePath.empty())
    {
        UE_LOG("[error] LevelTransitionManager: NextScenePath is empty! Call SetNextLevel() first.");
        return;
    }

    UE_LOG("[info] LevelTransitionManager: Transitioning to next level: %ls", NextScenePath.c_str());
    TransitionToLevel(NextScenePath);
}

void ALevelTransitionManager::ProcessPendingTransition()
{
    // bPendingTransition 플래그가 세워져 있으면 전환 실행
    if (!bPendingTransition) { return; }

    UE_LOG("[info] LevelTransitionManager: ProcessPendingTransition - Starting level transition");

    // LoadLevelFromFile() 호출 시 this가 파괴되므로, 경로를 로컬 변수에 복사
    FWideString LevelToLoad = PendingLevelPath;

    // 1. PIE 모드 확인
    if (!GEngine.IsPIEActive())
    {
        UE_LOG("[error] LevelTransitionManager: PIE is not active!");
        TransitionState = ELevelTransitionState::Idle;
        bPendingTransition = false;
        return;
    }

    // 2. PIE World 찾기
    UWorld* PIEWorld = nullptr;
    for (const auto& Context : GEngine.GetWorldContexts())
    {
        if (Context.WorldType == EWorldType::Game)
        {
            PIEWorld = Context.World;
            break;
        }
    }

    if (!PIEWorld)
    {
        UE_LOG("[error] LevelTransitionManager: Could not find PIE world context.");
        TransitionState = ELevelTransitionState::Idle;
        bPendingTransition = false;
        return;
    }

    // 3. 런타임 씬 전환 (PIE 종료 없이 직접 교체)
    UE_LOG("[info] LevelTransitionManager: Loading level from file: %ls", LevelToLoad.c_str());
    if (PIEWorld->LoadLevelFromFile(LevelToLoad) == false)
    {
        UE_LOG("[error] LevelTransitionManager: LoadLevelFromFile failed!");
        TransitionState = ELevelTransitionState::Idle;
        bPendingTransition = false;
        return;
    }

    // 4. 상태 리셋 (BeginPlay 전에 리셋해야 다음 전환 준비)
    bPendingTransition = false;
    TransitionState = ELevelTransitionState::Idle;

    // 5. 입력 모드를 기본값(GameAndUI)으로 리셋
    // 이전 씬에서 설정한 입력 모드(예: UIOnly)가 유지되는 것을 방지
    UInputManager::GetInstance().SetInputMode(EInputMode::GameAndUI);
    UE_LOG("[info] LevelTransitionManager: Input mode reset to GameAndUI");

    // 6. 새 씬의 게임플레이 시작
    UE_LOG("[info] LevelTransitionManager: BeginPlay on new level");
    PIEWorld->BeginPlay();

    UE_LOG("[info] LevelTransitionManager: Level transition completed successfully");
}

// ════════════════════════════════════════════════════════════════════════
// 유틸리티

bool ALevelTransitionManager::DoesLevelFileExist(const FWideString& LevelPath) const
{
    try
    {
        std::filesystem::path FilePath(LevelPath);
        return std::filesystem::exists(FilePath);
    }
    catch (const std::exception& e)
    {
        UE_LOG("[error] LevelTransitionManager: Exception while checking file existence: %s", e.what());
        return false;
    }
}
