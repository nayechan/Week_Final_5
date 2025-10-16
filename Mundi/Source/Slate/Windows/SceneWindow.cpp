#include "pch.h"
#include "SceneWindow.h"
#include "Widgets/SceneManagerWidget.h"
#include "Widgets/ShowFlagWidget.h"
#include "Widgets/ActorSpawnWidget.h"

//// UE_LOG 대체 매크로
//#define UE_LOG(fmt, ...)

IMPLEMENT_CLASS(USceneWindow)

USceneWindow::USceneWindow()
{
	FUIWindowConfig Config;
	Config.WindowTitle = "Scene Manager";
	Config.DefaultSize = ImVec2(350, 500);
	Config.DefaultPosition = ImVec2(1225, 10);
	Config.MinSize = ImVec2(300, 400);
	Config.bResizable = true;
	Config.bMovable = true;
	Config.bCollapsible = true;
	Config.DockDirection = EUIDockDirection::Center;

	Config.UpdateWindowFlags();
	SetConfig(Config);

	UActorSpawnWidget* ActorSpawnWidget = NewObject<UActorSpawnWidget>();
	AddWidget(ActorSpawnWidget);

	// Add Scene Manager Widget (main scene hierarchy)
	USceneManagerWidget* SceneManagerWidget = NewObject<USceneManagerWidget>();
	if (SceneManagerWidget)
	{
		AddWidget(SceneManagerWidget);
		UE_LOG("SceneWindow: SceneManagerWidget created successfully");
	}
	else
	{
		UE_LOG("SceneWindow: Failed to create SceneManagerWidget");
	}
	
	// Add Show Flag Widget for rendering control
	UShowFlagWidget* ShowFlagWidget = NewObject<UShowFlagWidget>();
	if (ShowFlagWidget)
	{
		AddWidget(ShowFlagWidget);
		UE_LOG("SceneWindow: ShowFlagWidget created successfully");
	}
	else
	{
		UE_LOG("SceneWindow: Failed to create ShowFlagWidget");
	}
	
	// Transform and termination widgets moved to Control Panel for better UX
}

void USceneWindow::Initialize()
{
	UE_LOG("SceneWindow: Successfully Initialized");
}
