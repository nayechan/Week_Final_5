#include "pch.h"
#include "SlateManager.h"
#include "SAnimationViewerWindow.h"
#include "Source/Runtime/Engine/Viewer/AnimationViewerBootstrap.h"
#include "Source/Runtime/Engine/GameFramework/SkeletalMeshActor.h"
#include "Source/Runtime/Engine/Viewer/EditorAssetPreviewContext.h"
#include "AnimSingleNodeInstance.h"
#include "Source/Runtime/Renderer/FViewport.h"
#include "Source/Runtime/Engine/Components/AudioComponent.h"

SAnimationViewerWindow::SAnimationViewerWindow()
{
    CenterRect = FRect(0, 0, 0, 0);

    LeftPanelRatio = 0.2f;   // 25% of width
    RightPanelRatio = 0.2f;  // 25% of width
}

SAnimationViewerWindow::~SAnimationViewerWindow()
{
    // Clean up tabs if any
    for (int i = 0; i < Tabs.Num(); ++i)
    {
        ViewerState* State = Tabs[i];
        AnimationViewerBootstrap::DestroyViewerState(State);
    }
    Tabs.Empty();
    ActiveState = nullptr;
}

void SAnimationViewerWindow::OnRender()
{
    // If window is closed, request cleanup and don't render
    if (!bIsOpen)
    {
        USlateManager::GetInstance().RequestCloseDetachedWindow(this);
        return;
    }

    // Parent detachable window (movable, top-level) with solid background
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoSavedSettings;

    // Initial placement (first frame only)
    if (!bInitialPlacementDone)
    {
        ImGui::SetNextWindowPos(ImVec2(Rect.Left, Rect.Top));
        ImGui::SetNextWindowSize(ImVec2(Rect.GetWidth(), Rect.GetHeight()));
        bInitialPlacementDone = true;
    }
    // Request focus for this frame
    if (bRequestFocus)
    {
        ImGui::SetNextWindowFocus();
    }

    // Generate a unique window title to pass to ImGui
    // Format: "Skeletal Mesh Viewer - MyAsset.fbx###0x12345678"
    char UniqueTitle[256];
    sprintf_s(UniqueTitle, sizeof(UniqueTitle), "%s###%p", WindowTitle.c_str(), this);

    bool bViewerVisible = false;
    if (ImGui::Begin(UniqueTitle, &bIsOpen, flags))
    {
        bViewerVisible = true;

        // 입력 라우팅을 위한 hover/focus 상태 캡처
        bIsWindowHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows);
        bIsWindowFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);

        //=====================================================
        // Tab Bar : Render tab bar and switch active state
        //=====================================================
        /*if (!ImGui::BeginTabBar("AnimationViewerTabs",
            ImGuiTabBarFlags_AutoSelectNewTabs | ImGuiTabBarFlags_Reorderable))
            return;*/
        RenderTabsAndToolbar(EViewerType::Animation);

        // 마지막 탭을 닫은 경우 렌더링 중단
        if (!bIsOpen)
        {
            USlateManager::GetInstance().RequestCloseDetachedWindow(this);
            ImGui::End();
            return;
        }

        //===============================
        // Update window rect
        //===============================
        ImVec2 pos = ImGui::GetWindowPos();
        ImVec2 size = ImGui::GetWindowSize();
        Rect.Left = pos.x; Rect.Top = pos.y; Rect.Right = pos.x + size.x; Rect.Bottom = pos.y + size.y;
        Rect.UpdateMinMax();

        ImVec2 contentAvail = ImGui::GetContentRegionAvail();
        float totalWidth = contentAvail.x;
        float totalHeight = contentAvail.y;

        float splitterWidth = 4.0f; // 분할선 두께

        float leftWidth = totalWidth * LeftPanelRatio;
        float rightWidth = totalWidth * RightPanelRatio;
        float centerWidth = totalWidth - leftWidth - rightWidth - (splitterWidth * 2);

        // 중앙 패널이 음수가 되지 않도록 보정 (안전장치)
        if (centerWidth < 0.0f)
        {
            centerWidth = 0.0f;
        }

        //======================================
        // Top panels (Left / Center / Right)
        //======================================

        // Remove spacing between panels
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));

        // +-+-+ Left panel +-+-+
        // : Asset Browser & Bone Hierarchy
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
        ImGui::BeginChild("LeftPanel", ImVec2(leftWidth, totalHeight), true, ImGuiWindowFlags_NoScrollbar);
        ImGui::PopStyleVar();
        RenderLeftPanel(leftWidth);
        ImGui::EndChild();

        ImGui::SameLine(0, 0); // No spacing between panels

        // Left splitter (드래그 가능한 분할선)
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.3f, 0.3f, 0.5f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.4f, 0.4f, 0.4f, 0.7f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.5f, 0.5f, 0.5f, 0.9f));
        ImGui::Button("##LeftSplitter", ImVec2(splitterWidth, totalHeight));
        ImGui::PopStyleColor(3);

        if (ImGui::IsItemHovered())
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);

        if (ImGui::IsItemActive())
        {
            float delta = ImGui::GetIO().MouseDelta.x;
            if (delta != 0.0f)
            {
                float newLeftRatio = LeftPanelRatio + delta / totalWidth;
                // 좌측 패널 최소 10%, 우측 패널과 겹치지 않도록 제한
                float maxLeftRatio = 1.0f - RightPanelRatio - (splitterWidth * 2) / totalWidth;
                LeftPanelRatio = std::max(0.1f, std::min(newLeftRatio, maxLeftRatio));
            }
        }

        ImGui::SameLine(0, 0); // No spacing between panels

        // +-+-+ Center panel +-+-+ - 완전히 가려진 경우 렌더링하지 않음
        if (centerWidth > 0.0f)
        {
            ImGui::BeginChild("CenterPanel", ImVec2(centerWidth, totalHeight), false,
                ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoNavInputs | ImGuiWindowFlags_NoNavFocus);
            RenderCenterPanel();
            ImGui::EndChild();

            ImGui::SameLine(0, 0); // No spacing between panels
        }
        else
        {
            // 중앙 패널이 완전히 가려진 경우 뷰포트 영역 초기화
            CenterRect = FRect(0, 0, 0, 0);
            CenterRect.UpdateMinMax();
        }

        // Right splitter (드래그 가능한 분할선)
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.3f, 0.3f, 0.5f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.4f, 0.4f, 0.4f, 0.7f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.5f, 0.5f, 0.5f, 0.9f));
        ImGui::Button("##RightSplitter", ImVec2(splitterWidth, totalHeight));
        ImGui::PopStyleColor(3);

        if (ImGui::IsItemHovered())
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);

        if (ImGui::IsItemActive())
        {
            float delta = ImGui::GetIO().MouseDelta.x;
            if (delta != 0.0f)
            {
                float newRightRatio = RightPanelRatio - delta / totalWidth;
                // 우측 패널 최소 10%, 좌측 패널과 겹치지 않도록 제한
                float maxRightRatio = 1.0f - LeftPanelRatio - (splitterWidth * 2) / totalWidth;
                RightPanelRatio = std::max(0.1f, std::min(newRightRatio, maxRightRatio));
            }
        }

        ImGui::SameLine(0, 0); // No spacing between panels

        // +-+-+ Right panel +-+-+
        // : Bone Properties
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
        ImGui::BeginChild("RightPanel", ImVec2(rightWidth, totalHeight), true);
        ImGui::PopStyleVar();
        {
            float bonePropsHeight = ImGui::GetContentRegionAvail().y * 0.5f;
            float animBrowserHeight = ImGui::GetContentRegionAvail().y * 0.5f;

            ImGui::BeginChild("BonePropertiesArea", ImVec2(0, bonePropsHeight), false);
            RenderRightPanel();
            ImGui::EndChild();
            
            ImGui::BeginChild("AnimationBrowserArea", ImVec2(0, 0), false);
            RenderAnimationBrowser();
            ImGui::EndChild();
        }
        ImGui::EndChild();
        ImGui::PopStyleVar();
    }
    ImGui::End();

    // If collapsed or not visible, clear the center rect so we don't render a floating viewport
    if (!bViewerVisible)
    {
        CenterRect = FRect(0, 0, 0, 0);
        CenterRect.UpdateMinMax();
        bIsWindowHovered = false;
        bIsWindowFocused = false;
    }

    // If window was closed via X button, notify the manager to clean up
    if (!bIsOpen)
    {
        // Just request to close the window
        USlateManager::GetInstance().RequestCloseDetachedWindow(this);
    }

    bRequestFocus = false;
}

