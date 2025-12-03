// ────────────────────────────────────────────────────────────────────────────
// HudExampleGameMode.h
// SGameHUD 사용 예제 게임 모드
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include "GameModeBase.h"
#include "Source/Runtime/Core/Memory/PointerTypes.h"
#include "AHudExampleGameMode.generated.h"

class SButton;
class STextBlock;

/**
 * AHudExampleGameMode
 *
 * SGameHUD를 사용하여 게임 UI를 구성하는 예제 게임 모드입니다.
 */
UCLASS(DisplayName="HUD 예제 게임 모드", Description="SGameHUD 사용 예제 게임 모드입니다.")
class AHudExampleGameMode : public AGameModeBase
{
public:
	GENERATED_REFLECTION_BODY()

	AHudExampleGameMode();

protected:
	~AHudExampleGameMode() override = default;

public:
	// ────────────────────────────────────────────────
	// 생명주기
	// ────────────────────────────────────────────────

	void BeginPlay() override;
	void EndPlay() override;
	void Tick(float DeltaSeconds) override;

	// ────────────────────────────────────────────────
	// 게임 라이프사이클
	// ────────────────────────────────────────────────

	void StartGame() override;

	// ────────────────────────────────────────────────
	// UI 업데이트
	// ────────────────────────────────────────────────

	/** 점수 UI 업데이트 */
	void UpdateScoreUI(int32 Score);

protected:
	// ────────────────────────────────────────────────
	// UI 위젯
	// ────────────────────────────────────────────────

	TSharedPtr<SButton> StartButton;
	TSharedPtr<STextBlock> ScoreText;
	TSharedPtr<STextBlock> SpeedText;
	TSharedPtr<STextBlock> RpmText;
	TSharedPtr<STextBlock> GearText;
};
