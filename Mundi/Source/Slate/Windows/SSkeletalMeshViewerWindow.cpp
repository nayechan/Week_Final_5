#include "pch.h"
#include "SSkeletalMeshViewerWindow.h"
#include "FViewport.h"
#include "FViewportClient.h"
#include "Source/Runtime/Engine/SkeletalViewer/SkeletalViewerBootstrap.h"
#include "Source/Editor/PlatformProcess.h"
#include "Source/Runtime/Engine/GameFramework/SkinnedMeshActor.h"
#include "Source/Runtime/Engine/Components/LineComponent.h"
#include "SelectionManager.h"
#include "BoneAnchorComponent.h"

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

bool SSkeletalMeshViewerWindow::Initialize(float StartX, float StartY, float Width, float Height, UWorld* InWorld, ID3D11Device* InDevice)
{
    World = InWorld;
    Device = InDevice;

    SetRect(StartX, StartY, StartX + Width, StartY + Height);

    // Create first tab/state
    OpenNewTab("Viewer 1");
    if (ActiveState && ActiveState->Viewport)
    {
        ActiveState->Viewport->Resize((uint32)StartX, (uint32)StartY, (uint32)Width, (uint32)Height);
    }

    bRequestFocus = true;
    return true;
}

void SSkeletalMeshViewerWindow::OnRender()
{
    // Parent detachable window (movable, top-level) with transparent background so 3D shows through
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBackground;

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
    bool bViewerVisible = false;
    if (ImGui::Begin("Skeletal Mesh Viewer", nullptr, flags))
    {
        bViewerVisible = true;
        // Render tab bar and switch active state
        if (ImGui::BeginTabBar("SkeletalViewerTabs", ImGuiTabBarFlags_AutoSelectNewTabs | ImGuiTabBarFlags_Reorderable))
        {
            for (int i = 0; i < Tabs.Num(); ++i)
            {
                ViewerState* State = Tabs[i];
                bool open = true;
                if (ImGui::BeginTabItem(State->Name.c_str(), &open))
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
        ImVec2 pos = ImGui::GetWindowPos();
        ImVec2 size = ImGui::GetWindowSize();
        Rect.Left = pos.x; Rect.Top = pos.y; Rect.Right = pos.x + size.x; Rect.Bottom = pos.y + size.y; Rect.UpdateMinMax();

        ImVec2 contentAvail = ImGui::GetContentRegionAvail();
        float totalWidth = contentAvail.x;
        float totalHeight = contentAvail.y;

        float leftWidth = totalWidth * LeftPanelRatio;
        float rightWidth = totalWidth * RightPanelRatio;
        float centerWidth = totalWidth - leftWidth - rightWidth;

        // Left panel
        ImGui::BeginChild("LeftPanel", ImVec2(leftWidth, totalHeight), true);
        if (ActiveState)
        {
            ImGui::Text("Load Skeletal Mesh");
            ImGui::Separator();

            // Path input + browse
            ImGui::PushItemWidth(-100.0f);
            ImGui::InputTextWithHint("##MeshPath", "Data/Model/YourMesh.fbx", ActiveState->MeshPathBuffer, sizeof(ActiveState->MeshPathBuffer));
            ImGui::PopItemWidth();
            ImGui::SameLine();
            if (ImGui::Button("Browse"))
            {
                auto widePath = FPlatformProcess::OpenLoadFileDialog(UTF8ToWide(GDataDir), L"fbx", L"FBX Files");
                if (!widePath.empty())
                {
                    std::string s = widePath.string();
                    strncpy_s(ActiveState->MeshPathBuffer, s.c_str(), sizeof(ActiveState->MeshPathBuffer) - 1);
                }
            }

            // [메시 로드 UI]
            // - 입력된 경로(또는 Browse로 선택된 경로)를 사용해 스켈레탈 메시를 로드하고
            //   프리뷰 액터에 바로 세팅합니다. 이후 본 오버레이는 다음 프레임에 재구성됩니다.
            if (ImGui::Button("Load FBX"))
            {
                FString Path = ActiveState->MeshPathBuffer;
                if (!Path.empty())
                {
                    USkeletalMesh* Mesh = UResourceManager::GetInstance().Load<USkeletalMesh>(Path);
                    if (Mesh && ActiveState->PreviewActor)
                    {
                        ActiveState->PreviewActor->SetSkeletalMesh(Path);
                        ActiveState->CurrentMesh = Mesh;
                        // 메시 표시에 대한 체크박스 상태와 동기화
                        if (auto* Skinned = ActiveState->PreviewActor->GetSkinnedMeshComponent())
                        {
                            Skinned->SetVisibility(ActiveState->bShowMesh);
                        }
                        // 새 메시 로드시 본 라인 재구축 요청
                        ActiveState->bBoneLinesDirty = true;
                        if (auto* LineComp = ActiveState->PreviewActor->GetBoneLineComponent())
                        {
                            // 기존 선 데이터를 지우고(화면 잔상 방지), 현재 토글 상태에 맞춰 가시성을 동기화합니다.
                            LineComp->ClearLines();
                            LineComp->SetLineVisible(ActiveState->bShowBones);
                        }
                    }
                }
            }
        }
        ImGui::EndChild();

        ImGui::SameLine();

        // Center panel (viewport area) — draw with no background so viewport remains visible
        ImGui::BeginChild("SkeletalMeshViewport", ImVec2(centerWidth, totalHeight), false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBackground);
        ImVec2 childPos = ImGui::GetWindowPos();
        ImVec2 childSize = ImGui::GetWindowSize();
        ImVec2 rectMin = childPos;
        ImVec2 rectMax(childPos.x + childSize.x, childPos.y + childSize.y);
        CenterRect.Left = rectMin.x; CenterRect.Top = rectMin.y; CenterRect.Right = rectMax.x; CenterRect.Bottom = rectMax.y; CenterRect.UpdateMinMax();
        ImGui::EndChild();

        ImGui::SameLine();

        // Right panel
        ImGui::BeginChild("RightPanel", ImVec2(rightWidth, totalHeight), true);
        ImGui::Text("Skeleton Tree");
        
        ImGui::Separator();
        
        if (ImGui::Checkbox("Show Mesh", &ActiveState->bShowMesh))
        {
            if (ActiveState->PreviewActor && ActiveState->PreviewActor->GetSkinnedMeshComponent())
            {
                ActiveState->PreviewActor->GetSkinnedMeshComponent()->SetVisibility(ActiveState->bShowMesh);
            }
        }
        
        ImGui::SameLine();
        // [본 표시 토글]
        // - 라인 컴포넌트의 Visibility를 직접 토글하여 불필요한 드로우를 줄입니다.
        if (ImGui::Checkbox("Show Bones", &ActiveState->bShowBones))
        {
            if (ActiveState->PreviewActor && ActiveState->PreviewActor->GetBoneLineComponent())
            {
                ActiveState->PreviewActor->GetBoneLineComponent()->SetLineVisible(ActiveState->bShowBones);
            }
            if (ActiveState->bShowBones)
            {
                ActiveState->bBoneLinesDirty = true; // 표시 켜질 때 재구축
            }
        }

        ImGui::Separator();
        
        if (!ActiveState->CurrentMesh)
        {
            ImGui::TextDisabled("No skeletal mesh loaded");
        }
        else
        {
            const FSkeleton* Skeleton = ActiveState->CurrentMesh->GetSkeleton();
            if (!Skeleton || Skeleton->Bones.IsEmpty())
            {
                ImGui::TextDisabled("Mesh has no bones");
            }
            else
            {
                const TArray<FBone>& Bones = Skeleton->Bones;
                // Build children adjacency per frame (cheap for typical skeleton sizes)
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

                // Recursive lambda to draw a node and its children
                std::function<void(int32)> DrawNode = [&](int32 Index)
                {
                    const bool bLeaf = Children[Index].IsEmpty();
                    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanFullWidth;
                    if (bLeaf)
                    {
                        flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
                    }
                    if (ActiveState->SelectedBoneIndex == Index)
                    {
                        flags |= ImGuiTreeNodeFlags_Selected;
                    }

                    ImGui::PushID(Index);
                    const char* Label = Bones[Index].Name.c_str();
                    bool open = ImGui::TreeNodeEx((void*)(intptr_t)Index, flags, "%s", Label ? Label : "<noname>");
                    if (ImGui::IsItemClicked())
                    {
                        if (ActiveState->SelectedBoneIndex != Index)
                        {
                            ActiveState->SelectedBoneIndex = Index;
                            ActiveState->bBoneLinesDirty = true; // 색상 갱신 필요

                            // Move gizmo to the selected bone and update selection
                            if (ActiveState->PreviewActor && ActiveState->World)
                            {
                                ActiveState->PreviewActor->MoveGizmoToBone(Index);
                                if (USceneComponent* Anchor = ActiveState->PreviewActor->GetBoneGizmoAnchor())
                                {
                                    // Ensure actor is selected so the component selection sticks
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

                // Draw all roots (ParentIndex == -1)
                for (int32 i = 0; i < Bones.size(); ++i)
                {
                    if (Bones[i].ParentIndex < 0)
                    {
                        DrawNode(i);
                    }
                }
            }
        }
        ImGui::EndChild();
    }
    ImGui::End();

    // If collapsed or not visible, clear the center rect so we don't render a floating viewport
    if (!bViewerVisible)
    {
        CenterRect = FRect(0, 0, 0, 0);
        CenterRect.UpdateMinMax();
    }

    bRequestFocus = false;

    if (bViewerVisible && ActiveState && ActiveState->Viewport)
    {
        // Ensure viewport matches current center rect before rendering
        const uint32 NewStartX = static_cast<uint32>(CenterRect.Left);
        const uint32 NewStartY = static_cast<uint32>(CenterRect.Top);
        const uint32 NewWidth  = static_cast<uint32>(CenterRect.Right - CenterRect.Left);
        const uint32 NewHeight = static_cast<uint32>(CenterRect.Bottom - CenterRect.Top);
        ActiveState->Viewport->Resize(NewStartX, NewStartY, NewWidth, NewHeight);

        // [본 오버레이 재구축]
        // - 본 라인은 편집 중에도 매 프레임 갱신되어야 시각적으로 즉시 반영됨
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
            ActiveState->bBoneLinesDirty = false; // 한 번만 재구성
        }

        ActiveState->Viewport->Render();
    }
    else if (ActiveState && ActiveState->Viewport)
    {
        // When not visible, ensure the viewport doesn't keep stale content
        ActiveState->Viewport->Resize(0, 0, 0, 0);
    }
}

void SSkeletalMeshViewerWindow::OnUpdate(float DeltaSeconds)
{
    if (!ActiveState || !ActiveState->Viewport)
        return;

    // Tick the preview world so editor actors (e.g., gizmo) update visibility/state
    if (ActiveState->World)
    {
        ActiveState->World->Tick(DeltaSeconds);
    }

    if (ActiveState && ActiveState->Client)
    {
        ActiveState->Client->Tick(DeltaSeconds);
    }
}

void SSkeletalMeshViewerWindow::OnMouseMove(FVector2D MousePos)
{
    if (!ActiveState || !ActiveState->Viewport) return;

    if (CenterRect.Contains(MousePos))
    {
        FVector2D LocalPos = MousePos - FVector2D(CenterRect.Left, CenterRect.Top);
        ActiveState->Viewport->ProcessMouseMove((int32)LocalPos.X, (int32)LocalPos.Y);
    }
}

void SSkeletalMeshViewerWindow::OnMouseDown(FVector2D MousePos, uint32 Button)
{
    if (!ActiveState || !ActiveState->Viewport) return;

    if (CenterRect.Contains(MousePos))
    {
        FVector2D LocalPos = MousePos - FVector2D(CenterRect.Left, CenterRect.Top);
        ActiveState->Viewport->ProcessMouseButtonDown((int32)LocalPos.X, (int32)LocalPos.Y, (int32)Button);
    }
}

void SSkeletalMeshViewerWindow::OnMouseUp(FVector2D MousePos, uint32 Button)
{
    if (!ActiveState || !ActiveState->Viewport) return;

    if (CenterRect.Contains(MousePos))
    {
        FVector2D LocalPos = MousePos - FVector2D(CenterRect.Left, CenterRect.Top);
        ActiveState->Viewport->ProcessMouseButtonUp((int32)LocalPos.X, (int32)LocalPos.Y, (int32)Button);
    }
}

void SSkeletalMeshViewerWindow::OpenNewTab(const char* Name)
{
    ViewerState* State = SkeletalViewerBootstrap::CreateViewerState(Name, World, Device);
    if (!State) return;
    Tabs.Add(State);
    ActiveTabIndex = Tabs.Num() - 1;
    ActiveState = State;
}

void SSkeletalMeshViewerWindow::CloseTab(int Index)
{
    if (Index < 0 || Index >= Tabs.Num()) return;
    ViewerState* State = Tabs[Index];
    SkeletalViewerBootstrap::DestroyViewerState(State);
    Tabs.RemoveAt(Index);
    if (Tabs.Num() == 0) { ActiveTabIndex = -1; ActiveState = nullptr; }
    else { ActiveTabIndex = std::min(Index, Tabs.Num() - 1); ActiveState = Tabs[ActiveTabIndex]; }
}
