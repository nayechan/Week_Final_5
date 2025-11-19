#include "pch.h"
#include "SViewerWindow.h"
#include "FViewport.h"
#include "FViewportClient.h"
#include "SelectionManager.h"
#include "SlateManager.h"
#include "SSkeletalMeshViewerWindow.h"
#include "SAnimationViewerWindow.h"
#include "SBlendSpaceEditorWindow.h"
#include "Source/Editor/FBXLoader.h"
#include "Source/Editor/PlatformProcess.h"
#include "Source/Runtime/Engine/GameFramework/SkeletalMeshActor.h"
#include "Source/Runtime/Engine/GameFramework/CameraActor.h"
#include "Source/Runtime/Engine/Components/CameraComponent.h"
#include "Source/Runtime/Engine/Viewer/EditorAssetPreviewContext.h"
#include "Texture.h"
#include "Gizmo/GizmoActor.h"
#include "ResourceManager.h"

SViewerWindow::SViewerWindow()
{
}

SViewerWindow::~SViewerWindow()
{
}

static inline FString GetBaseFilenameFromPath(const FString& InPath)
{
    const size_t lastSlash = InPath.find_last_of("/\\");
    FString filename = (lastSlash == FString::npos) ? InPath : InPath.substr(lastSlash + 1);

    const size_t lastDot = filename.find_last_of('.');
    if (lastDot != FString::npos)
    {
        filename = filename.substr(0, lastDot);
    }
    return filename;
}

bool SViewerWindow::Initialize(float StartX, float StartY, float Width, float Height, UWorld* InWorld, ID3D11Device* InDevice, UEditorAssetPreviewContext* InContext)
{
    World = InWorld;
    Device = InDevice;
    Context = InContext;

    // Create window title
    FString BaseTitle = GetWindowTitle();
    if (Context && !Context->AssetPath.empty())
    {
        // ex) "Data/Mesh/MyAsset.fbx" -> "MyAsset.fbx"
        std::filesystem::path fsPath(Context->AssetPath);
        FString AssetName = fsPath.filename().string();
        WindowTitle = BaseTitle + " - " + AssetName;
    }
    else
    {
        WindowTitle = BaseTitle;
    }

    SetRect(StartX, StartY, StartX + Width, StartY + Height);

    // 뷰어 툴바 아이콘 로드
    LoadViewerToolbarIcons(Device);

    // Create first tab/state
    FString InitialTabName = "Viewer 1";    // Default name
    if (Context && !Context->AssetPath.empty())
    {
        InitialTabName = GetBaseFilenameFromPath(Context->AssetPath);
    }
    OpenNewTab(InitialTabName.c_str());

    if (ActiveState && ActiveState->Viewport)
    {
        ActiveState->Viewport->Resize((uint32)StartX, (uint32)StartY, (uint32)Width, (uint32)Height);
    }

    bRequestFocus = true;
    return true;
}

void SViewerWindow::OnUpdate(float DeltaSeconds)
{
    if (!ActiveState || !ActiveState->Viewport)
        return;

    // Tick the preview world so editor actors (e.g., gizmo) update visibility/state
    if (ActiveState->World)
    {
        ActiveState->World->Tick(DeltaSeconds);

        // 뷰어 윈도우가 포커스되었을 때만 키보드 입력으로 기즈모 모드 전환
        if (ActiveState->World->GetGizmoActor() && bIsWindowFocused)
            ActiveState->World->GetGizmoActor()->ProcessGizmoModeSwitch();
    }

    if (ActiveState && ActiveState->Client)
    {
        ActiveState->Client->Tick(DeltaSeconds);

        // 뷰어 윈도우가 호버되어 있고 마우스 휠이 움직였을 때 ViewportClient에 전달
        // (ImGui 윈도우 내부의 마우스 휠 입력을 ViewportClient로 전달)
        ImGuiIO& io = ImGui::GetIO();
        if (bIsWindowHovered && io.MouseWheel != 0.0f)
        {
            // InputManager에 마우스 휠 델타 설정 (ViewportClient::MouseWheel에서 사용)
            UInputManager& InputMgr = UInputManager::GetInstance();
            // MouseWheel은 양수/음수로 방향을 나타냄, InputManager에 전달
            InputMgr.ProcessMessage(nullptr, WM_MOUSEWHEEL, MAKEWPARAM(0, (short)(io.MouseWheel * WHEEL_DELTA)), 0);
        }
    }
}

void SViewerWindow::OnMouseMove(FVector2D MousePos)
{
    if (!ActiveState || !ActiveState->Viewport) return;

    // 드래그 중이면 뷰포트 밖으로 나가도 입력 계속 전달 (기즈모 조작 유지)
    if (bLeftMousePressed || CenterRect.Contains(MousePos))
    {
        FVector2D LocalPos = MousePos - FVector2D(CenterRect.Left, CenterRect.Top);
        ActiveState->Viewport->ProcessMouseMove((int32)LocalPos.X, (int32)LocalPos.Y);
    }
}

void SViewerWindow::OnMouseDown(FVector2D MousePos, uint32 Button)
{
    if (!ActiveState || !ActiveState->Viewport) return;

    if (CenterRect.Contains(MousePos))
    {
        FVector2D LocalPos = MousePos - FVector2D(CenterRect.Left, CenterRect.Top);

        // First, always try gizmo picking (pass to viewport)
        ActiveState->Viewport->ProcessMouseButtonDown((int32)LocalPos.X, (int32)LocalPos.Y, (int32)Button);

        // Left click: if no gizmo was picked, try bone picking
        if (Button == 0 && ActiveState->PreviewActor && ActiveState->CurrentMesh && ActiveState->Client && ActiveState->World)
        {
            // Check if gizmo was picked by checking selection
            UActorComponent* SelectedComp = ActiveState->World->GetSelectionManager()->GetSelectedComponent();

            // Only do bone picking if gizmo wasn't selected
            if (!SelectedComp || !Cast<UBoneAnchorComponent>(SelectedComp))
            {
                // Get camera from viewport client
                ACameraActor* Camera = ActiveState->Client->GetCamera();
                if (Camera)
                {
                    // Get camera vectors
                    FVector CameraPos = Camera->GetActorLocation();
                    FVector CameraRight = Camera->GetRight();
                    FVector CameraUp = Camera->GetUp();
                    FVector CameraForward = Camera->GetForward();

                    // Calculate viewport-relative mouse position
                    FVector2D ViewportMousePos(MousePos.X - CenterRect.Left, MousePos.Y - CenterRect.Top);
                    FVector2D ViewportSize(CenterRect.GetWidth(), CenterRect.GetHeight());

                    // Generate ray from mouse position
                    FRay Ray = MakeRayFromViewport(
                        Camera->GetViewMatrix(),
                        Camera->GetProjectionMatrix(CenterRect.GetWidth() / CenterRect.GetHeight(), ActiveState->Viewport),
                        CameraPos,
                        CameraRight,
                        CameraUp,
                        CameraForward,
                        ViewportMousePos,
                        ViewportSize
                    );

                    // Try to pick a bone
                    float HitDistance;
                    int32 PickedBoneIndex = ActiveState->PreviewActor->PickBone(Ray, HitDistance);

                    if (PickedBoneIndex >= 0)
                    {
                        // Bone was picked
                        ActiveState->SelectedBoneIndex = PickedBoneIndex;
                        ActiveState->bBoneLinesDirty = true;
                        ActiveState->bRequestScrollToBone = true;

                        ExpandToSelectedBone(ActiveState, PickedBoneIndex);

                        // Move gizmo to the selected bone
                        ActiveState->PreviewActor->RepositionAnchorToBone(PickedBoneIndex);
                        if (USceneComponent* Anchor = ActiveState->PreviewActor->GetBoneGizmoAnchor())
                        {
                            ActiveState->World->GetSelectionManager()->SelectActor(ActiveState->PreviewActor);
                            ActiveState->World->GetSelectionManager()->SelectComponent(Anchor);
                        }
                    }
                    else
                    {
                        // No bone was picked - clear selection
                        ActiveState->SelectedBoneIndex = -1;
                        ActiveState->SelectedNotify.Invalidate();
                        ActiveState->bBoneLinesDirty = true;

                        // Hide gizmo and clear selection
                        if (UBoneAnchorComponent* Anchor = ActiveState->PreviewActor->GetBoneGizmoAnchor())
                        {
                            Anchor->SetVisibility(false);
                            Anchor->SetEditability(false);
                        }
                        ActiveState->World->GetSelectionManager()->ClearSelection();
                    }
                }
            }
        }

        // 좌클릭: 드래그 시작 (기즈모 조작 또는 본 선택)
        if (Button == 0)
        {
            bLeftMousePressed = true;
        }

        // 우클릭: 카메라 조작 시작 (커서 숨김 및 잠금)
        if (Button == 1)
        {
            INPUT.SetCursorVisible(false);
            INPUT.LockCursor();
            bRightMousePressed = true;
        }
    }
}

void SViewerWindow::OnMouseUp(FVector2D MousePos, uint32 Button)
{
    if (!ActiveState || !ActiveState->Viewport) return;

    // 드래그 중이었으면 뷰포트 밖에서 마우스를 놓아도 처리 (기즈모 해제 위해)
    if (bLeftMousePressed || bRightMousePressed || CenterRect.Contains(MousePos))
    {
        FVector2D LocalPos = MousePos - FVector2D(CenterRect.Left, CenterRect.Top);
        ActiveState->Viewport->ProcessMouseButtonUp((int32)LocalPos.X, (int32)LocalPos.Y, (int32)Button);

        // 좌클릭 해제: 드래그 종료
        if (Button == 0 && bLeftMousePressed)
        {
            bLeftMousePressed = false;
        }

        // 우클릭 해제: 커서 복원 및 잠금 해제
        if (Button == 1 && bRightMousePressed)
        {
            INPUT.SetCursorVisible(true);
            INPUT.ReleaseCursor();
            bRightMousePressed = false;
        }
    }
}



void SViewerWindow::OpenOrFocusTab(UEditorAssetPreviewContext* Context)
{
    if (!Context || Context->AssetPath.empty())
    {
        char label[32];
        sprintf_s(label, "Viewer %d", Tabs.Num() + 1);
        OpenNewTab(label);
        return;
    }

    for (int i = 0; i < Tabs.Num(); ++i)
    {
        ViewerState* State = Tabs[i];
        if (State && State->LoadedMeshPath == Context->AssetPath)
        {
            ActiveTabIndex = i;
            ActiveState = Tabs[i];
            UE_LOG("SViewerWindow: Found existing tab for %s. Switching to it.", Context->AssetPath.c_str());
            return;
        }
    }

    UE_LOG("SViewerWindow: No existing tab for %s. Creating a new one.", Context->AssetPath.c_str());
    FString AssetName = GetBaseFilenameFromPath(Context->AssetPath);
    ViewerState* NewState = CreateViewerState(AssetName.c_str(), Context);

    if (NewState)
    {
        Tabs.Add(NewState);
        ActiveTabIndex = Tabs.Num() - 1;
        ActiveState = NewState;
    }
}

