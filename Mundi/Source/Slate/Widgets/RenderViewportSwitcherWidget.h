#pragma once
#include "Widgets/Widget.h"

class URenderViewportSwitcherWidget :
    public UWidget
{
public:
	DECLARE_CLASS(URenderViewportSwitcherWidget, UWidget)

	void Initialize() override;
	void Update() override;
	void RenderWidget() override;

	// Special Member Function
	URenderViewportSwitcherWidget();
	~URenderViewportSwitcherWidget() override;
private:
	bool bUseMainViewport = true;

};

