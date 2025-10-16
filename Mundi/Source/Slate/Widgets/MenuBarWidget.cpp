#include "pch.h"
#include "MenuBarWidget.h"
#include "USlateManager.h"
#include "ImGui/imgui.h"
#include <EditorEngine.h>

// 필요하다면 외부 free 함수 사용 가능 (동일 TU가 아닐 경우 extern 선언이 필요)
// extern void LoadSplitterConfig(SSplitter* RootSplitter);

IMPLEMENT_CLASS(UMenuBarWidget)

UMenuBarWidget::UMenuBarWidget() {}
UMenuBarWidget::UMenuBarWidget(USlateManager* InOwner) : Owner(InOwner) {}

void UMenuBarWidget::Initialize() {}
void UMenuBarWidget::Update() {}

void UMenuBarWidget::RenderWidget()
{
    if (!ImGui::BeginMainMenuBar())
        return;

    // ===== File =====
    if (ImGui::BeginMenu("File"))
    {
        if (ImGui::MenuItem("New Scene", "Ctrl+N"))
            (FileAction ? FileAction : [this](auto a) { OnFileMenuAction(a); })("new_scene");

        if (ImGui::MenuItem("Open Scene", "Ctrl+O"))
            (FileAction ? FileAction : [this](auto a) { OnFileMenuAction(a); })("open_scene");

        ImGui::Separator();

        if (ImGui::MenuItem("Save Scene", "Ctrl+S"))
            (FileAction ? FileAction : [this](auto a) { OnFileMenuAction(a); })("save_scene");

        if (ImGui::MenuItem("Save Scene As...", "Ctrl+Shift+S"))
            (FileAction ? FileAction : [this](auto a) { OnFileMenuAction(a); })("save_scene_as");

        ImGui::Separator();

        if (ImGui::BeginMenu("Recent Scenes"))
        {
            ImGui::MenuItem("Scene1.scene");
            ImGui::MenuItem("Scene2.scene");
            ImGui::MenuItem("Scene3.scene");
            ImGui::EndMenu();
        }

        ImGui::Separator();

        if (ImGui::MenuItem("Exit", "Alt+F4"))
            (FileAction ? FileAction : [this](auto a) { OnFileMenuAction(a); })("exit");

        ImGui::EndMenu();
    }

    // ===== Edit =====
    if (ImGui::BeginMenu("Edit"))
    {
        if (ImGui::MenuItem("Undo", "Ctrl+Z"))
            (EditAction ? EditAction : [this](auto a) { OnEditMenuAction(a); })("undo");
        if (ImGui::MenuItem("Redo", "Ctrl+Y"))
            (EditAction ? EditAction : [this](auto a) { OnEditMenuAction(a); })("redo");

        ImGui::Separator();

        if (ImGui::MenuItem("Cut", "Ctrl+X"))
            (EditAction ? EditAction : [this](auto a) { OnEditMenuAction(a); })("cut");
        if (ImGui::MenuItem("Copy", "Ctrl+C"))
            (EditAction ? EditAction : [this](auto a) { OnEditMenuAction(a); })("copy");
        if (ImGui::MenuItem("Paste", "Ctrl+V"))
            (EditAction ? EditAction : [this](auto a) { OnEditMenuAction(a); })("paste");
        if (ImGui::MenuItem("Duplicate", "Ctrl+D"))
            (EditAction ? EditAction : [this](auto a) { OnEditMenuAction(a); })("duplicate");
        if (ImGui::MenuItem("Delete", "Delete"))
            (EditAction ? EditAction : [this](auto a) { OnEditMenuAction(a); })("delete");

        ImGui::Separator();

        if (ImGui::MenuItem("Select All", "Ctrl+A"))
            (EditAction ? EditAction : [this](auto a) { OnEditMenuAction(a); })("select_all");

        ImGui::Separator();

        if (ImGui::MenuItem("Project Settings"))
            (EditAction ? EditAction : [this](auto a) { OnEditMenuAction(a); })("project_settings");

        ImGui::EndMenu();
    }

    // ===== Window =====
    if (ImGui::BeginMenu("Window"))
    {
        if (ImGui::BeginMenu("Viewport Layout"))
        {
            if (ImGui::MenuItem("Single Viewport"))
                (WindowAction ? WindowAction : [this](auto a) { OnWindowMenuAction(a); })("single_viewport");

            if (ImGui::MenuItem("Four Split"))
                (WindowAction ? WindowAction : [this](auto a) { OnWindowMenuAction(a); })("four_split");

            ImGui::EndMenu();
        }

        ImGui::Separator();

        if (ImGui::MenuItem("Details Panel"))
            (WindowAction ? WindowAction : [this](auto a) { OnWindowMenuAction(a); })("details_panel");

        if (ImGui::MenuItem("Scene Manager"))
            (WindowAction ? WindowAction : [this](auto a) { OnWindowMenuAction(a); })("scene_manager");

        if (ImGui::MenuItem("Console"))
            (WindowAction ? WindowAction : [this](auto a) { OnWindowMenuAction(a); })("console");

        if (ImGui::MenuItem("Control Panel"))
            (WindowAction ? WindowAction : [this](auto a) { OnWindowMenuAction(a); })("control_panel");

        ImGui::Separator();

        if (ImGui::MenuItem("Reset Layout"))
            (WindowAction ? WindowAction : [this](auto a) { OnWindowMenuAction(a); })("reset_layout");

        ImGui::EndMenu();
    }

    // ===== Help =====
    if (ImGui::BeginMenu("Help"))
    {
        if (ImGui::MenuItem("Documentation"))
            (HelpAction ? HelpAction : [this](auto a) { OnHelpMenuAction(a); })("documentation");

        if (ImGui::MenuItem("Tutorials"))
            (HelpAction ? HelpAction : [this](auto a) { OnHelpMenuAction(a); })("tutorials");

        ImGui::Separator();

        if (ImGui::MenuItem("Keyboard Shortcuts"))
            (HelpAction ? HelpAction : [this](auto a) { OnHelpMenuAction(a); })("shortcuts");

        ImGui::Separator();

        if (ImGui::MenuItem("About"))
            (HelpAction ? HelpAction : [this](auto a) { OnHelpMenuAction(a); })("about");

        ImGui::EndMenu();
    }

    // ===== PIE Controls (centered) =====
    {
        ImGuiStyle& style = ImGui::GetStyle();
        const char* kStart = "Start PIE";
        const char* kEnd   = "End PIE";
        float startW = ImGui::CalcTextSize(kStart).x + style.FramePadding.x * 2.0f;
        float endW   = ImGui::CalcTextSize(kEnd).x   + style.FramePadding.x * 2.0f;
        float spacing = style.ItemSpacing.x;
        float totalW  = startW + spacing + endW;

        float regionMin = ImGui::GetWindowContentRegionMin().x;
        float regionMax = ImGui::GetWindowContentRegionMax().x;
        float posX = regionMin + (regionMax - regionMin - totalW) * 0.5f;
        // Avoid overlapping previously added menus
        float curX = ImGui::GetCursorPosX();
        if (posX < curX) posX = curX;
        ImGui::SameLine(posX);

#ifdef _EDITOR
        extern UEditorEngine GEngine;
        bool isPIE = GEngine.IsPIEActive();
        ImGui::BeginDisabled(isPIE);
        if (ImGui::Button(kStart, ImVec2(startW, 0.0f)))
        {
            GEngine.StartPIE();
            isPIE = true;
        }
        ImGui::EndDisabled();
        ImGui::SameLine(0.0f, spacing);
        ImGui::BeginDisabled(!isPIE);
        if (ImGui::Button(kEnd, ImVec2(endW, 0.0f)))
        {
            GEngine.EndPIE();
            isPIE = false;
        }
        ImGui::EndDisabled();
#else
        ImGui::Text("PIE unavailable");
#endif
    }

    ImGui::EndMainMenuBar();
}