void SViewerWindow::OnRenderViewport()
{
    if (ActiveState && ActiveState->Viewport && CenterRect.GetWidth() > 0 && CenterRect.GetHeight() > 0)
    {
        // Clamp viewport to window bounds to prevent integer wraparound and corruption
        extern float CLIENTWIDTH;
        extern float CLIENTHEIGHT;

        float ClampedLeft = std::max(0.0f, CenterRect.Left);
        float ClampedTop = std::max(0.0f, CenterRect.Top);
        float ClampedRight = std::min(CLIENTWIDTH, CenterRect.Right);
        float ClampedBottom = std::min(CLIENTHEIGHT, CenterRect.Bottom);

        // Skip rendering if viewport is completely outside window bounds
        if (ClampedLeft >= ClampedRight || ClampedTop >= ClampedBottom)
            return;

        const uint32 NewStartX = static_cast<uint32>(ClampedLeft);
        const uint32 NewStartY = static_cast<uint32>(ClampedTop);
        const uint32 NewWidth = static_cast<uint32>(ClampedRight - ClampedLeft);
        const uint32 NewHeight = static_cast<uint32>(ClampedBottom - ClampedTop);
        ActiveState->Viewport->Resize(NewStartX, NewStartY, NewWidth, NewHeight);

        // Child class specific update logic
        PreRenderViewportUpdate();

        // Viewport rendering (before ImGui)
        ActiveState->Viewport->Render();
    }
}

void SViewerWindow::RenderTabBar()
{
    for (int i = 0; i < Tabs.Num(); ++i)
    {
        ViewerState* State = Tabs[i];
        bool open = true;
        if (ImGui::BeginTabItem(State->Name.ToString().c_str(), &open))
        {
            ActiveTabIndex = i;
            ActiveState = State;
            ImGui::EndTabItem();
        }
        if (!open)
        {
            CloseTab(i);
            break;
        }
    }
    if (ImGui::TabItemButton("+", ImGuiTabItemFlags_Trailing))
    {
        char label[32]; sprintf_s(label, "Viewer %d", Tabs.Num() + 1);
        OpenNewTab(label);
    }
    ImGui::EndTabBar();
}

void SViewerWindow::RenderTabsAndToolbar(EViewerType CurrentViewerType)
{
    if (!ImGui::BeginTabBar("ViewerTabs", 
        ImGuiTabBarFlags_AutoSelectNewTabs | ImGuiTabBarFlags_Reorderable))
        return;

    for (int i = 0; i < Tabs.Num(); ++i)
    {
        ViewerState* State = Tabs[i];
        bool open = true;
        if (ImGui::BeginTabItem(State->Name.ToString().c_str(), &open))
        {
            ActiveTabIndex = i;
            ActiveState = State;
            ImGui::EndTabItem();
        }
        if (!open)
        {
            CloseTab(i);
            ImGui::EndTabBar();
            return;
        }
    }
    if (ImGui::TabItemButton("+", ImGuiTabItemFlags_Trailing))
    {
        char label[32]; sprintf_s(label, "Viewer %d", Tabs.Num() + 1);
        OpenNewTab(label);
    }

    const float SkelButtonWidth = 120.0f;
    const float AnimButtonWidth = 135.0f;
    const float BlendSpaceButtonWidth = 145.0f;
    const float spacing = ImGui::GetStyle().ItemSpacing.x;
    const float totalButtonsWidth = (SkelButtonWidth + AnimButtonWidth + BlendSpaceButtonWidth) + (spacing * 2);

    ImGui::SameLine();
    float availableWidth = ImGui::GetContentRegionAvail().x;
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + availableWidth - totalButtonsWidth);

    // Skeletal Viewer Button
    bool bSkeletalDisabled = (CurrentViewerType == EViewerType::Skeletal);
    if (bSkeletalDisabled)  ImGui::BeginDisabled();

    if (ImGui::Button("Skeletal Viewer", ImVec2(SkelButtonWidth, 0)))
    {
        // Find existing skeletal viewer window and focus it
        SViewerWindow* TargetWindow = nullptr;
        for (SWindow* Window : USlateManager::GetInstance().GetDetachedWindows())
        {
            if (dynamic_cast<SSkeletalMeshViewerWindow*>(Window))
            {
                TargetWindow = static_cast<SViewerWindow*>(Window);
                break;
            }
        }

        if (TargetWindow)
        {
            // Just focus the existing window
            TargetWindow->RequestFocus();
        }
        else if (ActiveState && ActiveState->CurrentMesh)
        {
            // Create new window only if none exists
            UEditorAssetPreviewContext* Context = NewObject<UEditorAssetPreviewContext>();
            Context->ViewerType = EViewerType::Skeletal;
            Context->AssetPath = ActiveState->LoadedMeshPath;
            USlateManager::GetInstance().OpenAssetViewer(Context);
        }
    }
    if (bSkeletalDisabled)  ImGui::EndDisabled();
    ImGui::SameLine();

    // Animation Viewer Button
    bool bAnimDisabled = (CurrentViewerType == EViewerType::Animation);
    if (bAnimDisabled)  ImGui::BeginDisabled();

    if (ImGui::Button("Animation Viewer", ImVec2(AnimButtonWidth, 0)))
    {
        // Find existing animation viewer window and focus it
        SViewerWindow* TargetWindow = nullptr;
        for (SWindow* Window : USlateManager::GetInstance().GetDetachedWindows())
        {
            if (dynamic_cast<SAnimationViewerWindow*>(Window))
            {
                TargetWindow = static_cast<SViewerWindow*>(Window);
                break;
            }
        }

        if (TargetWindow)
        {
            // Just focus the existing window
            TargetWindow->RequestFocus();
        }
        else if (ActiveState && ActiveState->CurrentMesh)
        {
            // Create new window only if none exists
            UEditorAssetPreviewContext* Context = NewObject<UEditorAssetPreviewContext>();
            Context->ViewerType = EViewerType::Animation;
            Context->AssetPath = ActiveState->LoadedMeshPath;
            USlateManager::GetInstance().OpenAssetViewer(Context);
        }
    }
    if (bAnimDisabled)  ImGui::EndDisabled();
    ImGui::SameLine();

    // Blend Space Editor Button
    bool bBlendSpaceDisabled = (CurrentViewerType == EViewerType::BlendSpace);
    if (bBlendSpaceDisabled)  ImGui::BeginDisabled();

    if (ImGui::Button("BlendSpace Editor", ImVec2(BlendSpaceButtonWidth, 0)))
    {
        // Find existing blend space editor window and focus it
        SViewerWindow* TargetWindow = nullptr;
        for (SWindow* Window : USlateManager::GetInstance().GetDetachedWindows())
        {
            if (dynamic_cast<SBlendSpaceEditorWindow*>(Window))
            {
                TargetWindow = static_cast<SViewerWindow*>(Window);
                break;
            }
        }

        if (TargetWindow)
        {
            // Just focus the existing window
            TargetWindow->RequestFocus();
        }
        else if (ActiveState && ActiveState->CurrentMesh)
        {
            // Create new window only if none exists
            UEditorAssetPreviewContext* Context = NewObject<UEditorAssetPreviewContext>();
            Context->ViewerType = EViewerType::BlendSpace;
            Context->AssetPath = ActiveState->LoadedMeshPath;
            USlateManager::GetInstance().OpenAssetViewer(Context);
        }
    }
    if (bBlendSpaceDisabled)  ImGui::EndDisabled();

    ImGui::EndTabBar();
}

void SViewerWindow::OpenNewTab(const char* Name)
{
    ViewerState* State = CreateViewerState(Name, Context);
    if (!State) return;

    Tabs.Add(State);
    ActiveTabIndex = Tabs.Num() - 1;
    ActiveState = State;
}

void SViewerWindow::CloseTab(int Index)
{
    if (Index < 0 || Index >= Tabs.Num()) return;
    ViewerState* State = Tabs[Index];
    DestroyViewerState(State);
    Tabs.RemoveAt(Index);
    if (Tabs.Num() == 0) { ActiveTabIndex = -1; ActiveState = nullptr; }
    else { ActiveTabIndex = std::min(Index, Tabs.Num() - 1); ActiveState = Tabs[ActiveTabIndex]; }
}