void SAnimationViewerWindow::OnUpdate(float DeltaSeconds)
{
    SViewerWindow::OnUpdate(DeltaSeconds);

    if (!ActiveState || !ActiveState->PreviewActor || !ActiveState->CurrentAnimation) return;

    USkeletalMeshComponent* MeshComp = ActiveState->PreviewActor->GetSkeletalMeshComponent();
    if (!MeshComp)  return;

    UAnimSingleNodeInstance* SingleAnimInstance = Cast<UAnimSingleNodeInstance>(MeshComp->GetAnimInstance());
    if (!SingleAnimInstance)    return;

    // Get current animation component's actual state
    bool bIsActuallyPlaying = MeshComp->IsPlayingAnimation();
    float CurrentAnimPosition = MeshComp->GetAnimationPosition();

    // Synchronize play/pause state
    if (ActiveState->bIsPlaying && !bIsActuallyPlaying)
    {
        MeshComp->PlayAnimation(ActiveState->CurrentAnimation, ActiveState->bIsLooping, ActiveState->PlaybackSpeed);
        MeshComp->SetAnimationPosition(ActiveState->CurrentTime);
    }
    else if (!ActiveState->bIsPlaying && bIsActuallyPlaying)
    {
        MeshComp->StopAnimation();
    }

    // Synchronize play rate and loop setting
    float FinalPlayRate = ActiveState->bReversePlay ? -ActiveState->PlaybackSpeed : ActiveState->PlaybackSpeed;
    SingleAnimInstance->SetPlayRate(FinalPlayRate);
    SingleAnimInstance->SetLooping(ActiveState->bIsLooping);

    // Control time slider (UI to Engine)
    // Update the component's animation position only when the user is dragging the slider
    if (ActiveState->bIsScrubbing)
    {
        MeshComp->PlayAnimation(ActiveState->CurrentAnimation, ActiveState->bIsLooping, 0.0f);
        MeshComp->SetAnimationPosition(ActiveState->CurrentTime);
        ActiveState->bBoneLinesDirty = true;
        if (ImGui::IsMouseReleased(ImGuiMouseButton_Left))
        {
            ActiveState->bIsScrubbing = false;
        }
    }
    // Update time slider (Engine to UI)
    // Update the UI with the engine's time only when the user is not dragging the slider.
    else
    {
        if (ActiveState->bIsPlaying)
        {
            ActiveState->CurrentTime = CurrentAnimPosition;
        }
    }

    // Update total animation length
    if (ActiveState->CurrentAnimation)
    {
        ActiveState->TotalTime = ActiveState->CurrentAnimation->GetSequenceLength();
    }

    // Notify Trigger Logic
    float PreviousTime = ActiveState->PreviousTime;
    if (ActiveState->CurrentTime < PreviousTime)
    {
        // If looped or jumped back: check from the beginning
        PreviousTime = 0.0f;
    }

    if (ActiveState->bIsPlaying && ActiveState->TotalTime > 0.0f)
    {
        // use track/notify index
        for (int32 trackIndex = 0; trackIndex < (int32)ActiveState->NotifyTracks.size(); ++trackIndex)
        {
            FNotifyTrack& track = ActiveState->NotifyTracks[trackIndex];
            for (int32 notifyIndex = 0; notifyIndex < (int32)track.Notifies.size(); ++notifyIndex)
            {
                FAnimNotifyEvent& notify = track.Notifies[notifyIndex];
                const float start = notify.TriggerTime;
                const float end = (notify.Duration > 0.0f)
                    ? (notify.TriggerTime + notify.Duration)
                    : notify.TriggerTime;

                const bool justTriggered = (start > PreviousTime && start <= ActiveState->CurrentTime);

                // Check the range so it remains true for the duration
                const bool inActiveRange = (notify.Duration > 0.0f &&
                        ActiveState->CurrentTime >= start &&
                        ActiveState->CurrentTime <= end);

                // 1) Log and play sound at the moment of trigger
                if (justTriggered)
                {
                    UE_LOG("[Notify] Track %d, Notify %d, '%s' START (t=%.3f, dur=%.3f)",
                        trackIndex,
                        notifyIndex,
                        notify.NotifyName.ToString().c_str(),
                        start,
                        notify.Duration
                    );

                    // If a sound is specified, play it once
                    if (!notify.SoundPath.empty() && ActiveState->PreviewActor)
                    {
                        UAudioComponent* AudioComp = (UAudioComponent*)ActiveState->PreviewActor->GetComponent(UAudioComponent::StaticClass());
                        if (AudioComp)
                        {
                            USound* SoundToPlay = UResourceManager::GetInstance().Load<USound>(notify.SoundPath);
                            if (SoundToPlay)
                            {
                                UE_LOG("[Notify] Play sound '%s'", notify.SoundPath.c_str());
                                AudioComp->SetSound(SoundToPlay);
                                AudioComp->Play();
                            }
                            else
                            {
                                UE_LOG("[Notify] Failed to load sound '%s'", notify.SoundPath.c_str());
                            }
                        }
                        else
                        {
                            UE_LOG("[Notify] AudioComponent not found on PreviewActor");
                        }
                    }
                }

                // 2) Log every frame during the duration
                if (inActiveRange)
                {
                    UE_LOG("[Notify] Track %d, Notify %d, '%s' ACTIVE (t=%.3f in [%.3f, %.3f])",
                        trackIndex,
                        notifyIndex,
                        notify.NotifyName.ToString().c_str(),
                        ActiveState->CurrentTime,
                        start,
                        end
                    );
                }

                // If Duration is 0, it's just a 'momentary event', 
                // so only the justTriggered log above will be printed
            }
        }
    }
    ActiveState->PreviousTime = ActiveState->CurrentTime;
}

