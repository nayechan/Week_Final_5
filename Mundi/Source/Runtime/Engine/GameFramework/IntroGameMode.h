#pragma once
#include "GameModeBase.h"
#include "Source/Runtime/Core/Memory/PointerTypes.h"
#include "AIntroGameMode.generated.h"

class SButton;
class STextBlock;

/**
 * 인트로 씬 전용 GameMode
 *
 * 기능:
 * - "게임 시작" 버튼: 다음 씬으로 전환
 * - "나가기" 버튼: 게임 종료
 * - 버튼에 스프라이트 적용 가능
 * - 카메라 입력 비활성화: 인트로 씬에서는 카메라가 고정됨
 */
UCLASS(DisplayName="인트로 게임 모드", Description="인트로 씬용 게임 시작/종료 버튼 제공")
class AIntroGameMode : public AGameModeBase
{
public:
    GENERATED_REFLECTION_BODY()

    AIntroGameMode();
    virtual ~AIntroGameMode() override = default;

    // ════════════════════════════════════════════════════════════════════════
    // 설정

    /**
     * 다음 씬 경로 (게임 시작 버튼 클릭 시 이동)
     */
    UPROPERTY(EditAnywhere)
    FWideString NextScenePath = L"Data/Scenes/ClothTest.scene";

    /**
     * 게임 시작 버튼 텍스트
     */
    UPROPERTY(EditAnywhere)
    FWideString StartButtonText = L"";

    /**
     * 나가기 버튼 텍스트
     */
    UPROPERTY(EditAnywhere)
    FWideString QuitButtonText = L"";

    /**
     * 타이틀 텍스트
     */
    UPROPERTY(EditAnywhere)
    FWideString TitleText = L"";

    // ════════════════════════════════════════════════════════════════════════
    // 생명주기

    virtual void BeginPlay() override;
    virtual void EndPlay() override;

private:
    // UI 위젯
    TSharedPtr<STextBlock> TitleWidget;
    TSharedPtr<SButton> StartButton;
    TSharedPtr<SButton> QuitButton;

    // UI 초기화
    void InitializeUI();

    // 버튼 클릭 핸들러
    static void OnStartButtonClicked();
    static void OnQuitButtonClicked();

    // UI 정리
    void ClearUI();
};
