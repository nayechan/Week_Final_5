#pragma once
#include "UIWindow.h"

class UPropertyWindow : public UUIWindow
{
public:
	DECLARE_CLASS(UPropertyWindow, UUIWindow)

	UPropertyWindow();
	virtual void Initialize() override;
};