void SViewerWindow::RenderLeftPanel(float PanelWidth)
{
    if (!ActiveState)   return;

    ImGuiStyle& style = ImGui::GetStyle();
    float spacing = style.ItemSpacing.y;

    // Asset Browser Section
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 6);
    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.30f, 0.30f, 0.30f, 0.8f));
    ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]);
    ImGui::Text("ASSET BROWSER");
    ImGui::PopFont();
    ImGui::PopStyleColor();
    ImGui::Dummy(ImVec2(0, 6));
    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(0.35f, 0.35f, 0.35f, 0.6f));
    ImGui::Separator();
    ImGui::PopStyleColor();
    ImGui::Dummy(ImVec2(0, 6));
    ImGui::Spacing();

    // Mesh path section
    ImGui::BeginGroup();
    ImGui::PushItemWidth(-1.0f);
    ImGui::InputTextWithHint("##MeshPath", "Browse for FBX file...", ActiveState->MeshPathBuffer, sizeof(ActiveState->MeshPathBuffer));
    ImGui::PopItemWidth();
    ImGui::Dummy(ImVec2(0, 4));
    ImGui::Spacing();

    // Buttons
    float innerPadding = 8.0f;  // 좌우 여백
    float availWidth = PanelWidth - innerPadding * 2.0f;
    float buttonHeight = 30.0f;
    float buttonWidth = (availWidth - 6.0f) * 0.5f;

    // Browse...
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.23f, 0.25f, 0.27f, 1.00f)); // #3A3F45
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.28f, 0.32f, 0.34f, 1.00f)); // #485057
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.18f, 0.20f, 0.21f, 1.00f)); // #2E3337
    if (ImGui::Button("Browse...", ImVec2(buttonWidth, buttonHeight)))
    {
        auto widePath = FPlatformProcess::OpenLoadFileDialog(
            UTF8ToWide(GDataDir), L"fbx", L"FBX Files");

        if (!widePath.empty())
        {
            std::string s = widePath.string();
            strncpy_s(ActiveState->MeshPathBuffer, s.c_str(),
                sizeof(ActiveState->MeshPathBuffer) - 1);
        }
    }

    ImGui::PopStyleColor(3);
    ImGui::SameLine(0.0f, 4.0f);
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.29f, 0.44f, 0.63f, 1.00f)); // #4A6FA0
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.36f, 0.51f, 0.72f, 1.00f)); // #5B82B8
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.22f, 0.36f, 0.51f, 1.00f)); // #385B82

    if (ImGui::Button("Load FBX", ImVec2(buttonWidth, buttonHeight)))
    {
        FString Path = ActiveState->MeshPathBuffer;
        if (!Path.empty())
        {
            USkeletalMesh* Mesh = UResourceManager::GetInstance().Load<USkeletalMesh>(Path);
            if (Mesh && ActiveState->PreviewActor)
            {
                ActiveState->PreviewActor->SetSkeletalMesh(Path);
                ActiveState->CurrentMesh = Mesh;

                // Expand all bone nodes by default on mesh load
                ActiveState->ExpandedBoneIndices.clear();
                if (const FSkeleton* Skeleton = ActiveState->CurrentMesh->GetSkeleton())
                {
                    // Auto-load an animation from the same FBX (if available) for convenience
                    if (UAnimSequence* Anim = UFbxLoader::GetInstance().LoadFbxAnimation(Path, Skeleton))
                    {
                        ActiveState->CurrentAnimation = Anim;
                        ActiveState->TotalTime = Anim->GetSequenceLength();
                        ActiveState->CurrentTime = 0.0f;
                        ActiveState->bIsPlaying = true;

                        // Use the settings of ActiveState
                        ActiveState->PreviewActor
                            ->GetSkeletalMeshComponent()
                            ->PlayAnimation(Anim, ActiveState->bIsLooping, ActiveState->PlaybackSpeed);
                    }
                    for (int32 i = 0; i < Skeleton->Bones.size(); ++i)
                    {
                        ActiveState->ExpandedBoneIndices.insert(i);
                    }
                }

                ActiveState->LoadedMeshPath = Path;  // Track for resource unloading
                if (auto* Skeletal = ActiveState->PreviewActor->GetSkeletalMeshComponent())
                {
                    Skeletal->SetVisibility(ActiveState->bShowMesh);
                }
                ActiveState->bBoneLinesDirty = true;
                if (auto* LineComp = ActiveState->PreviewActor->GetBoneLineComponent())
                {
                    LineComp->ClearLines();
                    LineComp->SetLineVisible(ActiveState->bShowBones);
                }
            }
        }
    }
    ImGui::PopStyleColor(3);
    ImGui::EndGroup();

    // Section divider
    ImGui::Dummy(ImVec2(0, 4));
    ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(0.40f, 0.40f, 0.40f, 0.80f));
    ImGui::Separator();
    ImGui::PopStyleColor();
    ImGui::Dummy(ImVec2(0, 8));
    ImGui::Spacing();

    // Display Options
    ImGui::BeginGroup();

    // Title
    ImGui::Text("DISPLAY OPTIONS");
    ImGui::Dummy(ImVec2(0, 4));
    // Checkbox Style
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(1.5, 1.5));
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.23f, 0.25f, 0.27f, 0.80f)); // #3A3F45 계열
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.28f, 0.30f, 0.33f, 0.90f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.20f, 0.22f, 0.25f, 1.00f));
    ImGui::PushStyleColor(ImGuiCol_CheckMark, ImVec4(0.75f, 0.80f, 0.90f, 1.00f));

    // Checkboxes
    if (ImGui::Checkbox("Show Mesh", &ActiveState->bShowMesh))
    {
        if (auto* comp = ActiveState->PreviewActor->GetSkeletalMeshComponent())
            comp->SetVisibility(ActiveState->bShowMesh);
    }

    ImGui::SameLine(0.0f, 12.0f);

    if (ImGui::Checkbox("Show Bones", &ActiveState->bShowBones))
    {
        if (auto* lineComp = ActiveState->PreviewActor->GetBoneLineComponent())
            lineComp->SetLineVisible(ActiveState->bShowBones);

        if (ActiveState->bShowBones)
            ActiveState->bBoneLinesDirty = true;
    }

    ImGui::PopStyleColor(4);
    ImGui::PopStyleVar();
    ImGui::EndGroup();

    // Section divider
    ImGui::Dummy(ImVec2(0, 8));
    ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(0.40f, 0.40f, 0.40f, 0.75f));
    ImGui::Separator();
    ImGui::PopStyleColor();
    ImGui::Dummy(ImVec2(0, 8));

    // Bone Hierarchy Section
    ImGui::Text("BONE HIERARCHY");
    ImGui::Dummy(ImVec2(0, 2));
    ImGui::Spacing();

    if (!ActiveState->CurrentMesh)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
        ImGui::TextWrapped("No skeletal mesh loaded.");
        ImGui::PopStyleColor();
    }
    else
    {
        const FSkeleton* Skeleton = ActiveState->CurrentMesh->GetSkeleton();
        if (!Skeleton || Skeleton->Bones.IsEmpty())
        {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
            ImGui::TextWrapped("This mesh has no skeleton data.");
            ImGui::PopStyleColor();
        }
        else
        {
            // Scrollable tree view
            ImGui::BeginChild("BoneTreeView", ImVec2(0, 0), true, ImGuiWindowFlags_NoScrollbar);
            ImGui::PushStyleVar(ImGuiStyleVar_IndentSpacing, 12.0f);
            const TArray<FBone>& Bones = Skeleton->Bones;
            TArray<TArray<int32>> Children;
            Children.resize(Bones.size());
            for (int32 i = 0; i < Bones.size(); ++i)
            {
                int32 Parent = Bones[i].ParentIndex;
                if (Parent >= 0 && Parent < Bones.size())
                {
                    Children[Parent].Add(i);
                }
            }

            std::function<void(int32)> DrawNode = [&](int32 Index)
                {
                    const bool bLeaf = Children[Index].IsEmpty();
                    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanFullWidth;

                    if (bLeaf)
                    {
                        flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
                    }

                    // 펼쳐진 노드는 명시적으로 열린 상태로 설정
                    if (ActiveState->ExpandedBoneIndices.count(Index) > 0)
                    {
                        ImGui::SetNextItemOpen(true);
                    }

                    if (ActiveState->SelectedBoneIndex == Index)
                    {
                        flags |= ImGuiTreeNodeFlags_Selected;
                    }

                    ImGui::PushID(Index);
                    const char* Label = Bones[Index].Name.c_str();

                    if (ActiveState->SelectedBoneIndex == Index)
                    {
                        ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.35f, 0.55f, 0.85f, 0.8f));
                        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.40f, 0.60f, 0.90f, 1.0f));
                        ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.30f, 0.50f, 0.80f, 1.0f));
                    }

                    bool open = ImGui::TreeNodeEx((void*)(intptr_t)Index, flags, "%s", Label ? Label : "<noname>");

                    if (ActiveState->SelectedBoneIndex == Index)
                    {
                        ImGui::PopStyleColor(3);

                        if (ActiveState->bRequestScrollToBone)
                        {
                            ImGui::SetScrollHereY(0.5f);    // 선택된 본까지 스크롤
                            ActiveState->bRequestScrollToBone = false; // 요청 처리 완료
                        }
                    }

                    // 사용자가 수동으로 노드를 접거나 펼쳤을 때 상태 업데이트
                    if (ImGui::IsItemToggledOpen())
                    {
                        if (open)
                            ActiveState->ExpandedBoneIndices.insert(Index);
                        else
                            ActiveState->ExpandedBoneIndices.erase(Index);
                    }

                    if (ImGui::IsItemClicked())
                    {
                        if (ActiveState->SelectedBoneIndex != Index)
                        {
                            ActiveState->SelectedBoneIndex = Index;
                            ActiveState->SelectedNotify.Invalidate();
                            ActiveState->bBoneLinesDirty = true;
                            ActiveState->bRequestScrollToBone = true;

                            ExpandToSelectedBone(ActiveState, Index);

                            if (ActiveState->PreviewActor && ActiveState->World)
                            {
                                ActiveState->PreviewActor->RepositionAnchorToBone(Index);
                                if (USceneComponent* Anchor = ActiveState->PreviewActor->GetBoneGizmoAnchor())
                                {
                                    ActiveState->World->GetSelectionManager()->SelectActor(ActiveState->PreviewActor);
                                    ActiveState->World->GetSelectionManager()->SelectComponent(Anchor);
                                }
                            }
                        }
                    }

                    if (!bLeaf && open)
                    {
                        for (int32 Child : Children[Index])
                        {
                            DrawNode(Child);
                        }
                        ImGui::TreePop();
                    }
                    ImGui::PopID();
                };

            for (int32 i = 0; i < Bones.size(); ++i)
            {
                if (Bones[i].ParentIndex < 0)
                {
                    DrawNode(i);
                }
            }
            ImGui::PopStyleVar();
            ImGui::EndChild();
        }
    }
}

void SViewerWindow::RenderRightPanel()
{
    if (!ActiveState)   return;

    ImGuiStyle& style = ImGui::GetStyle();
    
    // Panel header
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 6);
    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.30f, 0.30f, 0.30f, 0.8f));
    ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]);
    ImGui::Text("BONE PROPERTIES");
    ImGui::PopFont();
    ImGui::PopStyleColor();
    ImGui::Dummy(ImVec2(0, 6));
    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(0.35f, 0.35f, 0.35f, 0.6f));
    ImGui::Separator();
    ImGui::PopStyleColor();
    ImGui::Dummy(ImVec2(0, 6));
    ImGui::Spacing();

    // === 선택된 본의 트랜스폼 편집 UI ===
    if (ActiveState->SelectedBoneIndex < 0 || !ActiveState->CurrentMesh)
    {
        ImGui::TextWrapped("Select a bone from the hierarchy to edit.");
        return;
    }

    const FSkeleton* Skeleton = ActiveState->CurrentMesh->GetSkeleton();
    if (!Skeleton || ActiveState->SelectedBoneIndex >= Skeleton->Bones.size())
        return;

    const FBone& SelectedBone = Skeleton->Bones[ActiveState->SelectedBoneIndex];

    // Bone Name
    ImGui::Text("Selected Bone");
    ImGui::Separator();
    ImGui::TextWrapped("%s", SelectedBone.Name.c_str());
    ImGui::Dummy(ImVec2(0, 4));

    // Current Transform
    if (!ActiveState->bBoneRotationEditing)
        UpdateBoneTransformFromSkeleton(ActiveState);

    // ------------------------------------------
    // LOCATION
    // ------------------------------------------
    ImGui::Text("Location");
    bool bLocChanged = false;

    ImGui::PushItemWidth(-1);
    bLocChanged |= ImGui::DragFloat("X##loc", &ActiveState->EditBoneLocation.X, 0.1f);
    bLocChanged |= ImGui::DragFloat("Y##loc", &ActiveState->EditBoneLocation.Y, 0.1f);
    bLocChanged |= ImGui::DragFloat("Z##loc", &ActiveState->EditBoneLocation.Z, 0.1f);
    ImGui::PopItemWidth();

    if (bLocChanged) {
        ApplyBoneTransform(ActiveState);
        ActiveState->bBoneLinesDirty = true;
    }

    ImGui::Dummy(ImVec2(0, 8));

    // ------------------------------------------
    // ROTATION
    // ------------------------------------------
    ImGui::Text("Rotation");

    bool bRotChanged = false;

    if (ImGui::IsAnyItemActive())
        ActiveState->bBoneRotationEditing = true;

    ImGui::PushItemWidth(-1);
    bRotChanged |= ImGui::DragFloat("X##rot", &ActiveState->EditBoneRotation.X, 0.5f, -180.0f, 180.0f);
    bRotChanged |= ImGui::DragFloat("Y##rot", &ActiveState->EditBoneRotation.Y, 0.5f, -180.0f, 180.0f);
    bRotChanged |= ImGui::DragFloat("Z##rot", &ActiveState->EditBoneRotation.Z, 0.5f, -180.0f, 180.0f);
    ImGui::PopItemWidth();

    if (!ImGui::IsAnyItemActive())
        ActiveState->bBoneRotationEditing = false;

    if (bRotChanged) {
        ApplyBoneTransform(ActiveState);
        ActiveState->bBoneLinesDirty = true;
    }

    ImGui::Dummy(ImVec2(0, 8));

    // ------------------------------------------
    // SCALE
    // ------------------------------------------
    ImGui::Text("Scale");

    bool bScaleChanged = false;
    ImGui::PushItemWidth(-1);
    bScaleChanged |= ImGui::DragFloat("X##scale", &ActiveState->EditBoneScale.X, 0.01f, 0.001f, 100.f);
    bScaleChanged |= ImGui::DragFloat("Y##scale", &ActiveState->EditBoneScale.Y, 0.01f, 0.001f, 100.f);
    bScaleChanged |= ImGui::DragFloat("Z##scale", &ActiveState->EditBoneScale.Z, 0.01f, 0.001f, 100.f);
    ImGui::PopItemWidth();

    if (bScaleChanged) {
        ApplyBoneTransform(ActiveState);
        ActiveState->bBoneLinesDirty = true;
    }
}

