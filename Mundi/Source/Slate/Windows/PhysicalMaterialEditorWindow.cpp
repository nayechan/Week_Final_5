#include "pch.h"
#include "PhysicalMaterialEditorWindow.h"
#include "PhysicalMaterial.h"
#include "ResourceManager.h"
#include "SlateManager.h"
#include "ImGui/imgui.h"
#include "Source/Editor/PlatformProcess.h"
#include "Source/Slate/Widgets/ModalDialog.h"
#include "Texture.h"
#include <filesystem>

#include "Source/Runtime/Engine/Viewer/EditorAssetPreviewContext.h"
#include "Widgets/PropertyRenderer.h"

SPhysicalMaterialEditorWindow::SPhysicalMaterialEditorWindow()
{
    WindowTitle = "Physical Material Editor";
    PreloadEditorIcons();
}

SPhysicalMaterialEditorWindow::~SPhysicalMaterialEditorWindow()
{
    for (ViewerState* State : Tabs)
    {
        DestroyViewerState(State);
    }
    Tabs.Empty();
}

void SPhysicalMaterialEditorWindow::PreloadEditorIcons()
{
    FString NewIconPath = GDataDir + "/Icon/Plus.png";
    FString OpenIconPath = GDataDir + "/Icon/show-in-explorer.png";
    FString SaveIconPath = GDataDir + "/Icon/Toolbar_Save.png";
    FString SaveAsIconPath = GDataDir + "/Icon/Toolbar_SaveAs.png";

    IconNew = UResourceManager::GetInstance().Load<UTexture>(NewIconPath);
    IconOpen = UResourceManager::GetInstance().Load<UTexture>(OpenIconPath);
    IconSave = UResourceManager::GetInstance().Load<UTexture>(SaveIconPath);
    IconSaveAs = UResourceManager::GetInstance().Load<UTexture>(SaveAsIconPath);
}

ViewerState* SPhysicalMaterialEditorWindow::CreateViewerState(const char* Name, UEditorAssetPreviewContext* Context)
{
    PhysicalMaterialEditorState* NewState = new PhysicalMaterialEditorState();
    NewState->Name = Name;
    
    if (Context && !Context->AssetPath.empty())
    {
        NewState->CurrentFilePath = Context->AssetPath;
        NewState->EditingAsset = UResourceManager::GetInstance().Load<UPhysicalMaterial>(Context->AssetPath);
    }
    else
    {
        NewState->EditingAsset = NewObject<UPhysicalMaterial>();
        NewState->CurrentFilePath = "";
        NewState->bIsDirty = true; 
    }

    return NewState;
}

void SPhysicalMaterialEditorWindow::DestroyViewerState(ViewerState*& State)
{
    if (State)
    {
        delete State;
        State = nullptr;
    }
}

void SPhysicalMaterialEditorWindow::OpenOrFocusAsset(const FString& FilePath)
{
    for (int i = 0; i < Tabs.Num(); ++i)
    {
        PhysicalMaterialEditorState* State = static_cast<PhysicalMaterialEditorState*>(Tabs[i]);
        if (State->CurrentFilePath == FilePath)
        {
            ActiveTabIndex = i;
            ActiveState = State;
            return;
        }
    }

    UEditorAssetPreviewContext Context;
    Context.AssetPath = FilePath;
    
    std::filesystem::path fsPath(UTF8ToWide(FilePath));
    FString FileName = WideToUTF8(fsPath.filename().wstring());

    ViewerState* NewState = CreateViewerState(FileName.c_str(), &Context);
    if (NewState)
    {
        Tabs.Add(NewState);
        ActiveTabIndex = Tabs.Num() - 1;
        ActiveState = NewState;
    }
}

void SPhysicalMaterialEditorWindow::OnRender()
{
    if (!bIsOpen)
    {
        USlateManager::GetInstance().RequestCloseDetachedWindow(this);
        return;
    }

    ImGui::SetNextWindowSize(ImVec2(500, 600), ImGuiCond_FirstUseEver);

    char UniqueTitle[256];
    sprintf_s(UniqueTitle, "%s###PhyMatEd_%p", WindowTitle.c_str(), this);

    if (ImGui::Begin(UniqueTitle, &bIsOpen, ImGuiWindowFlags_MenuBar))
    {
        RenderTabsAndToolbar(EViewerType::PhysicalMaterial);

        if (GetActiveState() && GetActiveState()->EditingAsset)
        {
            RenderDetailsPanel();
        }
        else
        {
            ImGui::TextDisabled("No asset loaded. Create New or Open.");
        }
    }
    ImGui::End();
}

void SPhysicalMaterialEditorWindow::RenderTabsAndToolbar(EViewerType CurrentViewerType)
{
    if (ImGui::BeginTabBar("PhysicalMaterialTabs", ImGuiTabBarFlags_AutoSelectNewTabs | ImGuiTabBarFlags_Reorderable))
    {
        for (int i = 0; i < Tabs.Num(); ++i)
        {
            PhysicalMaterialEditorState* State = static_cast<PhysicalMaterialEditorState*>(Tabs[i]);
            bool open = true;
            
            FString DisplayName = State->Name.ToString();
            if (State->bIsDirty) DisplayName += "*";

            if (ImGui::BeginTabItem((DisplayName + "###Tab" + std::to_string(i)).c_str(), &open))
            {
                if (ActiveState != State)
                {
                    ActiveTabIndex = i;
                    ActiveState = State;
                }
                ImGui::EndTabItem();
            }

            if (!open)
            {
                CloseTab(i);
                ImGui::EndTabBar();
                return;
            }
        }
        ImGui::EndTabBar();
    }

    RenderToolbar();
}