void SAnimationViewerWindow::PreRenderViewportUpdate()
{
    if (!ActiveState || !ActiveState->PreviewActor) return;

    // 기본 애니메이션 포즈 위에 사용자가 조작한 본 트랜스폼 오프셋을 적용
    // 누적 방지를 위해 먼저 애니메이션 포즈로 리셋 후 additive 적용
    if (USkeletalMeshComponent* MeshComp = ActiveState->PreviewActor->GetSkeletalMeshComponent())
    {
        // 누적 방지를 위해 먼저 애니메이션 포즈로 리셋
        MeshComp->ResetToAnimationPose();

        if (!ActiveState->BoneAdditiveTransforms.IsEmpty())
        {
            MeshComp->ApplyAdditiveTransforms(ActiveState->BoneAdditiveTransforms);
        }
    }

    // 본이 선택된 경우, 기즈모 위치를 본의 최종 트랜스폼에 맞춰 업데이트
    if (ActiveState->SelectedBoneIndex >= 0 && ActiveState->World)
    {
        AGizmoActor* Gizmo = ActiveState->World->GetGizmoActor();
        bool bCurrentlyDragging = Gizmo && Gizmo->GetbIsDragging();

        // 드래그 첫 프레임인지 확인 (World→Relative→World 변환 오차 방지)
        bool bIsFirstDragFrame = bCurrentlyDragging && !ActiveState->bWasGizmoDragging;

        if (bCurrentlyDragging && !bIsFirstDragFrame)
        {
            // 첫 프레임이 아닐 때만 기즈모로부터 트랜스폼 업데이트
            UpdateBoneTransformFromGizmo(ActiveState);
        }
        else if (!bCurrentlyDragging)
        {
            ActiveState->PreviewActor->RepositionAnchorToBone(ActiveState->SelectedBoneIndex);
        }
        // 첫 프레임에서는 아무것도 하지 않음 (앵커가 아직 움직이지 않았으므로)

        // 드래그 상태 업데이트 (다음 프레임에서 첫 프레임 감지용)
        ActiveState->bWasGizmoDragging = bCurrentlyDragging;
    }

    // Reconstruct bone overlay
    if (ActiveState->bShowBones)
    {
        ActiveState->bBoneLinesDirty = true;
    }
    if (ActiveState->bShowBones && ActiveState->PreviewActor && ActiveState->CurrentMesh && ActiveState->bBoneLinesDirty)
    {
        if (ULineComponent* LineComp = ActiveState->PreviewActor->GetBoneLineComponent())
        {
            LineComp->SetLineVisible(true);
        }
        ActiveState->PreviewActor->RebuildBoneLines(ActiveState->SelectedBoneIndex);
        ActiveState->bBoneLinesDirty = false;
    }
}

ViewerState* SAnimationViewerWindow::CreateViewerState(const char* Name, UEditorAssetPreviewContext* Context)
{
    ViewerState* NewState = AnimationViewerBootstrap::CreateViewerState(Name, World, Device);
    if (!NewState) return nullptr;

    if (Context && !Context->AssetPath.empty())
    {
        LoadSkeletalMesh(NewState, Context->AssetPath);
    }
    return NewState;
}

void SAnimationViewerWindow::DestroyViewerState(ViewerState*& State)
{
    AnimationViewerBootstrap::DestroyViewerState(State);
}

void SAnimationViewerWindow::OnSkeletalMeshLoaded(ViewerState* State, const FString& Path)
{
    if (!State || !State->CurrentMesh)
        return;

    // Update compatible animation list with STRICT skeleton structure matching
    State->CompatibleAnimations.Empty();
    if (const FSkeleton* CurrentSkeleton = State->CurrentMesh->GetSkeleton())
    {
        // Compute signature for current mesh skeleton
        uint64 MeshSignature = ComputeSkeletonSignature(*CurrentSkeleton);
        int32 MeshBoneCount = static_cast<int32>(CurrentSkeleton->Bones.size());
        const FString& MeshSkeletonName = CurrentSkeleton->Name;

        UE_LOG("SAnimationViewerWindow: Mesh skeleton '%s' signature = 0x%016llX (%d bones)",
            MeshSkeletonName.c_str(), MeshSignature, MeshBoneCount);

        const auto& AllAnimations = UResourceManager::GetInstance().GetAnimations();
        int32 CompatibleCount = 0;

        for (UAnimSequence* Anim : AllAnimations)
        {
            if (!Anim) continue;

            // STRICT COMPATIBILITY CHECK:
            // 1. Skeleton signature must match (structure, hierarchy, names, order)
            // 2. Bone count must match
            // 3. Skeleton name should match (optional warning if different)

            bool bSignatureMatch = (Anim->GetSkeletonSignature() == MeshSignature);
            bool bBoneCountMatch = (Anim->GetSkeletonBoneCount() == MeshBoneCount);
            bool bNameMatch = (Anim->GetSkeletonName() == MeshSkeletonName);

            if (bSignatureMatch && bBoneCountMatch)
            {
                State->CompatibleAnimations.Add(Anim);
                CompatibleCount++;

                // Log warning if structure matches but name differs (unusual case)
                if (!bNameMatch)
                {
                    UE_LOG("SAnimationViewerWindow: Warning - Animation '%s' has matching structure "
                           "but different skeleton name ('%s' vs '%s')",
                        Anim->GetFilePath().c_str(),
                        Anim->GetSkeletonName().c_str(),
                        MeshSkeletonName.c_str());
                }
            }
            else if (bNameMatch && (!bSignatureMatch || !bBoneCountMatch))
            {
                // Log incompatible animations that have matching names (helps debugging)
                UE_LOG("SAnimationViewerWindow: Animation '%s' has matching skeleton name '%s' "
                       "but incompatible structure (sig: %s, bones: %s)",
                    Anim->GetFilePath().c_str(),
                    MeshSkeletonName.c_str(),
                    bSignatureMatch ? "OK" : "MISMATCH",
                    bBoneCountMatch ? "OK" : "MISMATCH");
            }
        }

        UE_LOG("SAnimationViewerWindow: Found %d compatible animations for skeleton '%s'",
            CompatibleCount, MeshSkeletonName.c_str());
    }
}

void SAnimationViewerWindow::RenderRightPanel()
{
    if (!ActiveState)   return;

    if (ActiveState->SelectedNotify.IsValid())
    {
        RenderNotifyProperties();
    }
    else
    {
        SViewerWindow::RenderRightPanel();
    }
}

void SAnimationViewerWindow::LoadSkeletalMesh(ViewerState* State, const FString& Path)
{
    if (!State || Path.empty())
        return;

    // Load the skeletal mesh using the resource manager
    USkeletalMesh* Mesh = UResourceManager::GetInstance().Load<USkeletalMesh>(Path);
    if (Mesh && State->PreviewActor)
    {
        // Set the mesh on the preview actor
        State->PreviewActor->SetSkeletalMesh(Path);
        State->CurrentMesh = Mesh;

        // Call the hook to update compatible animations (same as asset browser)
        OnSkeletalMeshLoaded(State, Path);

        // Reset current animation state
        State->CurrentAnimation = nullptr;
        State->TotalTime = 0.0f;
        State->CurrentTime = 0.0f;
        State->bIsPlaying = false;

        // Expand all bone nodes by default on mesh load
        State->ExpandedBoneIndices.clear();
        if (const FSkeleton* Skeleton = State->CurrentMesh->GetSkeleton())
        {
            for (int32 i = 0; i < Skeleton->Bones.size(); ++i)
            {
                State->ExpandedBoneIndices.insert(i);
            }
        }

        State->LoadedMeshPath = Path;  // Track for resource unloading

        // Update mesh path buffer for display in UI
        strncpy_s(State->MeshPathBuffer, Path.c_str(), sizeof(State->MeshPathBuffer) - 1);

        // Sync mesh visibility with checkbox state
        if (auto* Skeletal = State->PreviewActor->GetSkeletalMeshComponent())
        {
            Skeletal->SetVisibility(State->bShowMesh);
        }

        // Mark bone lines as dirty to rebuild on next frame
        State->bBoneLinesDirty = true;

        // Clear and sync bone line visibility
        if (auto* LineComp = State->PreviewActor->GetBoneLineComponent())
        {
            LineComp->ClearLines();
            LineComp->SetLineVisible(State->bShowBones);
        }

        UE_LOG("SAnimationViewerWindow: Loaded skeletal mesh from %s", Path.c_str());
    }
    else
    {
        UE_LOG("SAnimationViewerWindow: Failed to load skeletal mesh from %s", Path.c_str());
    }
}

