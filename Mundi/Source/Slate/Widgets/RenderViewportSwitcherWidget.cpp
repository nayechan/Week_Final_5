#include "pch.h"
#include "Widgets/RenderViewportSwitcherWidget.h"
#include "USlateManager.h"
#include "World.h"
#include "ImGui/imgui.h"

IMPLEMENT_CLASS(URenderViewportSwitcherWidget)

URenderViewportSwitcherWidget::URenderViewportSwitcherWidget()
{
}

URenderViewportSwitcherWidget::~URenderViewportSwitcherWidget()
{
}

void URenderViewportSwitcherWidget::Initialize()
{
}

void URenderViewportSwitcherWidget::Update()
{
}

void URenderViewportSwitcherWidget::RenderWidget()
{



    ImGui::Text("Viewport Mode:");
    ImGui::Separator();

    // === Single View 버튼 ===
    if (bUseMainViewport)
    {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.2f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.7f, 0.3f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.1f, 0.5f, 0.1f, 1.0f));
    }
    else
    {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.2f, 0.2f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.4f, 0.4f, 0.4f, 1.0f));
    }

    if (ImGui::Button("Single View", ImVec2(120, 35)))
    {
        SLATE.SwitchLayout(EViewportLayoutMode::SingleMain);
        bUseMainViewport = true;
        UE_LOG("UIManager: Switched to Main Viewport mode");
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Switch to single perspective viewport\nShortcut: F1");

    ImGui::PopStyleColor(3);

    ImGui::SameLine();

    // === Multi View 버튼 ===
    if (!bUseMainViewport)
    {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.2f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.7f, 0.3f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.1f, 0.5f, 0.1f, 1.0f));
    }
    else
    {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.2f, 0.2f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.4f, 0.4f, 0.4f, 1.0f));
    }

    if (ImGui::Button("Multi View", ImVec2(120, 35)))
    {
        SLATE.SwitchLayout(EViewportLayoutMode::FourSplit);
        bUseMainViewport = false;
        UE_LOG("UIManager: Switched to Multi Viewport mode");
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Switch to 4-panel viewport (Perspective, Front, Left, Top)\nShortcut: F2");

    ImGui::PopStyleColor(3);

}