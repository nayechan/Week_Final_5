#include "pch.h"
#include "MainToolbarWidget.h"
#include "ImGui/imgui.h"
#include "Texture.h"
#include "ResourceManager.h"
#include "Object.h"
#include "UIManager.h"
#include "World.h"
#include "Level.h"
#include "JsonSerializer.h"
#include "SelectionManager.h"
#include <EditorEngine.h>
#include <filesystem>
#include <windows.h>
#include <commdlg.h>

IMPLEMENT_CLASS(UMainToolbarWidget)

UMainToolbarWidget::UMainToolbarWidget()
    : UWidget("Main Toolbar")
{
}

void UMainToolbarWidget::Initialize()
{
    LoadToolbarIcons();
}

void UMainToolbarWidget::Update()
{
    // 키보드 단축키 처리
    HandleKeyboardShortcuts();

    // 대기 중인 명령 처리
    ProcessPendingCommands();
}

void UMainToolbarWidget::RenderWidget()
{
    RenderToolbar();
}

void UMainToolbarWidget::LoadToolbarIcons()
{
    // 아이콘 로딩 (사용자가 파일을 제공할 예정)
    IconNew = UResourceManager::GetInstance().Load<UTexture>("Data/Icon/Toolbar_New.png");
    IconSave = UResourceManager::GetInstance().Load<UTexture>("Data/Icon/Toolbar_Save.png");
    IconLoad = UResourceManager::GetInstance().Load<UTexture>("Data/Icon/Toolbar_Load.png");
    IconPlay = UResourceManager::GetInstance().Load<UTexture>("Data/Icon/Toolbar_Play.png");
    IconStop = UResourceManager::GetInstance().Load<UTexture>("Data/Icon/Toolbar_Stop.png");
    IconAddActor = UResourceManager::GetInstance().Load<UTexture>("Data/Icon/Toolbar_AddActor.png");
    LogoTexture = UResourceManager::GetInstance().Load<UTexture>("Data/Icon/Mundi_Logo.png");
}

void UMainToolbarWidget::RenderToolbar()
{
    // 툴바 윈도우 설정
    const float ToolbarHeight = 50.0f;
    ImVec2 ToolbarPos(0, 0);
    ImVec2 ToolbarSize(ImGui::GetIO().DisplaySize.x, ToolbarHeight);

    ImGui::SetNextWindowPos(ToolbarPos);
    ImGui::SetNextWindowSize(ToolbarSize);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration |
                             ImGuiWindowFlags_NoMove |
                             ImGuiWindowFlags_NoSavedSettings |
                             ImGuiWindowFlags_NoBringToFrontOnFocus |
                             ImGuiWindowFlags_NoScrollWithMouse |
                             ImGuiWindowFlags_NoScrollbar;

    if (ImGui::Begin("##MainToolbar", nullptr, flags))
    {
        // 상단 테두리 박스 렌더링
        ImVec2 windowPos = ImGui::GetWindowPos();
        ImVec2 windowSize = ImGui::GetWindowSize();
        const float BoxHeight = 8.0f;

        ImGui::GetWindowDrawList()->AddRectFilled(
            windowPos,
            ImVec2(windowPos.x + windowSize.x, windowPos.y + BoxHeight),
            ImGui::GetColorU32(ImVec4(0.15f, 0.45f, 0.25f, 1.0f))  // 진한 초록색 악센트
        );

        // 수직 중앙 정렬
        float cursorY = (ToolbarHeight - IconSize) / 2.0f;
        ImGui::SetCursorPosY(cursorY);
        ImGui::SetCursorPosX(8.0f); // 왼쪽 여백

        // Scene 관리 버튼들
        RenderSceneButtons();

        // 구분선
        ImGui::SameLine(0, 12.0f);
        ImVec2 separatorStart = ImGui::GetCursorScreenPos();
        separatorStart.y += 4.0f;  // 5픽셀 아래로 이동
        ImGui::GetWindowDrawList()->AddLine(
            separatorStart,
            ImVec2(separatorStart.x, separatorStart.y + IconSize),
            ImGui::GetColorU32(ImVec4(0.25f, 0.25f, 0.25f, 0.8f)),
            2.0f
        );
        ImGui::Dummy(ImVec2(2.0f, IconSize));

        // Actor Spawn 버튼
        ImGui::SameLine(0, 12.0f);
        RenderActorSpawnButton();

        // 구분선
        ImGui::SameLine(0, 12.0f);
        separatorStart = ImGui::GetCursorScreenPos();
        separatorStart.y += 4.0f;  // 5픽셀 아래로 이동
        ImGui::GetWindowDrawList()->AddLine(
            separatorStart,
            ImVec2(separatorStart.x, separatorStart.y + IconSize),
            ImGui::GetColorU32(ImVec4(0.25f, 0.25f, 0.25f, 0.8f)),
            2.0f
        );
        ImGui::Dummy(ImVec2(2.0f, IconSize));

        // PIE 제어 버튼들
        ImGui::SameLine(0, 12.0f);
        RenderPIEButtons();

        // 로고를 오른쪽에 배치
        if (LogoTexture && LogoTexture->GetShaderResourceView())
        {
            const float LogoHeight = ToolbarHeight * 0.9f;  // 툴바 높이의 70%
            const float LogoWidth = LogoHeight * 3.42f;     // 820:240 비율
            const float RightPadding = 16.0f;

            ImVec2 logoPos;
            logoPos.x = ImGui::GetWindowWidth() - LogoWidth - RightPadding;
            logoPos.y = (ToolbarHeight - LogoHeight) / 2.0f;

            ImGui::SetCursorPos(logoPos);
            ImGui::Image((void*)LogoTexture->GetShaderResourceView(), ImVec2(LogoWidth, LogoHeight));
        }
    }
    ImGui::End();
}