void SAnimationViewerWindow::RenderNotifyProperties()
{
    if (!ActiveState || !ActiveState->SelectedNotify.IsValid())
        return;

    ImGuiStyle& style = ImGui::GetStyle();
    float spacing = style.ItemSpacing.y;

    // -------------------------
    // Section Header
    // -------------------------
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 6);
    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.30f, 0.30f, 0.30f, 0.8f));
    ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]);
    ImGui::Text("NOTIFY PROPERTIES");
    ImGui::PopFont();
    ImGui::PopStyleColor();
    ImGui::Dummy(ImVec2(0, 6));
    ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(0.35f, 0.35f, 0.35f, 0.6f));
    ImGui::Separator();
    ImGui::PopStyleColor();
    ImGui::Dummy(ImVec2(0, 4));

    // -------------------------
    // Local ref
    // -------------------------
    int trackIdx = ActiveState->SelectedNotify.TrackIndex;
    int notifyIdx = ActiveState->SelectedNotify.NotifyIndex;

    FAnimNotifyEvent& notify =
        ActiveState->NotifyTracks[trackIdx].Notifies[notifyIdx];

    // -------------------------
    // Name
    // -------------------------
    ImGui::Spacing();
    char nameBuffer[128];
    strcpy_s(nameBuffer, sizeof(nameBuffer), notify.NotifyName.ToString().c_str());

    ImGui::Text("Name");
    ImGui::Dummy(ImVec2(0, 2));
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
    if (ImGui::InputText("##NotifyName", nameBuffer, sizeof(nameBuffer),
        ImGuiInputTextFlags_EnterReturnsTrue |
        ImGuiInputTextFlags_AutoSelectAll))
    {
        FName NewName(nameBuffer);
        notify.NotifyName = NewName;

        if (UAnimSequence* Anim = ActiveState->CurrentAnimation)
        {
            if (UAnimDataModel* Model = Anim->GetDataModel())
            {
                Model->NotifyTracks[trackIdx].Notifies[notifyIdx].NotifyName = NewName;
            }
        }
    }

    ImGui::Dummy(ImVec2(0, 10));

    // -------------------------
    // TriggerTime
    // -------------------------
    ImGui::Text("Trigger Time");
    ImGui::Dummy(ImVec2(0, 2));
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
    if (ImGui::DragFloat("##TriggerTime", &notify.TriggerTime,
        0.01f, 0.0f, ActiveState->TotalTime, "%.2f s"))
    {
        notify.TriggerTime = std::clamp(notify.TriggerTime, 0.0f, ActiveState->TotalTime);

        // Model sync
        if (UAnimSequence* Anim = ActiveState->CurrentAnimation)
        {
            if (UAnimDataModel* Model = Anim->GetDataModel())
            {
                Model->NotifyTracks[trackIdx].Notifies[notifyIdx].TriggerTime = notify.TriggerTime;
            }
        }
    }

    ImGui::Dummy(ImVec2(0, 8));

    // -------------------------
    // Duration
    // -------------------------
    ImGui::Text("Duration");
    ImGui::Dummy(ImVec2(0, 2));
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
    if (ImGui::DragFloat("##Duration", &notify.Duration,
        0.01f, 0.0f, ActiveState->TotalTime, "%.2f s"))
    {
        notify.Duration = std::max(0.0f, notify.Duration);

        // Model sync
        if (UAnimSequence* Anim = ActiveState->CurrentAnimation)
        {
            if (UAnimDataModel* Model = Anim->GetDataModel())
            {
                Model->NotifyTracks[trackIdx].Notifies[notifyIdx].Duration = notify.Duration;
            }
        }
    }

    ImGui::Dummy(ImVec2(0, 10));

    // -------------------------
    // Color
    // -------------------------
    ImGui::Text("Color");
    ImGui::Dummy(ImVec2(0, 2));
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);

    float col[4] = {
        notify.Color.R,
        notify.Color.G,
        notify.Color.B,
        notify.Color.A
    };

    if (ImGui::ColorEdit4("##NotifyColor", col,
        ImGuiColorEditFlags_NoLabel |
        ImGuiColorEditFlags_DisplayRGB))
    {
        notify.Color = FLinearColor(col[0], col[1], col[2], col[3]);

        // Model sync
        if (UAnimSequence* Anim = ActiveState->CurrentAnimation)
        {
            if (UAnimDataModel* Model = Anim->GetDataModel())
            {
                Model->NotifyTracks[trackIdx].Notifies[notifyIdx].Color = notify.Color;
            }
        }
    }

    ImGui::Spacing();
}

void SAnimationViewerWindow::AnimJumpToStart()
{
    if (!ActiveState || !ActiveState->PreviewActor) return;

    ActiveState->bIsPlaying = false;
    ActiveState->CurrentTime = 0.0f;

    if (USkeletalMeshComponent* MeshComp = ActiveState->PreviewActor->GetSkeletalMeshComponent())
    {
        MeshComp->PlayAnimation(ActiveState->CurrentAnimation, ActiveState->bIsLooping, 0.0f);
        MeshComp->SetAnimationPosition(ActiveState->CurrentTime);
        MeshComp->TickComponent(0.0f);

        // 애니메이션 포즈를 기반으로 현재 포즈 업데이트 (additive 포함)
        MeshComp->ResetToAnimationPose();
        if (!ActiveState->BoneAdditiveTransforms.IsEmpty())
        {
            MeshComp->ApplyAdditiveTransforms(ActiveState->BoneAdditiveTransforms);
        }
    }

    // 본 라인 즉시 다시 그리기
    if (ActiveState->bShowBones && ActiveState->CurrentMesh)
    {
        ActiveState->PreviewActor->RebuildBoneLines(ActiveState->SelectedBoneIndex);
    }
    ActiveState->bBoneLinesDirty = true;
}

void SAnimationViewerWindow::AnimJumpToEnd()
{
    if (!ActiveState || !ActiveState->PreviewActor) return;

    ActiveState->bIsPlaying = false;
    ActiveState->CurrentTime = ActiveState->TotalTime;

    if (USkeletalMeshComponent* MeshComp = ActiveState->PreviewActor->GetSkeletalMeshComponent())
    {
        MeshComp->PlayAnimation(ActiveState->CurrentAnimation, ActiveState->bIsLooping, 0.0f);
        MeshComp->SetAnimationPosition(ActiveState->CurrentTime);
        MeshComp->TickComponent(0.0f);

        // 애니메이션 포즈를 기반으로 현재 포즈 업데이트 (additive 포함)
        MeshComp->ResetToAnimationPose();
        if (!ActiveState->BoneAdditiveTransforms.IsEmpty())
        {
            MeshComp->ApplyAdditiveTransforms(ActiveState->BoneAdditiveTransforms);
        }
    }

    // 본 라인 즉시 다시 그리기
    if (ActiveState->bShowBones && ActiveState->CurrentMesh)
    {
        ActiveState->PreviewActor->RebuildBoneLines(ActiveState->SelectedBoneIndex);
    }
    ActiveState->bBoneLinesDirty = true;
}

void SAnimationViewerWindow::AnimStep(bool bForward)
{
    if (!ActiveState || !ActiveState->PreviewActor) return;

    ActiveState->bIsPlaying = false;
    const float FrameTime = 1.0f / 30.0f;

    if (bForward)
    {
        ActiveState->CurrentTime = FMath::Min(ActiveState->TotalTime, ActiveState->CurrentTime + FrameTime);
    }
    else
    {
        ActiveState->CurrentTime = FMath::Max(0.0f, ActiveState->CurrentTime - FrameTime);
    }

    if (USkeletalMeshComponent* MeshComp = ActiveState->PreviewActor->GetSkeletalMeshComponent())
    {
        MeshComp->PlayAnimation(ActiveState->CurrentAnimation, ActiveState->bIsLooping, 0.0f);
        MeshComp->SetAnimationPosition(ActiveState->CurrentTime);
        MeshComp->TickComponent(0.0f);

        // 애니메이션 포즈를 기반으로 현재 포즈 업데이트 (additive 포함)
        MeshComp->ResetToAnimationPose();
        if (!ActiveState->BoneAdditiveTransforms.IsEmpty())
        {
            MeshComp->ApplyAdditiveTransforms(ActiveState->BoneAdditiveTransforms);
        }
    }

    // 본 라인 즉시 다시 그리기
    if (ActiveState->bShowBones && ActiveState->CurrentMesh)
    {
        ActiveState->PreviewActor->RebuildBoneLines(ActiveState->SelectedBoneIndex);
    }
    ActiveState->bBoneLinesDirty = true;
}