void SViewerWindow::UpdateBoneTransformFromSkeleton(ViewerState* State)
{
    if (!State || !State->CurrentMesh || State->SelectedBoneIndex < 0)
        return;

    // Get the animated local transform
    FTransform AnimatedLocalTransform;
    if (State->CurrentAnimation)
    {
        TArray<FTransform> AllBoneTransforms;
        State->CurrentAnimation->ExtractBonePose(*State->CurrentMesh->GetSkeleton(), State->CurrentTime, false, false, AllBoneTransforms);
        if (State->SelectedBoneIndex < AllBoneTransforms.Num())
        {
            AnimatedLocalTransform = AllBoneTransforms[State->SelectedBoneIndex];
        }
    }
    else
    {
        if (State->PreviewActor && State->PreviewActor->GetSkeletalMeshComponent() &&
            State->SelectedBoneIndex < State->PreviewActor->GetSkeletalMeshComponent()->RefPose.Num())
        {
            AnimatedLocalTransform = State->PreviewActor->GetSkeletalMeshComponent()->RefPose[State->SelectedBoneIndex];
        }
    }

    // Check if there is an additive transform for this bone
    FTransform FinalLocalTransform = AnimatedLocalTransform;
    if (FTransform* Additive = State->BoneAdditiveTransforms.Find(State->SelectedBoneIndex))
    {
        FinalLocalTransform = (*Additive) * AnimatedLocalTransform;
    }

    // Update the UI edit fields with the final transform
    State->EditBoneLocation = FinalLocalTransform.Translation;
    State->EditBoneRotation = FinalLocalTransform.Rotation.ToEulerZYXDeg();
    State->EditBoneScale = FinalLocalTransform.Scale3D;
}

void SViewerWindow::UpdateBoneTransformFromGizmo(ViewerState* State)
{
    if (!State || State->SelectedBoneIndex < 0 || !State->PreviewActor || !State->CurrentMesh)
        return;

    USkeletalMeshComponent* MeshComp = State->PreviewActor->GetSkeletalMeshComponent();
    USceneComponent* Anchor = State->PreviewActor->GetBoneGizmoAnchor();
    const FSkeleton* Skeleton = State->CurrentMesh->GetSkeleton();

    if (!MeshComp || !Anchor || !Skeleton)
        return;

    // Get the world transform of the anchor manipulated by the gizmo
    const FTransform& AnchorWorldTransform = Anchor->GetWorldTransform();

    // Convert this world transform into the bone's local space (relative to the parent bone)
    const int32 ParentIndex = Skeleton->Bones[State->SelectedBoneIndex].ParentIndex;
    FTransform ParentWorldTransform;
    if (ParentIndex != -1)
    {
        // Get the parent bone's final world transform
        ParentWorldTransform = MeshComp->GetBoneWorldTransform(ParentIndex);
    }
    else
    {
        // For the root bone, the parent is the skeletal mesh component itself
        ParentWorldTransform = MeshComp->GetWorldTransform();
    }

    // Calculate the desired local transform relative to the parent's world transform
    FTransform DesiredLocalTransform = ParentWorldTransform.GetRelativeTransform(AnchorWorldTransform);

    // Update the state's editable bone transform parameters using the calculated local transform
    State->EditBoneLocation = DesiredLocalTransform.Translation;
    State->EditBoneRotation = DesiredLocalTransform.Rotation.ToEulerZYXDeg();
    State->EditBoneScale = DesiredLocalTransform.Scale3D;

    // Apply the updated local bone transform to the additive transform map
    ApplyBoneTransform(State);
}

void SViewerWindow::ApplyBoneTransform(ViewerState* State)
{
    if (!State || !State->CurrentMesh || State->SelectedBoneIndex < 0)
        return;

    // This is the desired final local transform from the UI/gizmo
    FTransform DesiredFinalLocal(State->EditBoneLocation, FQuat::MakeFromEulerZYX(State->EditBoneRotation), State->EditBoneScale);

    // Get the bone's transform from the current animation pose (without any additive offsets)
    FTransform AnimatedLocalTransform;
    if (State->CurrentAnimation)
    {
        TArray<FTransform> AllBoneTransforms;
        State->CurrentAnimation->ExtractBonePose(
            *State->CurrentMesh->GetSkeleton(),
            State->CurrentTime,
            false, // no looping in this context
            false, // no interpolation
            AllBoneTransforms);

        if (State->SelectedBoneIndex < AllBoneTransforms.Num())
        {
            AnimatedLocalTransform = AllBoneTransforms[State->SelectedBoneIndex];
        }
    }
    else
    {
        // No animation, base pose is the reference pose
        if (State->PreviewActor && State->PreviewActor->GetSkeletalMeshComponent() &&
            State->SelectedBoneIndex < State->PreviewActor->GetSkeletalMeshComponent()->RefPose.Num())
        {
            AnimatedLocalTransform = State->PreviewActor->GetSkeletalMeshComponent()->RefPose[State->SelectedBoneIndex];
        }
    }

    // The new additive transform is the difference
    FTransform AdditiveTransform = DesiredFinalLocal * AnimatedLocalTransform.Inverse();

    // Store it
    State->BoneAdditiveTransforms[State->SelectedBoneIndex] = AdditiveTransform;
}

void SViewerWindow::ExpandToSelectedBone(ViewerState* State, int32 BoneIndex)
{
    if (!State || !State->CurrentMesh)
        return;

    const FSkeleton* Skeleton = State->CurrentMesh->GetSkeleton();
    if (!Skeleton || BoneIndex < 0 || BoneIndex >= Skeleton->Bones.size())
        return;

    // 선택된 본부터 루트까지 모든 부모를 펼침
    int32 CurrentIndex = BoneIndex;
    while (CurrentIndex >= 0)
    {
        State->ExpandedBoneIndices.insert(CurrentIndex);
        CurrentIndex = Skeleton->Bones[CurrentIndex].ParentIndex;
    }
}

// ========================================
// 뷰어 툴바 관련 메서드 구현
// ========================================

void SViewerWindow::LoadViewerToolbarIcons(ID3D11Device* Device)
{
    // 기즈모 아이콘 로드
    IconSelect = NewObject<UTexture>();
    IconSelect->Load(GDataDir + "/Icon/Viewport_Toolbar_Select.png", Device);

    IconMove = NewObject<UTexture>();
    IconMove->Load(GDataDir + "/Icon/Viewport_Toolbar_Move.png", Device);

    IconRotate = NewObject<UTexture>();
    IconRotate->Load(GDataDir + "/Icon/Viewport_Toolbar_Rotate.png", Device);

    IconScale = NewObject<UTexture>();
    IconScale->Load(GDataDir + "/Icon/Viewport_Toolbar_Scale.png", Device);

    IconWorldSpace = NewObject<UTexture>();
    IconWorldSpace->Load(GDataDir + "/Icon/Viewport_Toolbar_WorldSpace.png", Device);

    IconLocalSpace = NewObject<UTexture>();
    IconLocalSpace->Load(GDataDir + "/Icon/Viewport_Toolbar_LocalSpace.png", Device);

    // 카메라 모드 아이콘 로드
    IconCamera = NewObject<UTexture>();
    IconCamera->Load(GDataDir + "/Icon/Viewport_Mode_Camera.png", Device);

    IconPerspective = NewObject<UTexture>();
    IconPerspective->Load(GDataDir + "/Icon/Viewport_Mode_Perspective.png", Device);

    // 직교 모드 아이콘 로드
    IconTop = NewObject<UTexture>();
    IconTop->Load(GDataDir + "/Icon/Viewport_Mode_Top.png", Device);

    IconBottom = NewObject<UTexture>();
    IconBottom->Load(GDataDir + "/Icon/Viewport_Mode_Bottom.png", Device);

    IconLeft = NewObject<UTexture>();
    IconLeft->Load(GDataDir + "/Icon/Viewport_Mode_Left.png", Device);

    IconRight = NewObject<UTexture>();
    IconRight->Load(GDataDir + "/Icon/Viewport_Mode_Right.png", Device);

    IconFront = NewObject<UTexture>();
    IconFront->Load(GDataDir + "/Icon/Viewport_Mode_Front.png", Device);

    IconBack = NewObject<UTexture>();
    IconBack->Load(GDataDir + "/Icon/Viewport_Mode_Back.png", Device);

    // 카메라 설정 아이콘 로드
    IconSpeed = NewObject<UTexture>();
    IconSpeed->Load(GDataDir + "/Icon/Viewport_Camera_Speed.png", Device);

    IconFOV = NewObject<UTexture>();
    IconFOV->Load(GDataDir + "/Icon/Viewport_Camera_FOV.png", Device);

    IconNearClip = NewObject<UTexture>();
    IconNearClip->Load(GDataDir + "/Icon/Viewport_Camera_NearClip.png", Device);

    IconFarClip = NewObject<UTexture>();
    IconFarClip->Load(GDataDir + "/Icon/Viewport_Camera_FarClip.png", Device);

    // 뷰모드 아이콘 로드
    IconViewMode_Lit = NewObject<UTexture>();
    IconViewMode_Lit->Load(GDataDir + "/Icon/Viewport_ViewMode_Lit.png", Device);

    IconViewMode_Unlit = NewObject<UTexture>();
    IconViewMode_Unlit->Load(GDataDir + "/Icon/Viewport_ViewMode_Unlit.png", Device);

    IconViewMode_Wireframe = NewObject<UTexture>();
    IconViewMode_Wireframe->Load(GDataDir + "/Icon/Viewport_Toolbar_WorldSpace.png", Device);

    IconViewMode_BufferVis = NewObject<UTexture>();
    IconViewMode_BufferVis->Load(GDataDir + "/Icon/Viewport_ViewMode_BufferVis.png", Device);
}