void UMainToolbarWidget::BeginButtonGroup()
{
    ImGui::BeginGroup();
}

void UMainToolbarWidget::EndButtonGroup()
{
    ImGui::EndGroup();

    // 그룹 영역 계산
    ImVec2 groupMin = ImGui::GetItemRectMin();
    ImVec2 groupMax = ImGui::GetItemRectMax();

    const float Padding = 1.0f;
    groupMin.x -= Padding;
    groupMin.y -= Padding;
    groupMax.x += Padding;
    groupMax.y += Padding;

    ImDrawList* drawList = ImGui::GetWindowDrawList();

    // 테두리만 그리기 (배경 제거)
    drawList->AddRect(
        groupMin,
        groupMax,
        ImGui::GetColorU32(ImVec4(0.4f, 0.45f, 0.5f, 0.8f)),
        4.0f,
        0,
        1.3f
    );
}

void UMainToolbarWidget::RenderSceneButtons()
{
    const ImVec2 IconSizeVec(IconSize, IconSize);

    // 버튼 스타일 설정 (SViewportWindow 스타일)
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 4));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(9, 0));
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.2f, 0.2f, 0.2f, 0.5f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.3f, 0.3f, 0.3f, 0.7f));

    BeginButtonGroup();

    // New 버튼
    if (IconNew && IconNew->GetShaderResourceView())
    {
        if (ImGui::ImageButton("##NewBtn", (void*)IconNew->GetShaderResourceView(), IconSizeVec))
        {
            PendingCommand = EToolbarCommand::NewScene;
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("새 씬을 생성합니다 [Ctrl+N]");
    }

    ImGui::SameLine();

    // Save 버튼
    if (IconSave && IconSave->GetShaderResourceView())
    {
        if (ImGui::ImageButton("##SaveBtn", (void*)IconSave->GetShaderResourceView(), IconSizeVec))
        {
            PendingCommand = EToolbarCommand::SaveScene;
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("현재 씬을 저장합니다 [Ctrl+S]");
    }

    ImGui::SameLine();

    // Load 버튼
    if (IconLoad && IconLoad->GetShaderResourceView())
    {
        if (ImGui::ImageButton("##LoadBtn", (void*)IconLoad->GetShaderResourceView(), IconSizeVec))
        {
            PendingCommand = EToolbarCommand::LoadScene;
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("씬을 불러옵니다 [Ctrl+O]");
    }

    EndButtonGroup();

    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar(3);
}

void UMainToolbarWidget::RenderActorSpawnButton()
{
    const ImVec2 IconSizeVec(IconSize, IconSize);

    // 드롭다운 버튼 스타일
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 4));
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.2f, 0.2f, 0.2f, 0.5f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.3f, 0.3f, 0.3f, 0.7f));

    ImGui::BeginGroup();

    // 아이콘과 화살표를 포함하는 버튼
    bool bButtonClicked = false;
    if (IconAddActor && IconAddActor->GetShaderResourceView())
    {
        // 커서 위치 저장
        ImVec2 buttonStartPos = ImGui::GetCursorScreenPos();

        // 아이콘+텍스트를 포함하는 투명 버튼 (전체 영역)
        const float ButtonWidth = IconSize + 20.0f;  // 아이콘 + 화살표 공간
        const float ButtonHeight = IconSize + 9.0f;

        if (ImGui::Button("##AddActorBtnInvisible", ImVec2(ButtonWidth, ButtonHeight)))
        {
            bButtonClicked = true;
        }
        bool bIsHovered = ImGui::IsItemHovered();

        // 버튼 위에 아이콘과 텍스트를 오버레이
        ImDrawList* drawList = ImGui::GetWindowDrawList();

        // 아이콘 그리기
        ImVec2 iconPos = buttonStartPos;
        iconPos.x += 4.0f;
        iconPos.y += 4.0f;
        drawList->AddImage(
            (void*)IconAddActor->GetShaderResourceView(),
            iconPos,
            ImVec2(iconPos.x + IconSize, iconPos.y + IconSize)
        );

        // 화살표 그리기
        ImVec2 arrowPos = buttonStartPos;
        arrowPos.x += IconSize + 5.0f;
        arrowPos.y += (ButtonHeight - 20.0f) / 2.0f;

        ImVec4 textColor = bIsHovered ? ImVec4(1.0f, 1.0f, 1.0f, 1.0f) : ImVec4(0.7f, 0.7f, 0.7f, 1.0f);
        drawList->AddText(arrowPos, ImGui::GetColorU32(textColor), "∨");

        // 툴팁
        if (bIsHovered)
            ImGui::SetTooltip("액터를 월드에 추가합니다");
    }

    // Actor Spawn 버튼만 테두리를 아래로 5픽셀 추가 확장
    ImVec2 groupMin = ImGui::GetItemRectMin();
    ImVec2 groupMax = ImGui::GetItemRectMax();

   // const float Padding = 1.0f;
   // groupMin.x -= Padding;
   // groupMin.y -= Padding;
   // groupMax.x += Padding;
   // groupMax.y += Padding + 7.0f;  // 아래쪽으로 5픽셀 추가

    ImDrawList* drawList = ImGui::GetWindowDrawList();

    drawList->AddRect(
        groupMin,
        groupMax,
        ImGui::GetColorU32(ImVec4(0.4f, 0.45f, 0.5f, 0.8f)),
        4.0f,
        0,
        1.3f
    );

    ImGui::EndGroup();

    // 팝업 열기
    if (bButtonClicked)
    {
        ImGui::OpenPopup("ActorSpawnPopup");
    }

    // Actor Spawn 팝업 렌더링
    if (ImGui::BeginPopup("ActorSpawnPopup"))
    {
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "액터 추가");
        ImGui::Separator();

        TArray<UClass*> SpawnableActors = UClass::GetAllSpawnableActors();
        for (UClass* ActorClass : SpawnableActors)
        {
            if (ActorClass && ActorClass->bIsSpawnable && ActorClass->DisplayName)
            {
                ImGui::PushID(ActorClass->DisplayName);
                if (ImGui::Selectable(ActorClass->DisplayName))
                {
                    // Actor 생성 명령 큐에 추가
                    PendingCommand = EToolbarCommand::SpawnActor;
                    PendingActorClass = ActorClass;
                    ImGui::CloseCurrentPopup();
                }
                if (ActorClass->Description && ImGui::IsItemHovered())
                {
                    ImGui::SetTooltip("%s", ActorClass->Description);
                }
                ImGui::PopID();
            }
        }
        ImGui::EndPopup();
    }

    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar(2);
}

