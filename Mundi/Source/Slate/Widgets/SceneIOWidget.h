#pragma once
#include "Widget.h"
#include "Vector.h"
#include "World.h"
#include <filesystem>

using std::filesystem::path;

/**
 * @brief Scene Save/Load Widget for managing scene files
 * Provides UI for saving, loading, and creating new scenes
 */
class USceneIOWidget : public UWidget
{
public:
	DECLARE_CLASS(USceneIOWidget, UWidget)

	void Initialize() override;
	void Update() override;
	void RenderWidget() override;

	// Special Member Function
	USceneIOWidget();
	~USceneIOWidget() override;

private:
	// UI Rendering Methods
	void RenderSaveLoadSection();
	void RenderStatusMessage();
	
	// Core Functionality
	void SaveLevel(const FString& InFilePath);
	void LoadLevel(const FString& InFilePath);
	void CreateNewLevel();
	
	// File Dialog Helpers
	static path OpenSaveFileDialog();
	static path OpenLoadFileDialog();
	
	// Status and UI State
	void SetStatusMessage(const FString& Message, bool bIsError = false);
	void UpdateStatusMessage(float DeltaTime);

	// Member Variables
	char NewLevelNameBuffer[256];
	FString StatusMessage;
	float StatusMessageTimer;
	bool bIsStatusError;
	
	static constexpr float STATUS_MESSAGE_DURATION = 3.0f;
};