void SAnimationViewerWindow::RenderCenterPanel()
{
    // 툴바 렌더링 (뷰포트 상단)
    RenderViewerToolbar();

    // 툴바 아래 남은 공간 계산
    float contentHeight = ImGui::GetContentRegionAvail().y;
    float itemSpacingY = ImGui::GetStyle().ItemSpacing.y;
    float viewportHeight = (contentHeight - itemSpacingY) * 0.7f;
    float timelineHeight = (contentHeight - itemSpacingY) * 0.3f;
    float innerWidth = ImGui::GetContentRegionAvail().x;

    RenderViewportArea(innerWidth, viewportHeight);
    RenderTimelineArea(innerWidth, timelineHeight);
}

void SAnimationViewerWindow::RenderViewportArea(float width, float height)
{
    // 뷰포트가 그려질 위치 저장
    ImVec2 Pos = ImGui::GetCursorScreenPos();

    // 뷰포트 영역 설정
    CenterRect.Left   = Pos.x;
    CenterRect.Top    = Pos.y;
    CenterRect.Right  = Pos.x + width;
    CenterRect.Bottom = Pos.y + height;
    CenterRect.UpdateMinMax();

    // 뷰포트 렌더링 (텍스처에)
    OnRenderViewport();

    // ImGui::Image로 결과 텍스처 표시
    if (ActiveState && ActiveState->Viewport)
    {
        ID3D11ShaderResourceView* SRV = ActiveState->Viewport->GetSRV();
        if (SRV)
        {
            ImGui::Image((void*)SRV, ImVec2(width, height));
            // ImGui의 Z-order를 고려한 정확한 hover 체크
            ActiveState->Viewport->SetViewportHovered(ImGui::IsItemHovered());
        }
        else
        {
            ImGui::Dummy(ImVec2(width, height));
            ActiveState->Viewport->SetViewportHovered(false);
        }
    }
    else
    {
        ImGui::Dummy(ImVec2(width, height));
    }
}

void SAnimationViewerWindow::RenderTimelineArea(float width, float height)
{
    ImGui::BeginChild("TimelineArea", ImVec2(width, height), true,
        ImGuiWindowFlags_NoNavInputs | ImGuiWindowFlags_NoNavFocus);

    RenderTimelineControls();
    ImGui::Separator();
    ImGui::Spacing();

    RenderTrackAndGridLayout();
    
    ImGui::EndChild();  // TimelineGridSmall
}


void SAnimationViewerWindow::RenderTimelineControls()
{
    ImGui::Spacing();

    // -------------------------------------------------------------------------
    // Left-Aligned Animation Controls
    // -------------------------------------------------------------------------
    if (ImGui::Button("|<<")) AnimJumpToStart();
    ImGui::SameLine();
    if (ImGui::Button("<")) AnimStep(false);
    ImGui::SameLine();

    if (ActiveState->bIsPlaying)
    {
        if (ImGui::Button("Pause")) ActiveState->bIsPlaying = false;
    }
    else
    {
        if (ImGui::Button("Play")) ActiveState->bIsPlaying = true;
    }

    ImGui::SameLine();
    if (ImGui::Button(">")) AnimStep(true);
    ImGui::SameLine();
    if (ImGui::Button(">>|")) AnimJumpToEnd();

    ImGui::SameLine();

    // Reverse Button with highlight
    bool bHighlighted = ActiveState->bReversePlay;
    if (bHighlighted)
    {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.6f, 0.9f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.4f, 0.7f, 1.0f, 1.0f));
    }
    if (ImGui::Button("Reverse"))
        ActiveState->bReversePlay = !ActiveState->bReversePlay;
    if (bHighlighted) ImGui::PopStyleColor(2);

    ImGui::SameLine();
    ImGui::Checkbox("Loop", &ActiveState->bIsLooping);

    const float windowRightEdge = ImGui::GetWindowContentRegionMax().x;
    const float speedButtonWidth = 70.0f;
    const float rightButtonStartX = windowRightEdge - speedButtonWidth;

    // -------------------------------------------------------------------------
    // Playback Speed Combo Button (Transparent)
    // -------------------------------------------------------------------------
    ImGui::SameLine();
    ImGui::SetCursorPosX(rightButtonStartX);

    char speedLabel[32];
    float v = ActiveState->PlaybackSpeed;

    // Check second decimal digit
    int secondDigit = (int)(v * 100) % 10;
    if (secondDigit == 0)   sprintf_s(speedLabel, " x%.1f v", v);   // x1.5 → x1.5
    else                    sprintf_s(speedLabel, " x%.2f v", v);   // x1.23 → x1.23

    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1, 1, 1, 0.1f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1, 1, 1, 0.15f));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2, 1));
    if (ImGui::Button(speedLabel, ImVec2(speedButtonWidth, 0)))
        ImGui::OpenPopup("PlaybackSpeedPopup");
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(3);

    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Playback Speed Options");

    // -------------------------------------------------------------------------
    // Playback Speed Popup
    // -------------------------------------------------------------------------
    ImGui::SetNextWindowSize(ImVec2(200, 0));
    if (ImGui::BeginPopup("PlaybackSpeedPopup"))
    {
        // Lighter hover/select background for rows
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.25f, 0.25f, 0.25f, 0.3f));
        ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.30f, 0.30f, 0.30f, 0.6f));

        ImGui::TextDisabled("PLAYBACK SPEED");
        ImGui::Separator();
        ImGui::Dummy(ImVec2(0, 4));

        const float presets[] = { 0.1f, 0.25f, 0.5f, 1.0f, 1.5f, 2.0f, 5.0f, 10.0f };
        float currentSpeed = ActiveState->PlaybackSpeed;
        bool bCustomIsSelected = true;

        for (float s : presets)
        {
            if (fabs(currentSpeed - s) < 0.001f)
            {
                bCustomIsSelected = false;
                break;
            }
        }

        ImDrawList* dl = ImGui::GetWindowDrawList();
        const float radioRadius = 4.0f;

        // ---------------------------------------------------------------------
        // Preset Rows
        // ---------------------------------------------------------------------
        for (int i = 0; i < IM_ARRAYSIZE(presets); i++)
        {
            float s = presets[i];
            bool isSelected = (fabs(currentSpeed - s) < 0.001f);

            ImGui::PushID(i);

            // Selectable row (label suppressed)
            if (ImGui::Selectable("     ", isSelected, ImGuiSelectableFlags_DontClosePopups))
            {
                ActiveState->PlaybackSpeed = s;

                float mouseX = ImGui::GetMousePos().x;
                float itemMinX = ImGui::GetItemRectMin().x;

                if (mouseX > itemMinX + 30.0f)
                    ImGui::CloseCurrentPopup();
            }

            // Row rect
            ImVec2 rMin = ImGui::GetItemRectMin();
            ImVec2 rMax = ImGui::GetItemRectMax();
            float centerY = (rMin.y + rMax.y) * 0.5f;

            // Radio indicator
            ImVec2 c(rMin.x + 12.0f, centerY);
            if (isSelected)
                dl->AddCircleFilled(c, radioRadius, ImGui::GetColorU32(ImGuiCol_Text));
            else
                dl->AddCircle(c, radioRadius, ImGui::GetColorU32(ImGuiCol_TextDisabled));

            // Label text
            char txt[32];
            sprintf_s(txt, "x%.1f", s);

            ImVec2 tSize = ImGui::CalcTextSize(txt);
            float tX = rMin.x + 24.0f;
            float tY = centerY - tSize.y * 0.5f - 2.0f;

            dl->AddText(ImVec2(tX, tY), ImGui::GetColorU32(ImGuiCol_Text), txt);

            ImGui::PopID();
        }

        ImGui::Separator();
        ImGui::Dummy(ImVec2(0, 4));

        // ---------------------------------------------------------------------
        // Custom Speed Row
        // ---------------------------------------------------------------------
        {
            ImGui::PushID("CustomSpeed");

            if (ImGui::Selectable("     ", bCustomIsSelected, ImGuiSelectableFlags_DontClosePopups))
            {
                float mouseX = ImGui::GetMousePos().x;
                float itemMinX = ImGui::GetItemRectMin().x;

                if (mouseX > itemMinX + 30.0f)
                    ImGui::CloseCurrentPopup();
            }

            ImVec2 rMin = ImGui::GetItemRectMin();
            ImVec2 rMax = ImGui::GetItemRectMax();
            float centerY = (rMin.y + rMax.y) * 0.5f;

            // Radio
            ImVec2 c(rMin.x + 12.0f, centerY);
            if (bCustomIsSelected)
                dl->AddCircleFilled(c, radioRadius, ImGui::GetColorU32(ImGuiCol_Text));
            else
                dl->AddCircle(c, radioRadius, ImGui::GetColorU32(ImGuiCol_TextDisabled));

            // Text
            const char* txt = "Custom";
            ImVec2 tSize = ImGui::CalcTextSize(txt);
            float tX = rMin.x + 24.0f;
            float tY = centerY - tSize.y * 0.5f - 1.0f;
            dl->AddText(ImVec2(tX, tY), ImGui::GetColorU32(ImGuiCol_Text), txt);

            // Slider
            ImGui::Dummy(ImVec2(0, 4));
            ImGui::Indent(25.0f);

            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 1));
            ImGui::SetNextItemWidth(-1);

            float tempSpeed = ActiveState->PlaybackSpeed;
            if (ImGui::SliderFloat("##CustomSlider", &tempSpeed, 0.1f, 5.0f, "%.2f"))
                ActiveState->PlaybackSpeed = tempSpeed;

            ImGui::PopStyleVar();
            ImGui::Unindent(25.0f);

            ImGui::PopID();
        }

        ImGui::PopStyleColor(2);
        ImGui::EndPopup();
    }
}

