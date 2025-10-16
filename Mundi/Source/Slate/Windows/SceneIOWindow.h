#pragma once
#include "SWindow.h"
#include "ImGui/imgui.h"
class UConsoleWindow;
class SSceneIOWindow : public SWindow
{
public:
    SSceneIOWindow();
    virtual ~SSceneIOWindow();

    void Initialize();
    virtual void OnRender() override;
    virtual void OnUpdate(float deltaSecond) override;

private:


    UConsoleWindow* ConsoleWindow;
};