AGizmoActor* SViewerWindow::GetGizmoActor()
{
    if (ActiveState && ActiveState->World)
    {
        return ActiveState->World->GetGizmoActor();
    }
    return nullptr;
}

void SViewerWindow::RenderViewerGizmoButtons()
{
    const ImVec2 IconSize(17, 17);

    // GizmoActor에서 직접 현재 모드 가져오기
    EGizmoMode CurrentGizmoMode = EGizmoMode::Select;
    AGizmoActor* GizmoActor = GetGizmoActor();
    if (GizmoActor)
    {
        CurrentGizmoMode = GizmoActor->GetMode();
    }

    // Select 버튼 (Q)
    bool bIsSelectActive = (CurrentGizmoMode == EGizmoMode::Select);
    ImVec4 SelectTintColor = bIsSelectActive ? ImVec4(0.3f, 0.6f, 1.0f, 1.0f) : ImVec4(1, 1, 1, 1);

    if (IconSelect && IconSelect->GetShaderResourceView())
    {
        if (ImGui::ImageButton("##SelectBtn", (void*)IconSelect->GetShaderResourceView(), IconSize,
            ImVec2(0, 0), ImVec2(1, 1), ImVec4(0, 0, 0, 0), SelectTintColor))
        {
            if (GizmoActor)
            {
                GizmoActor->SetMode(EGizmoMode::Select);
            }
        }
    }
    else
    {
        if (ImGui::Button("Select", ImVec2(60, 0)))
        {
            if (GizmoActor)
            {
                GizmoActor->SetMode(EGizmoMode::Select);
            }
        }
    }

    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("오브젝트를 선택합니다. [Q]");
    }
    ImGui::SameLine();

    // Move 버튼 (W)
    bool bIsMoveActive = (CurrentGizmoMode == EGizmoMode::Translate);
    ImVec4 MoveTintColor = bIsMoveActive ? ImVec4(0.3f, 0.6f, 1.0f, 1.0f) : ImVec4(1, 1, 1, 1);

    if (IconMove && IconMove->GetShaderResourceView())
    {
        if (ImGui::ImageButton("##MoveBtn", (void*)IconMove->GetShaderResourceView(), IconSize,
            ImVec2(0, 0), ImVec2(1, 1), ImVec4(0, 0, 0, 0), MoveTintColor))
        {
            if (GizmoActor)
            {
                GizmoActor->SetMode(EGizmoMode::Translate);
            }
        }
    }
    else
    {
        if (ImGui::Button("Move", ImVec2(60, 0)))
        {
            if (GizmoActor)
            {
                GizmoActor->SetMode(EGizmoMode::Translate);
            }
        }
    }

    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("오브젝트를 선택하고 이동시킵니다. [W]");
    }
    ImGui::SameLine();

    // Rotate 버튼 (E)
    bool bIsRotateActive = (CurrentGizmoMode == EGizmoMode::Rotate);
    ImVec4 RotateTintColor = bIsRotateActive ? ImVec4(0.3f, 0.6f, 1.0f, 1.0f) : ImVec4(1, 1, 1, 1);

    if (IconRotate && IconRotate->GetShaderResourceView())
    {
        if (ImGui::ImageButton("##RotateBtn", (void*)IconRotate->GetShaderResourceView(), IconSize,
            ImVec2(0, 0), ImVec2(1, 1), ImVec4(0, 0, 0, 0), RotateTintColor))
        {
            if (GizmoActor)
            {
                GizmoActor->SetMode(EGizmoMode::Rotate);
            }
        }
    }
    else
    {
        if (ImGui::Button("Rotate", ImVec2(60, 0)))
        {
            if (GizmoActor)
            {
                GizmoActor->SetMode(EGizmoMode::Rotate);
            }
        }
    }

    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("오브젝트를 선택하고 회전시킵니다. [E]");
    }
    ImGui::SameLine();

    // Scale 버튼 (R)
    bool bIsScaleActive = (CurrentGizmoMode == EGizmoMode::Scale);
    ImVec4 ScaleTintColor = bIsScaleActive ? ImVec4(0.3f, 0.6f, 1.0f, 1.0f) : ImVec4(1, 1, 1, 1);

    if (IconScale && IconScale->GetShaderResourceView())
    {
        if (ImGui::ImageButton("##ScaleBtn", (void*)IconScale->GetShaderResourceView(), IconSize,
            ImVec2(0, 0), ImVec2(1, 1), ImVec4(0, 0, 0, 0), ScaleTintColor))
        {
            if (GizmoActor)
            {
                GizmoActor->SetMode(EGizmoMode::Scale);
            }
        }
    }
    else
    {
        if (ImGui::Button("Scale", ImVec2(60, 0)))
        {
            if (GizmoActor)
            {
                GizmoActor->SetMode(EGizmoMode::Scale);
            }
        }
    }

    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("오브젝트를 선택하고 스케일을 조절합니다. [R]");
    }

    ImGui::SameLine();
}

void SViewerWindow::RenderViewerGizmoSpaceButton()
{
    const ImVec2 IconSize(17, 17);

    // GizmoActor에서 직접 현재 스페이스 가져오기
    EGizmoSpace CurrentGizmoSpace = EGizmoSpace::World;
    AGizmoActor* GizmoActor = GetGizmoActor();
    if (GizmoActor)
    {
        CurrentGizmoSpace = GizmoActor->GetSpace();
    }

    // 현재 스페이스에 따라 적절한 아이콘 표시
    bool bIsWorldSpace = (CurrentGizmoSpace == EGizmoSpace::World);
    UTexture* CurrentIcon = bIsWorldSpace ? IconWorldSpace : IconLocalSpace;
    const char* TooltipText = bIsWorldSpace ? "월드 스페이스 좌표 [Tab]" : "로컬 스페이스 좌표 [Tab]";

    // 선택 상태 tint (월드/로컬 모두 동일하게 흰색)
    ImVec4 TintColor = ImVec4(1, 1, 1, 1);

    if (CurrentIcon && CurrentIcon->GetShaderResourceView())
    {
        if (ImGui::ImageButton("##GizmoSpaceBtn", (void*)CurrentIcon->GetShaderResourceView(), IconSize,
            ImVec2(0, 0), ImVec2(1, 1), ImVec4(0, 0, 0, 0), TintColor))
        {
            // 버튼 클릭 시 스페이스 전환
            if (GizmoActor)
            {
                EGizmoSpace NewSpace = bIsWorldSpace ? EGizmoSpace::Local : EGizmoSpace::World;
                GizmoActor->SetSpace(NewSpace);
            }
        }
    }
    else
    {
        // 아이콘이 없는 경우 텍스트 버튼
        const char* ButtonText = bIsWorldSpace ? "World" : "Local";
        if (ImGui::Button(ButtonText, ImVec2(60, 0)))
        {
            if (GizmoActor)
            {
                EGizmoSpace NewSpace = bIsWorldSpace ? EGizmoSpace::Local : EGizmoSpace::World;
                GizmoActor->SetSpace(NewSpace);
            }
        }
    }

    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("%s", TooltipText);
    }

    ImGui::SameLine();
}

void SViewerWindow::RenderViewerToolbar()
{
    // 툴바 높이
    const float ToolbarHeight = 32.0f;

    // 툴바 영역 시작 (탭 키 네비게이션 완전 비활성화)
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.05f, 0.05f, 0.05f, 1.0f));
    ImGui::BeginChild("ViewerToolbar", ImVec2(0, ToolbarHeight), false,
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoNavInputs | ImGuiWindowFlags_NoNavFocus);

    // 세로 중앙 정렬
    float BtnHeight = ImGui::GetFrameHeight();
    float CenterY = (ToolbarHeight - BtnHeight) * 0.5f + 1.0f;
    ImGui::SetCursorPosY(CenterY);

    // 왼쪽 패딩
    const float SidePadding = 8.0f;
    ImGui::Dummy(ImVec2(SidePadding, 0));
    ImGui::SameLine();

    // 기즈모 버튼 스타일 설정 (메인 뷰와 동일)
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(2, 0));      // 간격 좁히기
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);            // 모서리 둥글게
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));        // 배경 투명
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.2f, 0.2f, 0.2f, 0.5f)); // 호버 배경

    // 기즈모 모드 버튼 (Q/W/E/R)
    RenderViewerGizmoButtons();

    // 구분선
    ImGui::TextColored(ImVec4(0.4f, 0.4f, 0.4f, 1.0f), "|");
    ImGui::SameLine();

    // 월드/로컬 스페이스 토글
    RenderViewerGizmoSpaceButton();

    // 기즈모 버튼 스타일 복원
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(2);

    // === 우측 정렬 버튼들 ===
    // 사용 가능한 전체 너비와 현재 커서 위치
    float AvailableWidth = ImGui::GetContentRegionAvail().x;
    float CursorStartX = ImGui::GetCursorPosX();
    ImVec2 CurrentCursor = ImGui::GetCursorPos();

    const float ButtonSpacing = 8.0f;

    // 현재 뷰모드 이름으로 실제 너비 계산
    const char* CurrentViewModeName = "뷰모드";
    if (ActiveState && ActiveState->Client)
    {
        EViewMode CurrentViewMode = ActiveState->Client->GetViewMode();
        switch (CurrentViewMode)
        {
        case EViewMode::VMI_Lit_Gouraud:
        case EViewMode::VMI_Lit_Lambert:
        case EViewMode::VMI_Lit_Phong:
            CurrentViewModeName = "라이팅 포함";
            break;
        case EViewMode::VMI_Unlit:
            CurrentViewModeName = "언릿";
            break;
        case EViewMode::VMI_Wireframe:
            CurrentViewModeName = "와이어프레임";
            break;
        case EViewMode::VMI_WorldNormal:
            CurrentViewModeName = "월드 노멀";
            break;
        case EViewMode::VMI_SceneDepth:
            CurrentViewModeName = "씬 뎁스";
            break;
        }
    }

    // ViewMode 버튼의 실제 너비 계산
    char viewModeText[64];
    sprintf_s(viewModeText, "%s %s", CurrentViewModeName, "∨");
    ImVec2 viewModeTextSize = ImGui::CalcTextSize(viewModeText);
    const float ViewModeButtonWidth = 17.0f + 4.0f + viewModeTextSize.x + 16.0f;

    // Camera 버튼 너비 계산
    char cameraText[64];
    sprintf_s(cameraText, "%s %s", "원근", "∨");
    ImVec2 cameraTextSize = ImGui::CalcTextSize(cameraText);
    const float CameraButtonWidth = 8.0f + 4.0f + cameraTextSize.x + 16.0f;

    // 오른쪽부터 역순으로 위치 계산
    // ViewMode는 오른쪽 끝
    float ViewModeX = CursorStartX + AvailableWidth - ViewModeButtonWidth - SidePadding;
    float CameraX = ViewModeX - ButtonSpacing - CameraButtonWidth;

    // 버튼들을 순서대로 그리기
    ImGui::SetCursorPos(ImVec2(CameraX, CurrentCursor.y));
    RenderCameraOptionDropdownMenu();

    ImGui::SetCursorPos(ImVec2(ViewModeX, CurrentCursor.y));
    RenderViewModeDropdownMenu();

    ImGui::EndChild();
    ImGui::PopStyleColor(); // ChildBg
}