void SAnimationViewerWindow::RenderTrackAndGridLayout()
{
    ImGui::BeginChild("TrackLayout", ImVec2(0, 0), false);

    float leftWidth = 130.0f;
    float rightWidth = ImGui::GetContentRegionAvail().x - leftWidth;

    const float RowHeight = 24.0f;
    const float HeaderHeight = 24.0f;

    TArray<FString> LeftRows;
    BuildLeftRows(LeftRows);     // Notifies(Tracks) / Curves / Attributes

    TArray<int> RowToNotifyIndex;
    BuildRowToNotifyIndex(LeftRows, RowToNotifyIndex);

    RenderLeftTrackList(leftWidth, RowHeight, HeaderHeight, LeftRows, RowToNotifyIndex);
    ImGui::SameLine();

    RenderRightTimeline(rightWidth, RowHeight, HeaderHeight, LeftRows, RowToNotifyIndex);
    ImGui::EndChild();
}

void SAnimationViewerWindow::RenderLeftTrackList(float width, float RowHeight, float HeaderHeight, const TArray<FString>& LeftRows, const TArray<int>& RowToNotifyIndex)
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::BeginChild("TrackListSmall", ImVec2(width, 0), true);
    ImGui::Dummy(ImVec2(0, HeaderHeight));
    ImGui::PopStyleVar();

    int RowCount = LeftRows.size();
    ImDrawList* DL = ImGui::GetWindowDrawList();
    ImVec2 childMin = ImGui::GetWindowPos();
    ImVec2 childMax = ImVec2(childMin.x + width, childMin.y + ImGui::GetWindowSize().y);
    DL->AddRectFilled(childMin, childMax, IM_COL32(28, 28, 28, 255));

    // Row Rendering Loop
    for (int i = 0; i < RowCount; i++)
    {
        ImVec2 rowPos = ImGui::GetCursorScreenPos();

        ImGui::InvisibleButton(
            ("TrackListRowBtn_" + std::to_string(i)).c_str(),
            ImVec2(width - 2, RowHeight)
        );

        // ============================
        // 0번 Row (Notifies) — 폴더블 UI
        // ============================
        if (i == 0)
        {
            const char* arrow = ActiveState->bFoldNotifies ? "▶" : "▼";

            DL->AddText(
                ImVec2(rowPos.x + 4, rowPos.y + 4),
                IM_COL32(255, 255, 255, 255),
                arrow
            );

            DL->AddText(
                ImVec2(rowPos.x + 20, rowPos.y + 4),
                IM_COL32(230, 230, 230, 255),
                "Notifies"
            );

            if (ImGui::IsItemClicked())
            {
                ActiveState->bFoldNotifies = !ActiveState->bFoldNotifies;
            }

            if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
                ImGui::OpenPopup("NotifiesContextMenu");
        }
        else
        {
            float indent = (RowToNotifyIndex[i] != -1) ? 16.0f : 0.0f;

            // General Row (Track, Curves, Attributes)
            DL->AddText(
                ImVec2(rowPos.x + 4 + indent, rowPos.y + 4),
                IM_COL32(230, 230, 230, 255),
                LeftRows[i].c_str()
            );
        }

        // Divider
        DL->AddLine(
            ImVec2(rowPos.x, rowPos.y + RowHeight),
            ImVec2(rowPos.x + width, rowPos.y + RowHeight),
            IM_COL32(80, 80, 80, 200)
        );
    }

    // Popup must be OUTSIDE the loop
    if (ImGui::BeginPopup("NotifiesContextMenu"))
    {
        if (ImGui::MenuItem("Add Notify Track"))
        {
            int idx = ActiveState->NotifyTracks.size();
            ActiveState->NotifyTracks.push_back(FNotifyTrack("Track " + std::to_string(idx)));
            SyncNotifyTracksToDataModel();
        }
        ImGui::EndPopup();
    }

    ImGui::EndChild();
}

void SAnimationViewerWindow::RenderRightTimeline(float width, float RowHeight, float HeaderHeight, const TArray<FString>& LeftRows, const TArray<int>& RowToNotifyIndex)
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::BeginChild("TimelineGridSmall", ImVec2(width, 0), true);
    ImGui::PopStyleVar();

    RenderTimelineHeader(HeaderHeight);
    RenderTimelineGridBody(RowHeight, LeftRows, RowToNotifyIndex);

    ImGui::EndChild();
}