// ---------------- 기본 동작 ----------------

void UMenuBarWidget::OnFileMenuAction(const char* action)
{
    UE_LOG("File menu action: %s", action);

    if (strcmp(action, "new_scene") == 0)
    {
        // TODO: 새 씬 생성
    }
    else if (strcmp(action, "open_scene") == 0)
    {
        // TODO: 씬 열기
    }
    else if (strcmp(action, "save_scene") == 0)
    {
        // TODO: 저장
    }
    else if (strcmp(action, "save_scene_as") == 0)
    {
        // TODO: 다른 이름으로 저장
    }
    else if (strcmp(action, "exit") == 0)
    {
        PostQuitMessage(0);
    }
}

void UMenuBarWidget::OnEditMenuAction(const char* action)
{
    UE_LOG("Edit menu action: %s", action);
    // TODO: Undo/Redo/Cut/Copy/Paste 등 구현 혹은 외부 콜백으로 처리
}

void UMenuBarWidget::OnWindowMenuAction(const char* action)
{
    UE_LOG("Window menu action: %s", action);

    if (!Owner) return;

    if (strcmp(action, "single_viewport") == 0)
    {
        Owner->SwitchLayout(EViewportLayoutMode::SingleMain);
    }
    else if (strcmp(action, "four_split") == 0)
    {
        Owner->SwitchLayout(EViewportLayoutMode::FourSplit);
    }
    else if (strcmp(action, "reset_layout") == 0)
    {
        // 추천: SMultiViewportWindow에 ResetLayout() 같은 래퍼 함수 하나 만들어 호출
        // Owner->ResetLayout();

        // 임시: 이미 공개 범위라면 외부 free 함수를 사용해도 됨
        // LoadSplitterConfig(Owner->GetRootSplitter());  // GetRootSplitter() 제공 필요
    }
    else
    {
        // "details_panel", "scene_manager", "console", "control_panel" 토글은
        // Owner 쪽에 Show/Hide API를 추가하여 호출하는 걸 권장
        // 예: Owner->ToggleDetailsPanel();
    }
}

void UMenuBarWidget::OnHelpMenuAction(const char* action)
{
    UE_LOG("Help menu action: %s", action);
    // TODO: 문서/튜토리얼/단축키/어바웃 처리
}