void SViewerWindow::RenderCameraOptionDropdownMenu()
{
    if (!ActiveState || !ActiveState->Client) return;

    ImVec2 cursorPos = ImGui::GetCursorPos();
    ImGui::SetCursorPosY(cursorPos.y - 0.7f);

    const ImVec2 IconSize(16, 16);

    // 현재 뷰포트 타입 가져오기
    EViewportType ViewportType = ActiveState->Client->GetViewportType();

    // 뷰포트 타입에 따른 이름 설정
    const char* ViewportName = "원근";
    switch (ViewportType)
    {
    case EViewportType::Perspective:
        ViewportName = "원근";
        break;
    case EViewportType::Orthographic_Top:
        ViewportName = "상단";
        break;
    case EViewportType::Orthographic_Bottom:
        ViewportName = "하단";
        break;
    case EViewportType::Orthographic_Left:
        ViewportName = "왼쪽";
        break;
    case EViewportType::Orthographic_Right:
        ViewportName = "오른쪽";
        break;
    case EViewportType::Orthographic_Front:
        ViewportName = "정면";
        break;
    case EViewportType::Orthographic_Back:
        ViewportName = "후면";
        break;
    }

    // 드롭다운 버튼 텍스트 준비
    char ButtonText[64];
    sprintf_s(ButtonText, "%s %s", ViewportName, "∨");

    // 버튼 너비 계산 (아이콘 크기 + 간격 + 텍스트 크기 + 좌우 패딩)
    ImVec2 TextSize = ImGui::CalcTextSize(ButtonText);
    const float HorizontalPadding = 8.0f;
    const float CameraDropdownWidth = IconSize.x + 4.0f + TextSize.x + HorizontalPadding * 2.0f;

    // 드롭다운 버튼 스타일 적용
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.15f, 0.15f, 1.00f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.20f, 0.20f, 0.20f, 1.00f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.25f, 0.25f, 0.25f, 1.00f));

    // 드롭다운 버튼 생성 (카메라 아이콘 + 현재 모드명 + 화살표)
    ImVec2 ButtonSize(CameraDropdownWidth, ImGui::GetFrameHeight());
    ImVec2 ButtonCursorPos = ImGui::GetCursorPos();

    // 고유 ID 생성
    char ButtonID[64];
    sprintf_s(ButtonID, "##ViewerCameraBtn_%p", this);

    // 버튼 클릭 영역
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(ButtonCursorPos,
        ImVec2(ButtonCursorPos.x + ButtonSize.x, ButtonCursorPos.y + ButtonSize.y),
        IM_COL32(135, 135, 135, 255), 4.0f);
    ImGui::SetCursorPos(ButtonCursorPos);
    if (ImGui::InvisibleButton(ButtonID, ButtonSize))
    {
        char PopupID[64];
        sprintf_s(PopupID, "ViewerCameraPopup_%p", this);
        ImGui::OpenPopup(PopupID);
    }

    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("카메라 옵션");
    }

    // 버튼 위에 내용 렌더링 (아이콘 + 텍스트, 가운데 정렬)
    float ButtonContentWidth = IconSize.x + 4.0f + TextSize.x;
    float ButtonContentStartX = ButtonCursorPos.x + (ButtonSize.x - ButtonContentWidth) * 0.5f;
    ImVec2 ButtonContentCursorPos = ImVec2(ButtonContentStartX, ButtonCursorPos.y + (ButtonSize.y - IconSize.y) * 0.5f);
    ImGui::SetCursorPos(ButtonContentCursorPos);

    // 현재 뷰포트 모드에 따라 아이콘 선택
    UTexture* CurrentModeIcon = nullptr;
    switch (ViewportType)
    {
    case EViewportType::Perspective:
        CurrentModeIcon = IconCamera;
        break;
    case EViewportType::Orthographic_Top:
        CurrentModeIcon = IconTop;
        break;
    case EViewportType::Orthographic_Bottom:
        CurrentModeIcon = IconBottom;
        break;
    case EViewportType::Orthographic_Left:
        CurrentModeIcon = IconLeft;
        break;
    case EViewportType::Orthographic_Right:
        CurrentModeIcon = IconRight;
        break;
    case EViewportType::Orthographic_Front:
        CurrentModeIcon = IconFront;
        break;
    case EViewportType::Orthographic_Back:
        CurrentModeIcon = IconBack;
        break;
    default:
        CurrentModeIcon = IconCamera;
        break;
    }

    if (CurrentModeIcon && CurrentModeIcon->GetShaderResourceView())
    {
        ImGui::Image((void*)CurrentModeIcon->GetShaderResourceView(), IconSize);
        ImGui::SameLine(0, 4);
    }

    ImGui::SetWindowFontScale(0.96f);
    ImGui::Text("%s", ButtonText);
    ImGui::SetWindowFontScale(1.0f);
    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar(1);

    // ===== 뷰포트 모드 드롭다운 팝업 =====
    char PopupID[64];
    sprintf_s(PopupID, "ViewerCameraPopup_%p", this);
    if (ImGui::BeginPopup(PopupID, ImGuiWindowFlags_NoMove))
    {
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 4));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));

        // 선택된 항목의 파란 배경 제거
        ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.2f, 0.2f, 0.2f, 0.5f));
        ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.3f, 0.3f, 0.3f, 0.6f));

        // --- 섹션 1: 원근 ---
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "원근");
        ImGui::Separator();

        bool bIsPerspective = (ViewportType == EViewportType::Perspective);
        const char* RadioIcon = bIsPerspective ? "●" : "○";

        // 원근 모드 선택 항목 (라디오 버튼 + 아이콘 + 텍스트 통합)
        ImVec2 SelectableSize(180, 20);
        ImVec2 SelectableCursorPos = ImGui::GetCursorPos();

        if (ImGui::Selectable("##Perspective", bIsPerspective, 0, SelectableSize))
        {
            ActiveState->Client->SetViewportType(EViewportType::Perspective);
            ActiveState->Client->SetupCameraMode();
            ImGui::CloseCurrentPopup();
        }

        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("뷰포트를 원근 보기로 전환합니다.");
        }

        // Selectable 위에 내용 렌더링
        ImVec2 ContentPos = ImVec2(SelectableCursorPos.x + 4, SelectableCursorPos.y + (SelectableSize.y - IconSize.y) * 0.5f);
        ImGui::SetCursorPos(ContentPos);

        ImGui::Text("%s", RadioIcon);
        ImGui::SameLine(0, 4);

        if (IconPerspective && IconPerspective->GetShaderResourceView())
        {
            ImGui::Image((void*)IconPerspective->GetShaderResourceView(), IconSize);
            ImGui::SameLine(0, 4);
        }

        ImGui::Text("원근");

        // --- 섹션 2: 직교 ---
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "직교");
        ImGui::Separator();

        // 직교 모드 목록
        struct ViewportModeEntry {
            EViewportType type;
            const char* koreanName;
            UTexture** icon;
            const char* tooltip;
        };

        ViewportModeEntry orthographicModes[] = {
            { EViewportType::Orthographic_Top, "상단", &IconTop, "뷰포트를 상단 보기로 전환합니다." },
            { EViewportType::Orthographic_Bottom, "하단", &IconBottom, "뷰포트를 하단 보기로 전환합니다." },
            { EViewportType::Orthographic_Left, "왼쪽", &IconLeft, "뷰포트를 왼쪽 보기로 전환합니다." },
            { EViewportType::Orthographic_Right, "오른쪽", &IconRight, "뷰포트를 오른쪽 보기로 전환합니다." },
            { EViewportType::Orthographic_Front, "정면", &IconFront, "뷰포트를 정면 보기로 전환합니다." },
            { EViewportType::Orthographic_Back, "후면", &IconBack, "뷰포트를 후면 보기로 전환합니다." }
        };

        for (int i = 0; i < 6; i++)
        {
            const auto& mode = orthographicModes[i];
            bool bIsSelected = (ViewportType == mode.type);
            const char* RadioIcon = bIsSelected ? "●" : "○";

            // 직교 모드 선택 항목 (라디오 버튼 + 아이콘 + 텍스트 통합)
            char SelectableID[32];
            sprintf_s(SelectableID, "##Ortho%d", i);

            ImVec2 OrthoSelectableCursorPos = ImGui::GetCursorPos();

            if (ImGui::Selectable(SelectableID, bIsSelected, 0, SelectableSize))
            {
                ActiveState->Client->SetViewportType(mode.type);
                ActiveState->Client->SetupCameraMode();
                ImGui::CloseCurrentPopup();
            }

            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("%s", mode.tooltip);
            }

            // Selectable 위에 내용 렌더링
            ImVec2 OrthoContentPos = ImVec2(OrthoSelectableCursorPos.x + 4, OrthoSelectableCursorPos.y + (SelectableSize.y - IconSize.y) * 0.5f);
            ImGui::SetCursorPos(OrthoContentPos);

            ImGui::Text("%s", RadioIcon);
            ImGui::SameLine(0, 4);

            if (*mode.icon && (*mode.icon)->GetShaderResourceView())
            {
                ImGui::Image((void*)(*mode.icon)->GetShaderResourceView(), IconSize);
                ImGui::SameLine(0, 4);
            }

            ImGui::Text("%s", mode.koreanName);
        }

        // --- 섹션 3: 이동 ---
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "이동");
        ImGui::Separator();

        ACameraActor* Camera = ActiveState->Client ? ActiveState->Client->GetCamera() : nullptr;
        if (Camera)
        {
            if (IconSpeed && IconSpeed->GetShaderResourceView())
            {
                ImGui::Image((void*)IconSpeed->GetShaderResourceView(), IconSize);
                ImGui::SameLine();
            }
            ImGui::Text("카메라 이동 속도");

            float speed = Camera->GetCameraSpeed();
            ImGui::SetNextItemWidth(180);
            if (ImGui::SliderFloat("##CameraSpeed", &speed, 1.0f, 100.0f, "%.1f"))
            {
                Camera->SetCameraSpeed(speed);
            }
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("WASD 키로 카메라를 이동할 때의 속도 (1-100)");
            }
        }

        // --- 섹션 4: 뷰 ---
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "뷰");
        ImGui::Separator();

        if (Camera && Camera->GetCameraComponent())
        {
            UCameraComponent* camComp = Camera->GetCameraComponent();

            // FOV
            if (IconFOV && IconFOV->GetShaderResourceView())
            {
                ImGui::Image((void*)IconFOV->GetShaderResourceView(), IconSize);
                ImGui::SameLine();
            }
            ImGui::Text("필드 오브 뷰");

            float fov = camComp->GetFOV();
            ImGui::SetNextItemWidth(180);
            if (ImGui::SliderFloat("##FOV", &fov, 30.0f, 120.0f, "%.1f"))
            {
                camComp->SetFOV(fov);
            }
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("카메라 시야각 (30-120도)\n값이 클수록 넓은 범위가 보입니다");
            }

            // 근평면
            if (IconNearClip && IconNearClip->GetShaderResourceView())
            {
                ImGui::Image((void*)IconNearClip->GetShaderResourceView(), IconSize);
                ImGui::SameLine();
            }
            ImGui::Text("근평면");

            float nearClip = camComp->GetNearClip();
            ImGui::SetNextItemWidth(180);
            if (ImGui::SliderFloat("##NearClip", &nearClip, 0.01f, 10.0f, "%.2f"))
            {
                camComp->SetClipPlanes(nearClip, camComp->GetFarClip());
            }
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("카메라에서 가장 가까운 렌더링 거리 (0.01-10)\n이 값보다 가까운 오브젝트는 보이지 않습니다");
            }

            // 원평면
            if (IconFarClip && IconFarClip->GetShaderResourceView())
            {
                ImGui::Image((void*)IconFarClip->GetShaderResourceView(), IconSize);
                ImGui::SameLine();
            }
            ImGui::Text("원평면");

            float farClip = camComp->GetFarClip();
            ImGui::SetNextItemWidth(180);
            if (ImGui::SliderFloat("##FarClip", &farClip, 10.0f, 10000.0f, "%.0f"))
            {
                camComp->SetClipPlanes(camComp->GetNearClip(), farClip);
            }
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("카메라에서 가장 먼 렌더링 거리 (10-10000)\n이 값보다 먼 오브젝트는 보이지 않습니다");
            }
        }

        ImGui::PopStyleColor(3);
        ImGui::PopStyleVar(2);
        ImGui::EndPopup();
    }
}