void UMainToolbarWidget::RenderPIEButtons()
{
    const ImVec2 IconSizeVec(IconSize, IconSize);

    // 버튼 스타일
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 4));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.2f, 0.2f, 0.2f, 0.5f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.3f, 0.3f, 0.3f, 0.7f));

    BeginButtonGroup();

#ifdef _EDITOR
    extern UEditorEngine GEngine;
    bool isPIE = GEngine.IsPIEActive();

    // Play 버튼
    ImGui::BeginDisabled(isPIE);
    if (IconPlay && IconPlay->GetShaderResourceView())
    {
        ImVec4 tint = isPIE ? ImVec4(0.5f, 0.5f, 0.5f, 0.5f) : ImVec4(1, 1, 1, 1);
        if (ImGui::ImageButton("##PlayBtn", (void*)IconPlay->GetShaderResourceView(),
                                IconSizeVec, ImVec2(0,0), ImVec2(1,1), ImVec4(0,0,0,0), tint))
        {
            PendingCommand = EToolbarCommand::StartPIE;
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Play In Editor를 시작합니다 [F5]");
    }
    ImGui::EndDisabled();

    ImGui::SameLine();

    // Stop 버튼
    ImGui::BeginDisabled(!isPIE);
    if (IconStop && IconStop->GetShaderResourceView())
    {
        ImVec4 tint = !isPIE ? ImVec4(0.5f, 0.5f, 0.5f, 0.5f) : ImVec4(1, 1, 1, 1);
        if (ImGui::ImageButton("##StopBtn", (void*)IconStop->GetShaderResourceView(),
                                IconSizeVec, ImVec2(0,0), ImVec2(1,1), ImVec4(0,0,0,0), tint))
        {
            PendingCommand = EToolbarCommand::EndPIE;
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Play In Editor를 종료합니다 [Shift+F5]");
    }
    ImGui::EndDisabled();
#endif

    EndButtonGroup();

    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar(3);
}

void UMainToolbarWidget::OnNewScene()
{
    try
    {
        UWorld* CurrentWorld = UUIManager::GetInstance().GetWorld();
        if (!CurrentWorld)
        {
            UE_LOG("MainToolbar: Cannot find World!");
            return;
        }

        // 로드 직전: Transform 위젯/선택 초기화
        UUIManager::GetInstance().ClearTransformWidgetSelection();
        GWorld->GetSelectionManager()->ClearSelection();

        // 새 레벨 생성 후 월드에 적용
        CurrentWorld->GetLightManager()->ClearAllLightList();
        CurrentWorld->SetLevel(ULevelService::CreateNewLevel());

        UE_LOG("MainToolbar: New scene created");
    }
    catch (const std::exception& Exception)
    {
        UE_LOG("MainToolbar: Create Error: %s", Exception.what());
    }
}

void UMainToolbarWidget::OnSaveScene()
{
    // Windows 파일 다이얼로그 열기
    std::filesystem::path selectedPath = OpenSaveFileDialog();
    if (selectedPath.empty())
        return;

    try
    {
        UWorld* CurrentWorld = UUIManager::GetInstance().GetWorld();
        if (!CurrentWorld)
        {
            UE_LOG("MainToolbar: Cannot find World!");
            return;
        }

        // 파일 경로에서 씬 이름 추출
        FString SceneName = selectedPath.stem().string(); // 확장자 제외한 파일명

        // Scene 디렉토리가 없으면 생성
        namespace fs = std::filesystem;
        fs::path SceneDir = "Scene";
        if (!fs::exists(SceneDir))
        {
            fs::create_directories(SceneDir);
            UE_LOG("MainToolbar: Created Scene directory");
        }

        FString FilePath = "Scene/" + SceneName + ".Scene";

        JSON LevelJson;
        CurrentWorld->GetLevel()->Serialize(false, LevelJson);
        bool bSuccess = FJsonSerializer::SaveJsonToFile(LevelJson, FilePath);

        UE_LOG("MainToolbar: Scene saved: %s", SceneName.c_str());
    }
    catch (const std::exception& Exception)
    {
        UE_LOG("MainToolbar: Save Error: %s", Exception.what());
    }
}

void UMainToolbarWidget::OnLoadScene()
{
    // Windows 파일 다이얼로그 열기
    std::filesystem::path selectedPath = OpenLoadFileDialog();
    if (selectedPath.empty())
        return;

    try
    {
        FString InFilePath = selectedPath.string();

        // 파일명에서 씬 이름 추출
        FString SceneName = InFilePath;
        size_t LastSlash = SceneName.find_last_of("\\/");
        if (LastSlash != std::string::npos)
        {
            SceneName = SceneName.substr(LastSlash + 1);
        }
        size_t LastDot = SceneName.find_last_of(".");
        if (LastDot != std::string::npos)
        {
            SceneName = SceneName.substr(0, LastDot);
        }

        // World 가져오기
        UWorld* CurrentWorld = UUIManager::GetInstance().GetWorld();
        if (!CurrentWorld)
        {
            UE_LOG("MainToolbar: Cannot find World!");
            return;
        }

        // 로드 직전: Transform 위젯/선택 초기화
        UUIManager::GetInstance().ClearTransformWidgetSelection();
        GWorld->GetSelectionManager()->ClearSelection();

        std::unique_ptr<ULevel> NewLevel = ULevelService::CreateDefaultLevel();
        JSON LevelJsonData;
        if (FJsonSerializer::LoadJsonFromFile(LevelJsonData, InFilePath))
        {
            NewLevel->Serialize(true, LevelJsonData);
        }
        else
        {
            UE_LOG("MainToolbar: Failed To Load Level From: %s", InFilePath.c_str());
            return;
        }
        CurrentWorld->SetLevel(std::move(NewLevel));

        UE_LOG("MainToolbar: Scene loaded successfully: %s", InFilePath.c_str());
    }
    catch (const std::exception& Exception)
    {
        UE_LOG("MainToolbar: Load Error: %s", Exception.what());
    }
}

std::filesystem::path UMainToolbarWidget::OpenSaveFileDialog()
{
    using std::filesystem::path;

    OPENFILENAMEW ofn;
    wchar_t szFile[260] = {};
    wchar_t szInitialDir[260] = {};

    // Scene 폴더를 기본 경로로 설정
    std::filesystem::path sceneDir = std::filesystem::current_path() / "Scene";
    wcscpy_s(szInitialDir, sceneDir.wstring().c_str());

    // Initialize OPENFILENAME
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = GetActiveWindow();
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile) / sizeof(wchar_t);
    ofn.lpstrFilter = L"Scene Files\0*.scene\0All Files\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = nullptr;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = szInitialDir;
    ofn.lpstrTitle = L"Save Scene File";
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT | OFN_EXPLORER | OFN_HIDEREADONLY | OFN_NOCHANGEDIR;
    ofn.lpstrDefExt = L"scene";

    UE_LOG("MainToolbar: Opening Save Dialog (Modal)...");

    if (GetSaveFileNameW(&ofn) == TRUE)
    {
        UE_LOG("MainToolbar: Save Dialog Closed");
        return path(szFile);
    }

    UE_LOG("MainToolbar: Save Dialog Cancelled");
    return L"";
}

