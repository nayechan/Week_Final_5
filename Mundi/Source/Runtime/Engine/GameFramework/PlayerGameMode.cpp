// ────────────────────────────────────────────────────────────────────────────
// HudExampleGameMode.cpp
// SGameHUD 사용 예제 게임 모드 구현
// ────────────────────────────────────────────────────────────────────────────
#include "pch.h"
#include "PlayerGameMode.h"
#include "DancingCharacter.h"
#include "Vehicle.h"
#include "Character.h"
#include "SkeletalMeshComponent.h"
#include "EPhysicsMode.h"
#include "InputManager.h"
#include "PlayerController.h"

#include "GameUI/SGameHUD.h"
#include "GameUI/SButton.h"
#include "GameUI/STextBlock.h"

// ────────────────────────────────────────────────────────────────────────────
// 생성자
// ────────────────────────────────────────────────────────────────────────────

APlayerGameMode::APlayerGameMode()
{
	// DefaultPawnClass = ADancingCharacter::StaticClass();
	DefaultPawnClass = ADancingCharacter::StaticClass();
}

// ────────────────────────────────────────────────────────────────────────────
// 생명주기
// ────────────────────────────────────────────────────────────────────────────

void APlayerGameMode::BeginPlay()
{
	Super::BeginPlay();

	if (!SGameHUD::Get().IsInitialized())
		return;

	// 시작 버튼 (화면 중앙)
	/*StartButton = MakeShared<SButton>();
	StartButton->SetText(L"게임 시작")
		.SetBackgroundColor(FSlateColor(0.2f, 0.5f, 0.8f, 1.f))
		.SetFontSize(24.f)
		.SetCornerRadius(8.f)
		.OnClicked([this]() {
			StartGame();
		});

	SGameHUD::Get().AddWidget(StartButton)
		.SetAnchor(0.5f, 0.5f)
		.SetPivot(0.5f, 0.5f)
		.SetOffset(0.f, 0.f)
		.SetSize(200.f, 60.f);*/

	// 점수 텍스트 (좌상단)
	ScoreText = MakeShared<STextBlock>();
	ScoreText->SetText(L"점수: 0")
		.SetFontSize(32.f)
		.SetColor(FSlateColor::White())
		.SetShadow(true, FVector2D(2.f, 2.f), FSlateColor::Black());

	SGameHUD::Get().AddWidget(ScoreText)
		.SetAnchor(0.f, 0.f)
		.SetOffset(20.f, 20.f)
		.SetSize(300.f, 50.f);
}

// ────────────────────────────────────────────────────────────────────────────
// 게임 라이프사이클
// ────────────────────────────────────────────────────────────────────────────

void APlayerGameMode::StartGame()
{
	Super::StartGame();

	// 게임 시작 시 시작 버튼 숨기기
	if (StartButton)
	{
		StartButton->SetVisibility(ESlateVisibility::Hidden);
	}
}

// ────────────────────────────────────────────────────────────────────────────
// UI 업데이트
// ────────────────────────────────────────────────────────────────────────────

void APlayerGameMode::UpdateScoreUI(int32 Score)
{
	if (ScoreText)
	{
		ScoreText->SetText(L"점수: " + std::to_wstring(Score));
	}
}

void APlayerGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// R키로 래그돌/키네마틱 토글
	if (UInputManager::GetInstance().IsKeyPressed('R'))
	{
		if (APlayerController* PC = GetPlayerController())
		{
			if (APawn* PlayerPawn = PC->GetPawn())
			{
				if (ACharacter* Character = Cast<ACharacter>(PlayerPawn))
				{
					if (USkeletalMeshComponent* Mesh = Character->GetMesh())
					{
						EPhysicsMode CurrentMode = Mesh->GetPhysicsMode();
						if (CurrentMode == EPhysicsMode::Ragdoll)
						{
							Mesh->SetPhysicsMode(EPhysicsMode::Kinematic);
							UE_LOG("Physics Mode: Kinematic");
						}
						else
						{
							Mesh->SetPhysicsMode(EPhysicsMode::Ragdoll);
							UE_LOG("Physics Mode: Ragdoll");
						}
					}
				}
			}
		}
	}
}