void SAnimationViewerWindow::RenderTimelineHeader(float height)
{
    ImGui::BeginChild("TimelineHeader", ImVec2(0, height), false, ImGuiWindowFlags_NoScrollbar);

    ImDrawList* DL = ImGui::GetWindowDrawList();
    ImVec2 headerPos = ImGui::GetCursorScreenPos();
    float headerWidth = ImGui::GetContentRegionAvail().x;

    // Background
    DL->AddRectFilled(headerPos,
        ImVec2(headerPos.x + headerWidth, headerPos.y + height),
        IM_COL32(30, 30, 30, 255));

    // Frame markers
    const int frameCount = 50;
    for (int f = 0; f < frameCount; f++)
    {
        float x = headerPos.x + (headerWidth / frameCount) * f;
        DL->AddLine(ImVec2(x, headerPos.y), ImVec2(x, headerPos.y + height),
            IM_COL32(80, 80, 80, 180));

        char buf[16];
        sprintf_s(buf, "%d", f);
        DL->AddText(ImVec2(x + 2, headerPos.y + 4), IM_COL32(200, 200, 200, 255), buf);
    }

    // Playhead handle
    float norm = ActiveState->TotalTime > 0 ? ActiveState->CurrentTime / ActiveState->TotalTime : 0.0f;
    float playheadX = headerPos.x + norm * headerWidth;
    DL->AddTriangleFilled(
        ImVec2(playheadX - 6, headerPos.y),
        ImVec2(playheadX + 6, headerPos.y),
        ImVec2(playheadX, headerPos.y + 10),
        IM_COL32(255, 80, 80, 255)
    );

    // Hitbox
    ImGui::SetCursorScreenPos(ImVec2(playheadX - 8, headerPos.y));
    ImGui::InvisibleButton("UnifiedPlayheadHandle", ImVec2(16, height));

    bool draggingHeader = ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left);
    if (draggingHeader)
    {
        float mx = std::clamp(ImGui::GetIO().MousePos.x, headerPos.x, headerPos.x + headerWidth);
        float newNorm = (mx - headerPos.x) / headerWidth;
        ActiveState->CurrentTime = newNorm * ActiveState->TotalTime;

        ActiveState->bIsScrubbing = true;

        if (auto* mesh = ActiveState->PreviewActor->GetSkeletalMeshComponent())
        {
            mesh->SetAnimationPosition(ActiveState->CurrentTime);
            mesh->TickComponent(0.f);
        }
    }
    if (ImGui::IsMouseReleased(ImGuiMouseButton_Left))
        ActiveState->bIsScrubbing = false;

    ImGui::EndChild(); // TimelineHeader
}

void SAnimationViewerWindow::RenderTimelineGridBody(float RowHeight, const TArray<FString>& LeftRows, const TArray<int>& RowToNotifyIndex)
{
    ImGui::BeginChild("TimelineGridBody", ImVec2(0, 0), false);

    ImDrawList* draw = ImGui::GetWindowDrawList();
    ImVec2 gridOrigin = ImGui::GetCursorScreenPos();
    ImVec2 gridAvail = ImGui::GetContentRegionAvail();

    const int RowCount = LeftRows.size();
    float TotalHeight = RowCount * RowHeight;
    float FullHeight = gridAvail.y;

    draw->AddRectFilled(
        gridOrigin,
        ImVec2(gridOrigin.x + gridAvail.x, gridOrigin.y + FullHeight),
        IM_COL32(20, 20, 20, 255)
    );

    // Frame columns
    const int frameCount = 50;
    for (int i = 0; i < frameCount; i++)
    {
        float x = gridOrigin.x + (float)i * (gridAvail.x / frameCount);
        draw->AddLine(
            ImVec2(x, gridOrigin.y),
            ImVec2(x, gridOrigin.y + FullHeight),
            IM_COL32(60, 60, 60, 160)
        );
    }

    //-----------------------------
    // Track Rows + Right-Click Menu
    //-----------------------------
    for (int row = 0; row < RowCount; row++)
    {
        float y0 = gridOrigin.y + row * RowHeight;
        float y1 = y0 + RowHeight;

        // Row bottom line
        draw->AddLine(
            ImVec2(gridOrigin.x, y1),
            ImVec2(gridOrigin.x + gridAvail.x, y1),
            IM_COL32(90, 90, 90, 160)
        );

        // ============================
        //  ROW → NotifyTrack 매핑 사용
        // ============================
        int NotifyIndex = RowToNotifyIndex[row];

        // Draw Notify Marker
        if (NotifyIndex != -1 && ActiveState->TotalTime > 0.0f)
        {
            FNotifyTrack& Track = ActiveState->NotifyTracks[NotifyIndex];

            for (int i = 0; i < Track.Notifies.size(); ++i)
            {
                const FAnimNotifyEvent& Notify = Track.Notifies[i];
                bool isSelected = (ActiveState->SelectedNotify.TrackIndex == NotifyIndex &&
                                   ActiveState->SelectedNotify.NotifyIndex == i);

                float NotifyStartX = gridOrigin.x +
                    (Notify.TriggerTime / ActiveState->TotalTime) * gridAvail.x;

                float NotifyEndX = NotifyStartX;
                if (Notify.Duration > 0.0f)
                {
                    float endNorm = (Notify.TriggerTime + Notify.Duration) / ActiveState->TotalTime;
                    NotifyEndX = gridOrigin.x + endNorm * gridAvail.x;
                }

                // Y center
                float midY = (y0 + y1) * 0.5f;

                //--- Diamond Marker ---
                float s = 5.0f;
                ImVec2 d0(NotifyStartX, midY - s);
                ImVec2 d1(NotifyStartX + s, midY);
                ImVec2 d2(NotifyStartX, midY + s);
                ImVec2 d3(NotifyStartX - s, midY);

                ImU32 fillColor = IM_COL32(160, 130, 220, 255);  // 보라 계열
                ImU32 fillHover = IM_COL32(200, 170, 255, 255);  // hover brighter
                ImU32 barColor = IM_COL32(110, 90, 190, 180);

                //--- Hitbox ---
                ImVec2 hbMin(NotifyStartX - 8, midY - 8);
                ImVec2 hbSize(16, 16);
                ImGui::SetCursorScreenPos(hbMin);
                ImGui::InvisibleButton(("NotifyHit_" + std::to_string(row) + "_" + std::to_string(i)).c_str(), hbSize);

                if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
                {
                    ActiveState->SelectedNotify.TrackIndex = NotifyIndex;
                    ActiveState->SelectedNotify.NotifyIndex = i;
                    ActiveState->SelectedBoneIndex = -1;
                }

                bool hovered = ImGui::IsItemHovered();

                //--- Duration Bar (if duration > 0) ---
                if (NotifyEndX > NotifyStartX)
                {
                    const float barHalfHeight = 9.0f;

                    ImVec2 barMin(NotifyStartX, midY - barHalfHeight);
                    ImVec2 barMax(NotifyEndX, midY + barHalfHeight);
                    ImU32 barFill = IM_COL32(255, 255, 255, 50);
                    ImU32 barBorder = IM_COL32(0, 0, 0, 200);
                    
                    draw->AddRectFilled(barMin, barMax, barFill, 2.5f);     // fill
                    draw->AddRect(barMin, barMax, barBorder, 2.5f, 0, 1.0f);    // border
                }

                //--- Draw Diamond ---
                fillColor = IM_COL32(255, 255, 255, 255);       // White
                fillHover = IM_COL32(225, 225, 225, 255);       // Slightly gray on hover
                ImU32 borderColor = IM_COL32(0, 0, 0, 255);     // Black thin border

                //--- Draw Diamond Fill ---
                draw->AddQuadFilled(
                    d0, d1, d2, d3,
                    hovered ? fillHover : fillColor
                );

                //--- Draw Diamond Border (thin black) ---
                if (isSelected)
                {
                    ImU32 borderColor = IM_COL32(0, 0, 0, 255);     // Black
                    draw->AddQuad(d0, d1, d2, d3, borderColor, 1.0f);
                }

                //--- Text (aligned inside the box or right of diamond) ---
                std::string label = Notify.NotifyName.ToString();
                float textX = (NotifyEndX > NotifyStartX) ? (NotifyStartX + 6) : (NotifyStartX + 10);

                draw->AddText(
                    ImVec2(textX, midY - 7),
                    IM_COL32(230, 230, 230, 255),
                    label.c_str()
                );

                //-----------------------------------------
                // Notify Popup
                //-----------------------------------------
                if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
                    ImGui::OpenPopup(("NotifyPopup_" + std::to_string(row) + "_" + std::to_string(i)).c_str());

                if (ImGui::BeginPopup(("NotifyPopup_" + std::to_string(row) + "_" + std::to_string(i)).c_str()))
                {
                    if (ImGui::MenuItem("Delete"))
                    {
                        Track.Notifies.erase(Track.Notifies.begin() + i);
                        SyncNotifyTracksToDataModel();
                    }

                    if (ImGui::MenuItem("Add Duration +0.2s"))
                        Track.Notifies[i].Duration += 0.2f;

                    ImGui::EndPopup();
                }

                //-----------------------------------------
                // Drag to move Notify
                //-----------------------------------------
                if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
                {
                    float mouseX = ImGui::GetIO().MousePos.x;
                    mouseX = std::clamp(mouseX, gridOrigin.x, gridOrigin.x + gridAvail.x);

                    float norm = (mouseX - gridOrigin.x) / gridAvail.x;
                    Track.Notifies[i].TriggerTime = norm * ActiveState->TotalTime;
                }
                if (ImGui::IsItemDeactivatedAfterEdit())
                {
                    SyncNotifyTracksToDataModel();
                }
            }
        }

        // Hitbox
        ImGui::SetCursorScreenPos(ImVec2(gridOrigin.x, y0));
        ImGui::InvisibleButton(
            ("GridRowBtn_" + std::to_string(row)).c_str(),
            ImVec2(gridAvail.x, RowHeight)
        );

        // If right-clicked anywhere in the row → Add Notify popup
        if (NotifyIndex != -1)
        {
            if (ImGui::BeginPopupContextItem(("RowPopup_" + std::to_string(row)).c_str()))
            {
                if (ImGui::MenuItem("Add Notify"))
                {
                    float mouseX = ImGui::GetMousePos().x - gridOrigin.x;
                    float newTime = (mouseX / gridAvail.x) * ActiveState->TotalTime;
                    newTime = std::clamp(newTime, 0.0f, ActiveState->TotalTime);

                    FAnimNotifyEvent newNotify;
                    newNotify.TriggerTime = newTime;
                    newNotify.Duration = 0.0f;

                    int notifyCount = 0;
                    for (const auto& track : ActiveState->NotifyTracks)
                        notifyCount += track.Notifies.size();

                    newNotify.NotifyName = FName("NewNotify_" + std::to_string(notifyCount + 1));

                    ActiveState->NotifyTracks[NotifyIndex].Notifies.Add(newNotify);
                    SyncNotifyTracksToDataModel();
                }

                ImGui::EndPopup();
            }
        }
    }

    //=========================
    // Playhead / Scrubbing
    //=========================

    float xMin = gridOrigin.x;
    float xMax = gridOrigin.x + gridAvail.x;

    float playheadX_Grid = xMin;
    if (ActiveState->TotalTime > 0)
        playheadX_Grid = xMin + (ActiveState->CurrentTime / ActiveState->TotalTime) * (xMax - xMin);

    // red line
    draw->AddLine(
        ImVec2(playheadX_Grid, gridOrigin.y),
        ImVec2(playheadX_Grid, gridOrigin.y + FullHeight),
        IM_COL32(255, 40, 40, 255), 2.0f
    );

    // handle triangle
    float handleY = gridOrigin.y - 8;
    draw->AddTriangleFilled(
        ImVec2(playheadX_Grid - 6, handleY),
        ImVec2(playheadX_Grid + 6, handleY),
        ImVec2(playheadX_Grid, handleY + 10),
        IM_COL32(255, 40, 40, 255)
    );

    // hitbox
    ImGui::SetCursorScreenPos(ImVec2(playheadX_Grid - 8, gridOrigin.y - 12));
    ImGui::InvisibleButton("UnifiedPlayheadHandle", ImVec2(16, FullHeight + 12));

    bool dragging = ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left);

    if (dragging)
    {
        ActiveState->bIsScrubbing = true;

        float mouseX = ImGui::GetIO().MousePos.x;
        mouseX = std::clamp(mouseX, xMin, xMax);

        float norm = (mouseX - xMin) / (xMax - xMin);
        ActiveState->CurrentTime = norm * ActiveState->TotalTime;

        if (auto* MeshComp = ActiveState->PreviewActor->GetSkeletalMeshComponent())
        {
            MeshComp->SetAnimationPosition(ActiveState->CurrentTime);
            MeshComp->TickComponent(0.f);
        }
    }
    if (ImGui::IsMouseReleased(ImGuiMouseButton_Left))
    {
        ActiveState->bIsScrubbing = false;
    }

    ImGui::EndChild(); // TimelineHeader
}

