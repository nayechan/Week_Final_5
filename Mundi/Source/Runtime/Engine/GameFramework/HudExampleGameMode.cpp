// ────────────────────────────────────────────────────────────────────────────
// HudExampleGameMode.cpp
// SGameHUD 사용 예제 게임 모드 구현
// ────────────────────────────────────────────────────────────────────────────
#include "pch.h"
#include "HudExampleGameMode.h"
#include "DancingCharacter.h"
#include "Vehicle.h"

#include "GameUI/SGameHUD.h"
#include "GameUI/SButton.h"
#include "GameUI/STextBlock.h"
#include"PlayerController.h"

// ────────────────────────────────────────────────────────────────────────────
// 생성자
// ────────────────────────────────────────────────────────────────────────────

AHudExampleGameMode::AHudExampleGameMode()
{
	// DefaultPawnClass = ADancingCharacter::StaticClass();
	DefaultPawnClass = AVehicle::StaticClass();
}

// ────────────────────────────────────────────────────────────────────────────
// 생명주기
// ────────────────────────────────────────────────────────────────────────────

void AHudExampleGameMode::BeginPlay()
{
	Super::BeginPlay();

	if (!SGameHUD::Get().IsInitialized())
		return;

	// 이미 위젯이 생성되어 있으면 중복 생성 방지
	if (SpeedText)
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

	// 속도 텍스트 (우하단 - 자동차 게임 스타일)
	SpeedText = MakeShared<STextBlock>();
	SpeedText->SetText(L"0 km/h")
		.SetFontSize(48.f)
		.SetColor(FSlateColor::White())
		.SetShadow(true, FVector2D(3.f, 3.f), FSlateColor::Black());

	SGameHUD::Get().AddWidget(SpeedText)
		.SetAnchor(1.f, 1.f)      // 우하단 앵커
		.SetPivot(1.f, 1.f)       // 우하단 피벗
		.SetOffset(-40.f, -40.f)  // 화면 가장자리에서 40픽셀 안쪽
		.SetSize(300.f, 70.f);

	// 기어 텍스트 (속도 위)
	GearText = MakeShared<STextBlock>();
	GearText->SetText(L"N")
		.SetFontSize(36.f)
		.SetColor(FSlateColor(1.f, 0.8f, 0.2f, 1.f))  // 노란색
		.SetShadow(true, FVector2D(2.f, 2.f), FSlateColor::Black());

	SGameHUD::Get().AddWidget(GearText)
		.SetAnchor(1.f, 1.f)
		.SetPivot(1.f, 1.f)
		.SetOffset(-40.f, -110.f)  // 속도 위
		.SetSize(100.f, 50.f);

	// RPM 텍스트 (기어 위)
	RpmText = MakeShared<STextBlock>();
	RpmText->SetText(L"0 RPM")
		.SetFontSize(24.f)
		.SetColor(FSlateColor(0.8f, 0.8f, 0.8f, 1.f))  // 밝은 회색
		.SetShadow(true, FVector2D(2.f, 2.f), FSlateColor::Black());

	SGameHUD::Get().AddWidget(RpmText)
		.SetAnchor(1.f, 1.f)
		.SetPivot(1.f, 1.f)
		.SetOffset(-40.f, -160.f)  // 기어 위
		.SetSize(200.f, 40.f);
}

void AHudExampleGameMode::EndPlay()
{
	Super::EndPlay();

	// HUD 위젯 정리
	if (SGameHUD::Get().IsInitialized())
	{
		if (StartButton)
		{
			SGameHUD::Get().RemoveWidget(StartButton);
			StartButton.Reset();
		}
		if (ScoreText)
		{
			SGameHUD::Get().RemoveWidget(ScoreText);
			ScoreText.Reset();
		}
		if (SpeedText)
		{
			SGameHUD::Get().RemoveWidget(SpeedText);
			SpeedText.Reset();
		}
		if (RpmText)
		{
			SGameHUD::Get().RemoveWidget(RpmText);
			RpmText.Reset();
		}
		if (GearText)
		{
			SGameHUD::Get().RemoveWidget(GearText);
			GearText.Reset();
		}
	}
}

// ────────────────────────────────────────────────────────────────────────────
// 게임 라이프사이클
// ────────────────────────────────────────────────────────────────────────────

void AHudExampleGameMode::StartGame()
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

void AHudExampleGameMode::UpdateScoreUI(int32 Score)
{
	if (ScoreText)
	{
		ScoreText->SetText(L"점수: " + std::to_wstring(Score));
	}
}

void AHudExampleGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!GetPlayerController()) return;

	// 현재 플레이어의 Pawn이 Vehicle인지 확인
	APawn* PlayerPawn = GetPlayerController()->GetPawn();
	if (!PlayerPawn) return;

	AVehicle* Vehicle = Cast<AVehicle>(PlayerPawn);
	if (!Vehicle || !Vehicle->VehicleMovement) return;

	// 속도 UI 업데이트
	if (SpeedText)
	{
		float SpeedMs = Vehicle->VehicleMovement->GetForwardSpeed();
		float SpeedKmh = std::abs(SpeedMs) * 3.6f;
		int32 SpeedInt = static_cast<int32>(SpeedKmh);
		SpeedText->SetText(std::to_wstring(SpeedInt) + L" km/h");
	}

	// RPM UI 업데이트
	if (RpmText)
	{
		float Rpm = Vehicle->VehicleMovement->GetEngineRPM();
		int32 RpmInt = static_cast<int32>(Rpm);
		RpmText->SetText(std::to_wstring(RpmInt) + L" RPM");
	}

	// 기어 UI 업데이트
	if (GearText)
	{
		int32 Gear = Vehicle->VehicleMovement->GetCurrentGear();
		std::wstring GearStr;
		if (Gear < 0)
			GearStr = L"R";
		else if (Gear == 0)
			GearStr = L"N";
		else
			GearStr = std::to_wstring(Gear);

		GearText->SetText(GearStr);
	}
}
