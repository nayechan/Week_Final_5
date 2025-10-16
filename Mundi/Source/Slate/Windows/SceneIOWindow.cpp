#include "pch.h"
#include "SceneIOWindow.h"
#include "Windows/ConsoleWindow.h"
SSceneIOWindow::SSceneIOWindow()
{
	ConsoleWindow = new UConsoleWindow;
    UGlobalConsole::SetConsoleWidget(ConsoleWindow->GetConsoleWidget());
}

SSceneIOWindow::~SSceneIOWindow()
{
    delete ConsoleWindow;
   
}
void SSceneIOWindow::Initialize()
{
}
void SSceneIOWindow::OnRender()
{
    ImGui::SetNextWindowPos(ImVec2(Rect.Min.X, Rect.Min.Y));
    ImGui::SetNextWindowSize(ImVec2(Rect.GetWidth(), Rect.GetHeight()));

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse
        | ImGuiWindowFlags_NoResize
        | ImGuiWindowFlags_NoMove;

    if (ImGui::Begin("Console", nullptr, flags))
    {
        if (ConsoleWindow) {
            ConsoleWindow->RenderWidget();
        }
    }
    ImGui::End();
}

void SSceneIOWindow::OnUpdate(float deltaSecond)
{
    if (ConsoleWindow) {
        ConsoleWindow->Update();
    }
}
