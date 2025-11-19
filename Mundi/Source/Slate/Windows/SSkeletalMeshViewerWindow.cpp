#include "pch.h"
#include "SlateManager.h"
#include "SSkeletalMeshViewerWindow.h"
#include "Source/Runtime/Engine/Viewer/SkeletalViewerBootstrap.h"
#include "Source/Runtime/Engine/GameFramework/SkeletalMeshActor.h"
#include "Source/Runtime/Engine/Viewer/EditorAssetPreviewContext.h"
#include "Source/Runtime/Renderer/FViewport.h"

SSkeletalMeshViewerWindow::SSkeletalMeshViewerWindow()
{
    CenterRect = FRect(0, 0, 0, 0);
}

SSkeletalMeshViewerWindow::~SSkeletalMeshViewerWindow()
{
    // Clean up tabs if any
    for (int i = 0; i < Tabs.Num(); ++i)
    {
        ViewerState* State = Tabs[i];
        SkeletalViewerBootstrap::DestroyViewerState(State);
    }
    Tabs.Empty();
    ActiveState = nullptr;
}

void SSkeletalMeshViewerWindow::OnRender()
{
    // If window is closed, request cleanup and don't render
    if (!bIsOpen)
    {
        USlateManager::GetInstance().RequestCloseDetachedWindow(this);
        return;
    }

    // Parent detachable window (movable, top-level) with solid background
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoSavedSettings;

    if (!bInitialPlacementDone)
    {
        ImGui::SetNextWindowPos(ImVec2(Rect.Left, Rect.Top));
        ImGui::SetNextWindowSize(ImVec2(Rect.GetWidth(), Rect.GetHeight()));
        bInitialPlacementDone = true;
    }
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

        // Render tab bar and switch active state
        /*if (!ImGui::BeginTabBar("SkeletalViewerTabs", 
            ImGuiTabBarFlags_AutoSelectNewTabs | ImGuiTabBarFlags_Reorderable))
            return;*/
        RenderTabsAndToolbar(EViewerType::Skeletal);

        // 마지막 탭을 닫은 경우 렌더링 중단
        if (!bIsOpen)
        {
            USlateManager::GetInstance().RequestCloseDetachedWindow(this);
            ImGui::End();
            return;
        }

        ImVec2 pos = ImGui::GetWindowPos();
        ImVec2 size = ImGui::GetWindowSize();
        Rect.Left = pos.x; Rect.Top = pos.y; Rect.Right = pos.x + size.x; Rect.Bottom = pos.y + size.y; Rect.UpdateMinMax();

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

        // Remove spacing between panels
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));

        // Left panel - Asset Browser & Bone Hierarchy
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

        // Center panel (viewport area) - 완전히 가려진 경우 렌더링하지 않음
        if (centerWidth > 0.0f)
        {
            ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
            ImGui::BeginChild("CenterPanel", ImVec2(centerWidth, totalHeight), false,
                ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoNavInputs | ImGuiWindowFlags_NoNavFocus);
            ImGui::PopStyleVar();

            // 뷰어 툴바 렌더링 (뷰포트 상단)
            RenderViewerToolbar();

            // 툴바 아래 뷰포트 영역
            ImVec2 viewportPos = ImGui::GetCursorScreenPos();
            float remainingWidth = ImGui::GetContentRegionAvail().x;
            float remainingHeight = ImGui::GetContentRegionAvail().y;

            // 뷰포트 영역 설정
            CenterRect.Left = viewportPos.x;
            CenterRect.Top = viewportPos.y;
            CenterRect.Right = viewportPos.x + remainingWidth;
            CenterRect.Bottom = viewportPos.y + remainingHeight;
            CenterRect.UpdateMinMax();

            // 뷰포트 렌더링 (텍스처에)
            OnRenderViewport();

            // ImGui::Image로 결과 텍스처 표시
            if (ActiveState && ActiveState->Viewport)
            {
                ID3D11ShaderResourceView* SRV = ActiveState->Viewport->GetSRV();
                if (SRV)
                {
                    ImGui::Image((void*)SRV, ImVec2(remainingWidth, remainingHeight));
                    // ImGui의 Z-order를 고려한 정확한 hover 체크
                    ActiveState->Viewport->SetViewportHovered(ImGui::IsItemHovered());
                }
                else
                {
                    ImGui::Dummy(ImVec2(remainingWidth, remainingHeight));
                    ActiveState->Viewport->SetViewportHovered(false);
                }
            }
            else
            {
                ImGui::Dummy(ImVec2(remainingWidth, remainingHeight));
            }

            ImGui::EndChild(); // CenterPanel

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

        // Right panel - Bone Properties
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
        ImGui::BeginChild("RightPanel", ImVec2(rightWidth, totalHeight), true);
        ImGui::PopStyleVar();
        RenderRightPanel();
        ImGui::EndChild(); // RightPanel

        // Pop the ItemSpacing style
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

void SSkeletalMeshViewerWindow::PreRenderViewportUpdate()
{
    if (!ActiveState || !ActiveState->PreviewActor) return;

    // 기본 포즈 위에 사용자가 조작한 본 트랜스폼 오프셋을 적용
    // SAnimationViewerWindow는 TickComponent()가 매 프레임 포즈를 리셋하지만,
    // 여기서는 애니메이션이 없으므로 수동으로 RefPose로 리셋해야 누적을 방지할 수 있음
    if (USkeletalMeshComponent* MeshComp = ActiveState->PreviewActor->GetSkeletalMeshComponent())
    {
        // 누적 방지를 위해 먼저 참조 포즈로 리셋
        MeshComp->ResetToRefPose();

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

ViewerState* SSkeletalMeshViewerWindow::CreateViewerState(const char* Name, UEditorAssetPreviewContext* Context)
{
    ViewerState* NewState = SkeletalViewerBootstrap::CreateViewerState(Name, World, Device);
    if (!NewState) return nullptr;

    if (Context && !Context->AssetPath.empty())
    {
        LoadSkeletalMesh(NewState, Context->AssetPath);
    }
    return NewState;
}

void SSkeletalMeshViewerWindow::DestroyViewerState(ViewerState*& State)
{
    SkeletalViewerBootstrap::DestroyViewerState(State);
}

void SSkeletalMeshViewerWindow::LoadSkeletalMesh(ViewerState* State, const FString& Path)
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

        UE_LOG("SSkeletalMeshViewerWindow: Loaded skeletal mesh from %s", Path.c_str());
    }
    else
    {
        UE_LOG("SSkeletalMeshViewerWindow: Failed to load skeletal mesh from %s", Path.c_str());
    }
}

// ImGui draw callback - Direct3D 뷰포트 렌더링
void SSkeletalMeshViewerWindow::ViewportRenderCallback(const ImDrawList* parent_list, const ImDrawCmd* cmd)
{
    // UserCallbackData로 전달된 this 포인터 가져오기
    SSkeletalMeshViewerWindow* window = (SSkeletalMeshViewerWindow*)cmd->UserCallbackData;

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