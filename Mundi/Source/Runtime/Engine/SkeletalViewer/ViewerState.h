#pragma once
#include <string>

class UWorld; class FViewport; class FViewportClient;

class ViewerState
{
public:
    std::string Name;
    UWorld* World = nullptr;
    FViewport* Viewport = nullptr;
    FViewportClient* Client = nullptr;
};
