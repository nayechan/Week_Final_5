#include "pch.h"
#include "ControlPanelWindow.h"
#include "Widgets/CameraControlWidget.h"
#include "Widgets/FPSWidget.h"
#include "Widgets/ActorTerminationWidget.h"
#include "Widgets/PrimitiveSpawnWidget.h"
#include "Widgets/SceneIOWidget.h"
#include "Widgets/RenderViewportSwitcherWidget.h"

//// UE_LOG 대체 매크로
//#define UE_LOG(fmt, ...)

IMPLEMENT_CLASS(UControlPanelWindow)

/**
 * @brief Control Panel Constructor
 * 적절한 사이즈의 윈도우 제공
 */
UControlPanelWindow::UControlPanelWindow()
{
	FUIWindowConfig Config;
	Config.WindowTitle = "Control Panel";
	Config.DefaultSize = ImVec2(400, 620);
	Config.DefaultPosition = ImVec2(10, 10);
	Config.MinSize = ImVec2(400, 200);
	Config.DockDirection = EUIDockDirection::Left;
	Config.Priority = 15;
	Config.bResizable = true;
	Config.bMovable = true;
	Config.bCollapsible = true;

	Config.UpdateWindowFlags();
	SetConfig(Config);

	UFPSWidget* FPSWidget = NewObject<UFPSWidget>();
	FPSWidget->Initialize();
	AddWidget(FPSWidget);

	UActorTerminationWidget* ActorTerminationWidget = NewObject<UActorTerminationWidget>();
	ActorTerminationWidget->Initialize();
	AddWidget(ActorTerminationWidget);

	UPrimitiveSpawnWidget* PrimitiveSpawnWidget = NewObject<UPrimitiveSpawnWidget>();
	PrimitiveSpawnWidget->Initialize();
	AddWidget(PrimitiveSpawnWidget);

	USceneIOWidget* SceneIOWidget = NewObject<USceneIOWidget>();
	SceneIOWidget->Initialize();
	AddWidget(SceneIOWidget);

	UCameraControlWidget* CameraControlWidget = NewObject<UCameraControlWidget>();
	CameraControlWidget->Initialize();
	AddWidget(CameraControlWidget);

	URenderViewportSwitcherWidget* RenderViewPortSWitcherWidget = NewObject<URenderViewportSwitcherWidget>();
	RenderViewPortSWitcherWidget->Initialize();
	AddWidget(RenderViewPortSWitcherWidget);
}

/**
 * @brief 초기화 함수
 */
void UControlPanelWindow::Initialize()
{
	UE_LOG("ControlPanelWindow: Initialized");
}
