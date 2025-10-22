#pragma once
#include "Widgets/Widget.h"
#include <filesystem>

class UTexture;

class UMainToolbarWidget : public UWidget
{
public:
    DECLARE_CLASS(UMainToolbarWidget, UWidget)

    UMainToolbarWidget();

    // UWidget 인터페이스
    void Initialize() override;
    void Update() override;
    void RenderWidget() override;

private:
    // 렌더링 메서드
    void RenderToolbar();
    void RenderSceneButtons();
    void RenderActorSpawnButton();
    void RenderPIEButtons();

    // UI 헬퍼 메서드
    void BeginButtonGroup();
    void EndButtonGroup();

    // 아이콘 로딩
    void LoadToolbarIcons();

    // 이벤트 핸들러
    void OnNewScene();
    void OnSaveScene();
    void OnLoadScene();

    // 키보드 입력 처리
    void HandleKeyboardShortcuts();

    // 파일 다이얼로그 헬퍼
    std::filesystem::path OpenSaveFileDialog();
    std::filesystem::path OpenLoadFileDialog();

private:
    // 명령 타입 정의
    enum class EToolbarCommand
    {
        None,
        NewScene,
        SaveScene,
        LoadScene,
        SpawnActor,
        StartPIE,
        EndPIE
    };

    // 명령 큐 처리
    void ProcessPendingCommands();

private:
    // 아이콘 텍스처
    UTexture* IconNew = nullptr;
    UTexture* IconSave = nullptr;
    UTexture* IconLoad = nullptr;
    UTexture* IconPlay = nullptr;
    UTexture* IconStop = nullptr;
    UTexture* IconAddActor = nullptr;
    UTexture* LogoTexture = nullptr;

    // 아이콘 설정
    float IconSize = 25.0f;

    // 명령 큐
    EToolbarCommand PendingCommand = EToolbarCommand::None;
    UClass* PendingActorClass = nullptr;
};