void SViewerWindow::RenderViewModeDropdownMenu()
{
    if (!ActiveState || !ActiveState->Client) return;

    ImVec2 cursorPos = ImGui::GetCursorPos();
    ImGui::SetCursorPosY(cursorPos.y - 1.0f);

    const ImVec2 IconSize(15, 15);

    // 현재 뷰모드 이름 및 아이콘 가져오기
    EViewMode CurrentViewMode = ActiveState->Client->GetViewMode();
    const char* CurrentViewModeName = "뷰모드";
    UTexture* CurrentViewModeIcon = nullptr;

    switch (CurrentViewMode)
    {
    case EViewMode::VMI_Lit_Gouraud:
    case EViewMode::VMI_Lit_Lambert:
    case EViewMode::VMI_Lit_Phong:
        CurrentViewModeName = "라이팅 포함";
        CurrentViewModeIcon = IconViewMode_Lit;
        break;
    case EViewMode::VMI_Unlit:
        CurrentViewModeName = "언릿";
        CurrentViewModeIcon = IconViewMode_Unlit;
        break;
    case EViewMode::VMI_Wireframe:
        CurrentViewModeName = "와이어프레임";
        CurrentViewModeIcon = IconViewMode_Wireframe;
        break;
    case EViewMode::VMI_WorldNormal:
        CurrentViewModeName = "월드 노멀";
        CurrentViewModeIcon = IconViewMode_BufferVis;
        break;
    case EViewMode::VMI_SceneDepth:
        CurrentViewModeName = "씬 뎁스";
        CurrentViewModeIcon = IconViewMode_BufferVis;
        break;
    }

    // 드롭다운 버튼 텍스트 준비
    char ButtonText[64];
    sprintf_s(ButtonText, "%s %s", CurrentViewModeName, "∨");

    // 버튼 너비 계산 (아이콘 크기 + 간격 + 텍스트 크기 + 좌우 패딩)
    ImVec2 TextSize = ImGui::CalcTextSize(ButtonText);
    const float Padding = 8.0f;
    const float DropdownWidth = IconSize.x + 4.0f + TextSize.x + Padding * 2.0f;

    // 스타일 적용
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.15f, 0.15f, 1.00f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.20f, 0.20f, 0.20f, 1.00f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.25f, 0.25f, 0.25f, 1.00f));

    // 드롭다운 버튼 생성 (아이콘 + 텍스트)
    ImVec2 ButtonSize(DropdownWidth, ImGui::GetFrameHeight());
    ImVec2 ButtonCursorPos = ImGui::GetCursorPos();

    // 고유 ID 생성
    char ButtonID[64];
    sprintf_s(ButtonID, "##ViewerViewModeBtn_%p", this);

    // 버튼 클릭 영역
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(ButtonCursorPos, 
        ImVec2(ButtonCursorPos.x + ButtonSize.x, ButtonCursorPos.y + ButtonSize.y), 
        IM_COL32(135, 135, 135, 255), 4.0f);
    ImGui::SetCursorPos(ButtonCursorPos);
    if (ImGui::InvisibleButton(ButtonID, ButtonSize))
    {
        char PopupID[64];
        sprintf_s(PopupID, "ViewerViewModePopup_%p", this);
        ImGui::OpenPopup(PopupID);
    }

    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("뷰모드 선택");
    }

    // 버튼 위에 내용 렌더링 (아이콘 + 텍스트, 가운데 정렬)
    float ButtonContentWidth = IconSize.x + 4.0f + TextSize.x;
    float ButtonContentStartX = ButtonCursorPos.x + (ButtonSize.x - ButtonContentWidth) * 0.5f;
    ImVec2 ButtonContentCursorPos = ImVec2(ButtonContentStartX, ButtonCursorPos.y + (ButtonSize.y - IconSize.y) * 0.5f);
    ImGui::SetCursorPos(ButtonContentCursorPos);

    // 현재 뷰모드 아이콘 표시
    if (CurrentViewModeIcon && CurrentViewModeIcon->GetShaderResourceView())
    {
        ImGui::Image((void*)CurrentViewModeIcon->GetShaderResourceView(), IconSize);
        ImGui::SameLine(0, 4);
    }
    ImGui::SetWindowFontScale(0.96f);
    ImGui::Text("%s", ButtonText);
    ImGui::SetWindowFontScale(1.0f);
    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar(1);

    // ===== 뷰모드 드롭다운 팝업 =====
    char PopupID[64];
    sprintf_s(PopupID, "ViewerViewModePopup_%p", this);
    if (ImGui::BeginPopup(PopupID, ImGuiWindowFlags_NoMove))
    {
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 4));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));

        // 선택된 항목의 파란 배경 제거
        ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.2f, 0.2f, 0.2f, 0.5f));
        ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.3f, 0.3f, 0.3f, 0.6f));

        // --- 섹션: 뷰모드 ---
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "뷰모드");
        ImGui::Separator();

        // ===== Lit 메뉴 (서브메뉴 포함) =====
        bool bIsLitMode = (CurrentViewMode == EViewMode::VMI_Lit_Phong ||
            CurrentViewMode == EViewMode::VMI_Lit_Gouraud ||
            CurrentViewMode == EViewMode::VMI_Lit_Lambert);

        const char* LitRadioIcon = bIsLitMode ? "●" : "○";

        // Selectable로 감싸서 전체 호버링 영역 확보
        ImVec2 LitCursorPos = ImGui::GetCursorScreenPos();
        ImVec2 LitSelectableSize(180, IconSize.y);

        // 호버 감지용 투명 Selectable
        ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0, 0, 0, 0));
        bool bLitHovered = ImGui::Selectable("##LitHoverArea", false, ImGuiSelectableFlags_AllowItemOverlap, LitSelectableSize);
        ImGui::PopStyleColor();

        // Selectable 위에 내용 렌더링 (순서: ● + [텍스처] + 텍스트)
        ImGui::SetCursorScreenPos(LitCursorPos);

        ImGui::Text("%s", LitRadioIcon);
        ImGui::SameLine(0, 4);

        if (IconViewMode_Lit && IconViewMode_Lit->GetShaderResourceView())
        {
            ImGui::Image((void*)IconViewMode_Lit->GetShaderResourceView(), IconSize);
            ImGui::SameLine(0, 4);
        }

        if (ImGui::BeginMenu("라이팅포함"))
        {
            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "셰이딩 모델");
            ImGui::Separator();

            // PHONG
            bool bIsPhong = (CurrentViewMode == EViewMode::VMI_Lit_Phong);
            const char* PhongIcon = bIsPhong ? "●" : "○";
            char PhongLabel[32];
            sprintf_s(PhongLabel, "%s PHONG", PhongIcon);
            if (ImGui::MenuItem(PhongLabel))
            {
                ActiveState->Client->SetViewMode(EViewMode::VMI_Lit_Phong);
                ImGui::CloseCurrentPopup();
            }
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("픽셀 단위 셰이딩 (Per-Pixel)\n스페큘러 하이라이트 포함");
            }

            // GOURAUD
            bool bIsGouraud = (CurrentViewMode == EViewMode::VMI_Lit_Gouraud);
            const char* GouraudIcon = bIsGouraud ? "●" : "○";
            char GouraudLabel[32];
            sprintf_s(GouraudLabel, "%s GOURAUD", GouraudIcon);
            if (ImGui::MenuItem(GouraudLabel))
            {
                ActiveState->Client->SetViewMode(EViewMode::VMI_Lit_Gouraud);
                ImGui::CloseCurrentPopup();
            }
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("정점 단위 셰이딩 (Per-Vertex)\n부드러운 색상 보간");
            }

            // LAMBERT
            bool bIsLambert = (CurrentViewMode == EViewMode::VMI_Lit_Lambert);
            const char* LambertIcon = bIsLambert ? "●" : "○";
            char LambertLabel[32];
            sprintf_s(LambertLabel, "%s LAMBERT", LambertIcon);
            if (ImGui::MenuItem(LambertLabel))
            {
                ActiveState->Client->SetViewMode(EViewMode::VMI_Lit_Lambert);
                ImGui::CloseCurrentPopup();
            }
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("Diffuse만 계산하는 간단한 셰이딩\n스페큘러 하이라이트 없음");
            }

            ImGui::EndMenu();
        }

        // ===== Unlit 메뉴 =====
        bool bIsUnlit = (CurrentViewMode == EViewMode::VMI_Unlit);
        const char* UnlitRadioIcon = bIsUnlit ? "●" : "○";

        // Selectable로 감싸서 전체 호버링 영역 확보
        ImVec2 UnlitCursorPos = ImGui::GetCursorScreenPos();
        ImVec2 UnlitSelectableSize(180, IconSize.y);

        if (ImGui::Selectable("##UnlitSelectableArea", false, 0, UnlitSelectableSize))
        {
            ActiveState->Client->SetViewMode(EViewMode::VMI_Unlit);
            ImGui::CloseCurrentPopup();
        }

        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("조명 계산 없이 텍스처와 색상만 표시");
        }

        // Selectable 위에 내용 렌더링 (순서: ● + [텍스처] + 텍스트)
        ImGui::SetCursorScreenPos(UnlitCursorPos);

        ImGui::Text("%s", UnlitRadioIcon);
        ImGui::SameLine(0, 4);

        if (IconViewMode_Unlit && IconViewMode_Unlit->GetShaderResourceView())
        {
            ImGui::Image((void*)IconViewMode_Unlit->GetShaderResourceView(), IconSize);
            ImGui::SameLine(0, 4);
        }

        ImGui::Text("언릿");

        // ===== Wireframe 메뉴 =====
        bool bIsWireframe = (CurrentViewMode == EViewMode::VMI_Wireframe);
        const char* WireframeRadioIcon = bIsWireframe ? "●" : "○";

        // Selectable로 감싸서 전체 호버링 영역 확보
        ImVec2 WireframeCursorPos = ImGui::GetCursorScreenPos();
        ImVec2 WireframeSelectableSize(180, IconSize.y);

        if (ImGui::Selectable("##WireframeSelectableArea", false, 0, WireframeSelectableSize))
        {
            ActiveState->Client->SetViewMode(EViewMode::VMI_Wireframe);
            ImGui::CloseCurrentPopup();
        }

        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("메시의 외곽선(에지)만 표시");
        }

        // Selectable 위에 내용 렌더링 (순서: ● + [텍스처] + 텍스트)
        ImGui::SetCursorScreenPos(WireframeCursorPos);

        ImGui::Text("%s", WireframeRadioIcon);
        ImGui::SameLine(0, 4);

        if (IconViewMode_Wireframe && IconViewMode_Wireframe->GetShaderResourceView())
        {
            ImGui::Image((void*)IconViewMode_Wireframe->GetShaderResourceView(), IconSize);
            ImGui::SameLine(0, 4);
        }

        ImGui::Text("와이어프레임");

        // ===== Buffer Visualization 메뉴 (서브메뉴 포함) =====
        bool bIsBufferVis = (CurrentViewMode == EViewMode::VMI_WorldNormal ||
            CurrentViewMode == EViewMode::VMI_SceneDepth);

        const char* BufferVisRadioIcon = bIsBufferVis ? "●" : "○";

        // Selectable로 감싸서 전체 호버링 영역 확보
        ImVec2 BufferVisCursorPos = ImGui::GetCursorScreenPos();
        ImVec2 BufferVisSelectableSize(180, IconSize.y);

        // 호버 감지용 투명 Selectable
        ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0, 0, 0, 0));
        bool bBufferVisHovered = ImGui::Selectable("##BufferVisHoverArea", false, ImGuiSelectableFlags_AllowItemOverlap, BufferVisSelectableSize);
        ImGui::PopStyleColor();

        // Selectable 위에 내용 렌더링 (순서: ● + [텍스처] + 텍스트)
        ImGui::SetCursorScreenPos(BufferVisCursorPos);

        ImGui::Text("%s", BufferVisRadioIcon);
        ImGui::SameLine(0, 4);

        if (IconViewMode_BufferVis && IconViewMode_BufferVis->GetShaderResourceView())
        {
            ImGui::Image((void*)IconViewMode_BufferVis->GetShaderResourceView(), IconSize);
            ImGui::SameLine(0, 4);
        }

        if (ImGui::BeginMenu("버퍼 시각화"))
        {
            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "버퍼 시각화");
            ImGui::Separator();

            // Scene Depth
            bool bIsSceneDepth = (CurrentViewMode == EViewMode::VMI_SceneDepth);
            const char* SceneDepthIcon = bIsSceneDepth ? "●" : "○";
            char SceneDepthLabel[32];
            sprintf_s(SceneDepthLabel, "%s 씬 뎁스", SceneDepthIcon);
            if (ImGui::MenuItem(SceneDepthLabel))
            {
                ActiveState->Client->SetViewMode(EViewMode::VMI_SceneDepth);
                ImGui::CloseCurrentPopup();
            }
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("씬의 깊이 정보를 그레이스케일로 표시\n어두울수록 카메라에 가까움");
            }

            // World Normal
            bool bIsWorldNormal = (CurrentViewMode == EViewMode::VMI_WorldNormal);
            const char* WorldNormalIcon = bIsWorldNormal ? "●" : "○";
            char WorldNormalLabel[32];
            sprintf_s(WorldNormalLabel, "%s 월드 노멀", WorldNormalIcon);
            if (ImGui::MenuItem(WorldNormalLabel))
            {
                ActiveState->Client->SetViewMode(EViewMode::VMI_WorldNormal);
                ImGui::CloseCurrentPopup();
            }
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("월드 공간의 노멀 벡터를 RGB로 표시\nR=X, G=Y, B=Z 축 방향");
            }

            ImGui::EndMenu();
        }

        ImGui::PopStyleColor(3);
        ImGui::PopStyleVar(2);
        ImGui::EndPopup();
    }
}