void SPhysicalMaterialEditorWindow::RenderToolbar()
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));
    
    const float IconSize = 24.0f;
    const ImVec2 IconVec(IconSize, IconSize);

    // 1. New
    if (IconNew && IconNew->GetShaderResourceView())
    {
        if (ImGui::ImageButton("##New", (ImTextureID)IconNew->GetShaderResourceView(), IconVec))
        {
            CreateNewAsset();
        }
    }
    else
    {
        if (ImGui::Button("New"))
        {
            CreateNewAsset();
        }
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Create New Asset");

    ImGui::SameLine();

    // 2. Open
    if (IconOpen && IconOpen->GetShaderResourceView())
    {
        if (ImGui::ImageButton("##Open", (ImTextureID)IconOpen->GetShaderResourceView(), IconVec))
        {
            OpenAsset();
        }
    }
    else
    {
        if (ImGui::Button("Open"))
        {
            OpenAsset();
        }
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Open Asset");

    ImGui::SameLine();

    // 3. Save
    if (IconSave && IconSave->GetShaderResourceView())
    {
        if (ImGui::ImageButton("##Save", (ImTextureID)IconSave->GetShaderResourceView(), IconVec))
        {
            SaveAsset();
        }
    }
    else
    {
        if (ImGui::Button("Save"))
        {
            SaveAsset();
        }
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Save Asset");
    
    ImGui::SameLine();

    // 4. Save As Button
    if (IconSaveAs && IconSaveAs->GetShaderResourceView())
    {
        if (ImGui::ImageButton("##SaveAs", (ImTextureID)IconSaveAs->GetShaderResourceView(), IconVec))
        {
            SaveAssetAs();
        }
    }
    else
    {
        if (ImGui::Button("Save As"))
        {
            SaveAssetAs();
        }
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Save Asset As...");

    if (PhysicalMaterialEditorState* State = GetActiveState())
    {
        ImGui::SameLine();
        ImGui::TextDisabled("|  %s", State->CurrentFilePath.empty() ? "Untitled (Not Saved)" : State->CurrentFilePath.c_str());
    }
    
    ImGui::Separator();
    ImGui::PopStyleVar();
}

void SPhysicalMaterialEditorWindow::RenderDetailsPanel()
{
    PhysicalMaterialEditorState* State = GetActiveState();
    if (!State || !State->EditingAsset) return;

    static char SearchBuf[128] = "";
    
    ImGui::Spacing();
    
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 12.0f);
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
    ImGui::InputTextWithHint("##Search", "Search Details", SearchBuf, IM_ARRAYSIZE(SearchBuf));
    ImGui::PopStyleVar();
    
    ImGui::Spacing();
    ImGui::Separator();

    ImGui::BeginChild("DetailsScrollRegion", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);

    if (UPropertyRenderer::RenderAllPropertiesWithInheritance(State->EditingAsset))
    {
        State->bIsDirty = true;
        State->EditingAsset->UpdatePhysicsMaterial();
    }

    ImGui::EndChild();
}

void SPhysicalMaterialEditorWindow::CreateNewAsset()
{
    ViewerState* NewState = CreateViewerState("Untitled", nullptr);
    if (NewState)
    {
        Tabs.Add(NewState);
        ActiveTabIndex = Tabs.Num() - 1;
        ActiveState = NewState;
    }
}

void SPhysicalMaterialEditorWindow::OpenAsset()
{
    std::wstring widePath = FPlatformProcess::OpenLoadFileDialog(
        UTF8ToWide(GDataDir),
        L"phxmtl",
        L"phxmtl"
    );

    if (widePath.empty()) return;

    FString FilePath = WideToUTF8(widePath);
    OpenOrFocusAsset(FilePath);
}

void SPhysicalMaterialEditorWindow::OnSave()
{
    SaveAsset();
}

void SPhysicalMaterialEditorWindow::SaveAsset()
{
    PhysicalMaterialEditorState* State = GetActiveState();
    if (!State || !State->EditingAsset) return;

    // 1. 저장된 경로가 없으면(새 파일) -> 경로 지정 다이얼로그
    if (State->CurrentFilePath.empty())
    {
        SaveAssetAs();
        return;  // SaveAssetAs에서 경로 설정 후 다시 SaveAsset 호출
    }

    // 2. 저장 수행 (경로가 있으면 바로 저장)
    JSON JsonHandle;
    State->EditingAsset->Serialize(false, JsonHandle);

    FWideString WidePath(State->CurrentFilePath.begin(), State->CurrentFilePath.end());
    if (FJsonSerializer::SaveJsonToFile(JsonHandle, WidePath))
    {
        State->bIsDirty = false;

        // 3. 리소스 매니저에 등록 (다른 곳에서 Load로 불러올 수 있도록)
        UResourceManager::GetInstance().AddOrReplace<UPhysicalMaterial>(State->CurrentFilePath, State->EditingAsset);

        UE_LOG("[PhysicalMaterialEditor] Saved: %s", State->CurrentFilePath.c_str());
    }
    else
    {
        FModalDialog::Get().Show("Error", "Failed to save file.", EModalType::OK);
    }
}

void SPhysicalMaterialEditorWindow::SaveAssetAs()
{
    PhysicalMaterialEditorState* State = GetActiveState();
    if (!State || !State->EditingAsset) return;

    std::wstring widePath = FPlatformProcess::OpenSaveFileDialog(
        UTF8ToWide(GDataDir),
        L"phxmtl",
        L"phxmtl"
    );

    if (widePath.empty()) return;

    State->CurrentFilePath = WideToUTF8(widePath);

    std::filesystem::path fsPath(widePath);
    State->Name = WideToUTF8(fsPath.filename().wstring()).c_str();

    // 경로 설정 후 실제 저장 수행
    SaveAsset();
}