void SAnimationViewerWindow::BuildLeftRows(TArray<FString>& OutRows)
{
    OutRows.clear();
    OutRows.push_back("Notifies");

    // Notify Tracks only if unfolded
    if (!ActiveState->bFoldNotifies)
    {
        for (auto& Track : ActiveState->NotifyTracks)
            OutRows.push_back(Track.Name);
    }

    OutRows.push_back("Curves");
    OutRows.push_back("Attributes");
}

void SAnimationViewerWindow::BuildRowToNotifyIndex(const TArray<FString>& InRows, TArray<int>& OutMapping)
{
    OutMapping.clear();
    OutMapping.reserve(InRows.size());

    // row 0 = Notifies header
    OutMapping.push_back(-1);

    // The order of NotifyTrack rows is immediately after the "Notifies" header.
    // i.e., LeftRows:
    // [0] Notifies
    // [1..N] NotifyTrack (only when expanded)
    // [... ] Curves
    // [... ] Attributes

    // Notify Tracks only if unfolded
    if (!ActiveState->bFoldNotifies) {
        for (int i = 0; i < ActiveState->NotifyTracks.size(); ++i)
            OutMapping.push_back(i);
    }

    OutMapping.push_back(-1);     // Curves
    OutMapping.push_back(-1);     // Attributes
}

void SAnimationViewerWindow::SyncNotifyTracksToDataModel()
{
    if (ActiveState && ActiveState->CurrentAnimation && ActiveState->CurrentAnimation->GetDataModel())
    {
        ActiveState->CurrentAnimation->GetDataModel()->NotifyTracks = ActiveState->NotifyTracks;
    }
}

// ImGui draw callback - Direct3D 뷰포트 렌더링
void SAnimationViewerWindow::ViewportRenderCallback(const ImDrawList* parent_list, const ImDrawCmd* cmd)
{
    // UserCallbackData로 전달된 this 포인터 가져오기
    SAnimationViewerWindow* window = (SAnimationViewerWindow*)cmd->UserCallbackData;

    if (window && window->ActiveState && window->ActiveState->Viewport)
    {
        FViewport* viewport = window->ActiveState->Viewport;

        // D3D 디바이스 컨텍스트 가져오기
        ID3D11Device* device = window->Device;
        ID3D11DeviceContext* context = nullptr;
        device->GetImmediateContext(&context);

        if (context)
        {
            // ImGui가 변경한 D3D 상태를 뷰포트 렌더링에 맞게 초기화
            // 1. Viewport 설정
            D3D11_VIEWPORT d3dViewport = {};
            d3dViewport.Width = static_cast<float>(viewport->GetSizeX());
            d3dViewport.Height = static_cast<float>(viewport->GetSizeY());
            d3dViewport.MinDepth = 0.0f;
            d3dViewport.MaxDepth = 1.0f;
            d3dViewport.TopLeftX = static_cast<float>(viewport->GetStartX());
            d3dViewport.TopLeftY = static_cast<float>(viewport->GetStartY());
            context->RSSetViewports(1, &d3dViewport);

            // 2. D3D 렌더 상태 복구 (ImGui → 3D 렌더링용)
            float blendFactor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
            context->OMSetBlendState(nullptr, blendFactor, 0xffffffff);  // 블렌딩 비활성화
            context->OMSetDepthStencilState(nullptr, 0);                  // 기본 깊이 테스트
            context->RSSetState(nullptr);                                 // 기본 래스터라이저
            context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

            // 3. 뷰포트 렌더링 실행
            window->OnRenderViewport();

            context->Release();
        }
    }
}