std::filesystem::path UMainToolbarWidget::OpenLoadFileDialog()
{
    using std::filesystem::path;

    OPENFILENAMEW ofn;
    wchar_t szFile[260] = {};
    wchar_t szInitialDir[260] = {};

    // Scene 폴더를 기본 경로로 설정
    std::filesystem::path sceneDir = std::filesystem::current_path() / "Scene";
    wcscpy_s(szInitialDir, sceneDir.wstring().c_str());

    // Initialize OPENFILENAME
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = GetActiveWindow();
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile) / sizeof(wchar_t);
    ofn.lpstrFilter = L"Scene Files\0*.scene\0All Files\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = nullptr;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = szInitialDir;
    ofn.lpstrTitle = L"Load Scene File";
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_EXPLORER | OFN_HIDEREADONLY | OFN_NOCHANGEDIR;

    UE_LOG("MainToolbar: Opening Load Dialog (Modal)...");

    if (GetOpenFileNameW(&ofn) == TRUE)
    {
        UE_LOG("MainToolbar: Load Dialog Closed");
        return path(szFile);
    }

    UE_LOG("MainToolbar: Load Dialog Cancelled");
    return L"";
}

void UMainToolbarWidget::ProcessPendingCommands()
{
    if (PendingCommand == EToolbarCommand::None)
        return;

    switch (PendingCommand)
    {
    case EToolbarCommand::NewScene:
        OnNewScene();
        break;

    case EToolbarCommand::SaveScene:
        OnSaveScene();
        break;

    case EToolbarCommand::LoadScene:
        OnLoadScene();
        break;

    case EToolbarCommand::SpawnActor:
        if (PendingActorClass && GWorld)
        {
            AActor* NewActor = GWorld->SpawnActor(PendingActorClass);
            if (NewActor)
            {
                UE_LOG("MainToolbar: Spawned actor %s", PendingActorClass->DisplayName);
            }
        }
        PendingActorClass = nullptr;
        break;

    case EToolbarCommand::StartPIE:
#ifdef _EDITOR
        {
            extern UEditorEngine GEngine;
            GEngine.StartPIE();
        }
#endif
        break;

    case EToolbarCommand::EndPIE:
#ifdef _EDITOR
        {
            extern UEditorEngine GEngine;
            GEngine.EndPIE();
        }
#endif
        break;

    default:
        break;
    }

    // 명령 처리 완료 후 초기화
    PendingCommand = EToolbarCommand::None;
}

void UMainToolbarWidget::HandleKeyboardShortcuts()
{
    ImGuiIO& io = ImGui::GetIO();

    // Ctrl+N: New Scene
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_N, false))
    {
        PendingCommand = EToolbarCommand::NewScene;
    }

    // Ctrl+S: Save Scene
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S, false))
    {
        PendingCommand = EToolbarCommand::SaveScene;
    }

    // Ctrl+O: Open/Load Scene
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_O, false))
    {
        PendingCommand = EToolbarCommand::LoadScene;
    }

#ifdef _EDITOR
    extern UEditorEngine GEngine;

    // F5: Start PIE
    if (ImGui::IsKeyPressed(ImGuiKey_F5, false) && !GEngine.IsPIEActive())
    {
        PendingCommand = EToolbarCommand::StartPIE;
    }

    // Shift+F5: Stop PIE
    if (io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_F5, false) && GEngine.IsPIEActive())
    {
        PendingCommand = EToolbarCommand::EndPIE;
    }
#endif
}