void SViewerWindow::RenderAnimationBrowser(
    std::function<void(UAnimSequence*)> OnAnimationSelected,
    std::function<bool(UAnimSequence*)> IsAnimationSelected)
{
    if (!ActiveState)   return;

    ImGuiStyle& style = ImGui::GetStyle();

    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 6);
    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.30f, 0.30f, 0.30f, 0.8f));
    ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]);
    ImGui::Text("ANIMATION BROWSER");
    ImGui::PopFont();
    ImGui::PopStyleColor();
    ImGui::Dummy(ImVec2(0, 6));
    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(0.35f, 0.35f, 0.35f, 0.6f));
    ImGui::Separator();
    ImGui::PopStyleColor();
    ImGui::Dummy(ImVec2(0, 6));
    ImGui::Spacing();
    
    // Checkbox Style
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.5, 0.5));
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.23f, 0.25f, 0.27f, 0.80f)); // #3A3F45 계열
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.28f, 0.30f, 0.33f, 0.90f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.20f, 0.22f, 0.25f, 1.00f));
    ImGui::PushStyleColor(ImGuiCol_CheckMark, ImVec4(0.75f, 0.80f, 0.90f, 1.00f));

    ImGui::PushItemWidth(-1.0f);
    ImGui::Checkbox("Show Only Compatible", &ActiveState->bShowOnlyCompatible);
    ImGui::PopItemWidth();
    ImGui::PopStyleColor(4);
    ImGui::PopStyleVar();

    ImGui::Dummy(ImVec2(0, 6));
    ImGui::Spacing();

    const TArray<UAnimSequence*>& AnimsToShow = ActiveState->bShowOnlyCompatible
        ? ActiveState->CompatibleAnimations
        : UResourceManager::GetInstance().GetAnimations();

    if (AnimsToShow.IsEmpty())
    {
        ImGui::Text(ActiveState->bShowOnlyCompatible
            ? "No compatible animations found."
            : "No animations loaded in ResourceManager.");
        return;
    }

    // Table row highlight colors
    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.2f, 0.2f, 0.2f, 0.5f)); // selected bg
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.4f, 0.4f, 0.4f, 0.2f)); // hover bg
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.2f, 0.2f, 0.2f, 0.5f)); // selected active bg

    // table row colors (transparent OK)
    ImGui::PushStyleColor(ImGuiCol_TableRowBg, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_TableRowBgAlt, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(8, 4));

    // Table flags (Sortable disabled)
    ImGuiTableFlags flags = ImGuiTableFlags_Borders
        | ImGuiTableFlags_RowBg
        | ImGuiTableFlags_Resizable
        | ImGuiTableFlags_ScrollY
        | ImGuiTableFlags_SizingStretchProp;

    if (ImGui::BeginTable("AnimBrowserTable", 2, flags, ImVec2(-1, -1)))
    {
        // Setup columns (NoSort 플래그 추가)
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_NoSort, 0.4f);
        ImGui::TableSetupColumn("Path", ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_NoSort, 0.6f);
        ImGui::TableSetupScrollFreeze(0, 1); // Freeze header row
        ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(8, 2)); // y padding 줄이기
        ImGui::TableHeadersRow();
        ImGui::PopStyleVar();

        // Table rows
        ImGuiListClipper clipper;
        clipper.Begin(AnimsToShow.size());

        while (clipper.Step())
        {
            for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; row++)
            {
                UAnimSequence* Anim = AnimsToShow[row];
                if (!Anim) continue;

                // Determine if this animation is selected
                bool isSelected = false;
                if (IsAnimationSelected)
                {
                    // Use custom selection check if provided
                    isSelected = IsAnimationSelected(Anim);
                }
                else
                {
                    // Default: check if it's the currently playing animation
                    isSelected = (ActiveState->CurrentAnimation == Anim);
                }

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);

                // Selectable spans entire row
                ImGui::PushID(row);
                if (ImGui::Selectable(("##row" + std::to_string(row)).c_str(),
                    isSelected,
                    ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap))
                {
                    // If a custom callback is provided, use it
                    if (OnAnimationSelected)
                    {
                        OnAnimationSelected(Anim);
                    }
                    // Otherwise, default behavior: play the animation
                    else if (ActiveState->PreviewActor)
                    {
                        ActiveState->CurrentAnimation = Anim;
                        ActiveState->TotalTime = Anim->GetSequenceLength();
                        ActiveState->CurrentTime = 0.0f;
                        ActiveState->bIsPlaying = true;
                        USkeletalMeshComponent* MeshComp = ActiveState->PreviewActor->GetSkeletalMeshComponent();
                        if (MeshComp)
                        {
                            MeshComp->PlayAnimation(Anim, ActiveState->bIsLooping, ActiveState->PlaybackSpeed);
                        }
                    }
                }
                ImGui::PopID();

                // Name column
                ImGui::SameLine();
                ImGui::Text("%s", Anim->GetName().c_str());

                // Path column
                ImGui::TableSetColumnIndex(1);
                ImGui::TextDisabled("%s", Anim->GetFilePath().c_str());
            }
        }

        ImGui::EndTable();
    }

    ImGui::PopStyleVar();
    ImGui::PopStyleColor(5);
}