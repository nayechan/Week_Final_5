#pragma once
#include "SViewerWindow.h"

class UPhysicalMaterial;
class UTexture;

struct PhysicalMaterialEditorState : public ViewerState
{
    UPhysicalMaterial* EditingAsset = nullptr;
    FString CurrentFilePath;
    bool bIsDirty = false;

    virtual ~PhysicalMaterialEditorState() {}
};

class SPhysicalMaterialEditorWindow : public SViewerWindow
{
public:
    SPhysicalMaterialEditorWindow();
    virtual ~SPhysicalMaterialEditorWindow();

    virtual void OnRender() override;
    virtual void OnSave() override;

    void OpenOrFocusAsset(const FString& FilePath);

protected:
    virtual ViewerState* CreateViewerState(const char* Name, UEditorAssetPreviewContext* Context) override;
    virtual void DestroyViewerState(ViewerState*& State) override;
    virtual FString GetWindowTitle() const override { return "Physical Material Editor"; }
    virtual void RenderTabsAndToolbar(EViewerType CurrentViewerType) override;

    void RenderToolbar();
    void RenderDetailsPanel();

    void CreateNewAsset();
    void OpenAsset();
    void SaveAsset();
    void SaveAssetAs();

    void PreloadEditorIcons();

    PhysicalMaterialEditorState* GetActiveState() const 
    { 
        return static_cast<PhysicalMaterialEditorState*>(ActiveState); 
    }

private:
    UTexture* IconNew = nullptr;
    UTexture* IconOpen = nullptr;
    UTexture* IconSave = nullptr;
    UTexture* IconSaveAs = nullptr;
};