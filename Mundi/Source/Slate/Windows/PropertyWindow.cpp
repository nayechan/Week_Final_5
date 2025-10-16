#include "pch.h"
#include "PropertyWindow.h"
#include "Widgets/TargetActorTransformWidget.h"

IMPLEMENT_CLASS(UPropertyWindow)

UPropertyWindow::UPropertyWindow()
{
	FUIWindowConfig Config;
	Config.WindowTitle = "Property Window";
	Config.DefaultSize = ImVec2(400, 620);
	Config.DefaultPosition = ImVec2(10, 10);
	Config.MinSize = ImVec2(400, 200);
	Config.DockDirection = EUIDockDirection::Top;
	Config.Priority = 15;
	Config.bResizable = true;
	Config.bMovable = true;
	Config.bCollapsible = true;

	auto TargetActorTransformWidget = NewObject<UTargetActorTransformWidget>();
	TargetActorTransformWidget->Initialize();
	AddWidget(TargetActorTransformWidget);

	Config.UpdateWindowFlags();
	SetConfig(Config);
}

void UPropertyWindow::Initialize()
{
	UE_LOG("UPropertyWindow: Initialized");
}
