#include "pch.h"
#include "SDetailsWindow.h"
#include "Windows/PropertyWindow.h"
#include "UIManager.h"
SDetailsWindow::SDetailsWindow()
    : SWindow()
    , DetailsWidget(nullptr)
{
    Initialize();
}

SDetailsWindow::~SDetailsWindow()
{
    delete DetailsWidget;
}

void SDetailsWindow::Initialize()
{
    DetailsWidget = new UPropertyWindow();
    DetailsWidget->Initialize();
}

void SDetailsWindow::OnRender()
{
    // === 패널 위치/크기 고정 (SceneIOWindow와 동일) ===
    ImGui::SetNextWindowPos(ImVec2(Rect.Min.X, Rect.Min.Y));
    ImGui::SetNextWindowSize(ImVec2(Rect.GetWidth(), Rect.GetHeight()));

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse
        | ImGuiWindowFlags_NoResize
        | ImGuiWindowFlags_NoMove;

    if (ImGui::Begin("디테일", nullptr, flags))
    {
        if (DetailsWidget)
            DetailsWidget->RenderWidget();
    }
    ImGui::End();
}

void SDetailsWindow::OnUpdate(float deltaSecond)
{
    DetailsWidget->Update();
}