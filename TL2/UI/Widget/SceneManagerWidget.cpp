#include "pch.h"
#include "SceneManagerWidget.h"
#include "../UIManager.h"
#include "../../ImGui/imgui.h"
#include "World.h"
#include "../../RenderSettings.h"
#include "../../CameraActor.h"
#include "../../CameraComponent.h"
#include "../../Actor.h"
#include "../../StaticMeshActor.h"
#include "../../SelectionManager.h"
#include "GizmoActor.h"
#include <algorithm>
#include <string>
#include <EditorEngine.h>

//// UE_LOG 대체 매크로
//#define UE_LOG(fmt, ...)

USceneManagerWidget::USceneManagerWidget()
    : UWidget("Scene Manager")
    , UIManager(&UUIManager::GetInstance())
    , SelectionManager(nullptr)
{
}

USceneManagerWidget::~USceneManagerWidget()
{
    ClearActorTree();
}

void USceneManagerWidget::Initialize()
{
    UIManager = &UUIManager::GetInstance();
    SelectionManager = nullptr;
}

void USceneManagerWidget::Update()
{
    // 지연된 새로고침 처리 (렌더링 중 iterator invalidation 방지)
    if (bNeedRefreshNextFrame)
    {
        RefreshActorTree();
        bNeedRefreshNextFrame = false;
        return; // 이번 프레임은 새로고침만 하고 끝
    }
    
    // 액터 수 변화 감지만 수행 (최소한의 검사)
    static size_t LastActorCount = 0;
    
    UWorld* World = GetCurrentWorld();
    if (World)
    {
        size_t CurrentActorCount = World->GetActors().size();
        if (CurrentActorCount != LastActorCount)
        {
            RefreshActorTree();
            LastActorCount = CurrentActorCount;
            return; // 새로고침 후 이번 프레임은 더 이상 처리하지 않음
        }
        
        // 비용이 많이 드는 유효성 검사를 5초마다만 수행
        static int32 ValidationCounter = 0;
        ValidationCounter++;
        if (ValidationCounter % 300 == 0) // 5초마다 (60FPS 기준)
        {
            // 느린 유효성 검사 - 몇 개만 샘플링
            bool bNeedRefresh = false;
            const TArray<AActor*>& WorldActors = World->GetActors();
            
            // 전체 검사 대신 몇 개만 샘플링
            int32 CheckCount = 0;
            for (auto* RootNode : RootNodes)
            {
                if (++CheckCount > 10) break; // 최대 10개만 검사
                
                if (RootNode && RootNode->IsActor())
                {
                    if (!RootNode->Actor || 
                        std::find(WorldActors.begin(), WorldActors.end(), RootNode->Actor) == WorldActors.end())
                    {
                        bNeedRefresh = true;
                        break;
                    }
                }
            }
            
            if (bNeedRefresh)
            {
                RequestDelayedRefresh();
                return;
            }
        }
    }
    else if (LastActorCount != 0)
    {
        RefreshActorTree();
        LastActorCount = 0;
    }
    
    // Sync selection from viewport
    SyncSelectionFromViewport();
    
    // 정기적으로 SelectionManager 정리 (매 프레임마다 하지 않고 가끔씩)
    static int32 CleanupCounter = 0;
    CleanupCounter++;
    if (CleanupCounter % 60 == 0) // 약 1초마다
    {
        if (UWorld* W = GetCurrentWorld())
        {
            W->GetSelectionManager()->CleanupInvalidActors();
        }
    }
}

void USceneManagerWidget::RenderWidget()
{
    ImGui::Text("Scene Manager");
    ImGui::Spacing();
    
    // Toolbar
    //RenderToolbar();
    ImGui::Separator();
    
    // World status
    UWorld* World = GetCurrentWorld();
    if (!World)
    {
        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "No World Available");
        return;
    }
    
    ImGui::Text("Objects: %zu", World->GetActors().size());
    ImGui::Separator();
    
    // Actor tree view
    ImGui::BeginChild("ActorTreeView", ImVec2(0, 240), true);
    
    // 유효성 검사는 Update()에서 처리했으므로 여기서는 바로 렌더링
    if (bNeedRefreshNextFrame)
    {
        ImGui::Text("로딩 중...");
    }
    else
    {
        // 빠른 렌더링 - 중복 검사 없이 바로 출력
        for (auto* RootNode : RootNodes)
        {
            if (RootNode)
            {
                // Categories are always shown, individual actors are filtered
                if (RootNode->IsCategory() || ShouldShowActor(RootNode->Actor))
                {
                    RenderActorNode(RootNode);
                }
            }
        }
    }
    
    ImGui::EndChild();
    
    // Context menu
    if (bShowContextMenu)
    {
        RenderContextMenu();
    }
    
    // Status bar
    ImGui::Separator();
    AActor* SelectedActor = nullptr;
    if (UWorld* W = GetCurrentWorld())
    {
        SelectedActor = W->GetSelectionManager()->GetSelectedActor();
    }
    if (SelectedActor)
    {
        // 액터 이름을 가져오기 전에 안전성 확인
        try 
        {
            ImGui::Text("Selected: %s", SelectedActor->GetName().ToString().c_str());
        }
        catch (...)
        {
            ImGui::Text("Selected: [Invalid Actor]");
            // 유효하지 않은 액터를 정리
            if (SelectionManager)
            {
                SelectionManager->CleanupInvalidActors();
            }
        }
    }
    else
    {
        ImGui::Text("No selection");
    }
}

UWorld* USceneManagerWidget::GetCurrentWorld() const
{
    if (!UIManager)
        return nullptr;
    return UIManager->GetWorld();
}

void USceneManagerWidget::RefreshActorTree()
{
    UWorld* World = GetCurrentWorld();
    if (!World)
    {
        ClearActorTree();
        return;
    }
    
    // Clear existing tree
    ClearActorTree();
    
    // Build new hierarchy
    BuildActorHierarchy();
}

void USceneManagerWidget::BuildActorHierarchy()
{
    UWorld* World = GetCurrentWorld();
    if (!World)
        return;
    
    // Build categorized hierarchy instead of flat
    BuildCategorizedHierarchy();
}

void USceneManagerWidget::RenderActorNode(FActorTreeNode* Node, int32 Depth)
{
    if (!Node)
        return;
    
    // Handle category nodes
    if (Node->IsCategory())
    {
        RenderCategoryNode(Node, Depth);
        return;
    }
    
    // Handle actor nodes
    if (!Node->Actor)
        return; // 이미 사전 검사에서 걸러졌어야 하지만 방어 코드
    
    AActor* Actor = Node->Actor;
    
    // Skip if doesn't pass filters
    if (!ShouldShowActor(Actor))
        return;
    
    ImGuiTreeNodeFlags NodeFlags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
    
    // Check if selected
    bool bIsSelected = false;
    if (UWorld* W = GetCurrentWorld())
    {
        bIsSelected = W->GetSelectionManager()->IsActorSelected(Actor);
    }
    if (bIsSelected)
    {
        NodeFlags |= ImGuiTreeNodeFlags_Selected;
    }
    
    // Leaf node if no children
    if (Node->Children.empty())
    {
        NodeFlags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
    }
    
    // Create unique ID for ImGui
    ImGui::PushID(Actor);
    
    // Sync node visibility with actual actor state each frame
    Node->bIsVisible = Actor->IsActorVisible();
    
    // Visibility toggle button (only for actors)
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
    const char* VisibilityIcon = Node->bIsVisible ? "O" : "X";
    if (ImGui::SmallButton(VisibilityIcon))
    {
        HandleActorVisibilityToggle(Actor);
    }
    ImGui::PopStyleColor();
    
    ImGui::SameLine();
    
    // Actor name and tree node

    bool bNodeOpen = ImGui::TreeNodeEx(Actor->GetName().ToString().c_str(), NodeFlags);
    
    // Handle selection
    if (ImGui::IsItemClicked())
    {
        HandleActorSelection(Actor);
    }
    if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && ImGui::IsItemHovered())
    {
        if (GEngine.GetDefaultWorld() && GEngine.GetDefaultWorld()->GetCameraActor())
        {
            ACameraActor* Cam = GEngine.GetDefaultWorld()->GetCameraActor();
            FVector Center = Actor->GetActorLocation();
            float kDist = 8.0f;
            FVector Dir = Cam->GetForward();
            if (Dir.SizeSquared() < KINDA_SMALL_NUMBER) Dir = FVector(1, 0, 0);
            FVector Pos = Center - Dir * kDist;
            Cam->SetActorLocation(Pos);
        }
        ImGui::ClearActiveID();
    }
    
    // Handle right-click context menu
    if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
    {
        ContextMenuTarget = Actor;
        bShowContextMenu = true;
        ImGui::OpenPopup("ActorContextMenu");
    }
    
    // Handle drag source
    if (ImGui::BeginDragDropSource())
    {
        ImGui::SetDragDropPayload("ACTOR_DRAG", &Actor, sizeof(AActor*));
        ImGui::Text("Move %s", Actor->GetName().ToString().c_str());
        DragSource = Actor;
        ImGui::EndDragDropSource();
    }
    
    // Handle drop target
    if (ImGui::BeginDragDropTarget())
    {
        if (const ImGuiPayload* Payload = ImGui::AcceptDragDropPayload("ACTOR_DRAG"))
        {
            AActor* DroppedActor = *(AActor**)Payload->Data;
            if (DroppedActor != Actor)
            {
                // TODO: Implement hierarchy reparenting
                UE_LOG("Would reparent %s to %s", DroppedActor->GetName().ToString().c_str(), Actor->GetName().ToString().c_str());
            }
        }
        ImGui::EndDragDropTarget();
    }
    
    // Render children if node is open
    if (bNodeOpen && !Node->Children.empty())
    {
        for (FActorTreeNode* Child : Node->Children)
        {
            RenderActorNode(Child, Depth + 1);
        }
    }
    
    if (bNodeOpen && !Node->Children.empty())
    {
        ImGui::TreePop();
    }
    
    ImGui::PopID();
}

bool USceneManagerWidget::ShouldShowActor(AActor* Actor) const
{
    if (!Actor)
        return false;
    
    // Filter by selection
    if (bShowOnlySelectedObjects && !SelectionManager->IsActorSelected(Actor))
        return false;
    
    return true;
}

void USceneManagerWidget::HandleActorSelection(AActor* Actor)
{
    if (!Actor)
        return;
    
    if (UWorld* W = GetCurrentWorld())
    {
        // Clear previous selection and select this actor
        W->GetSelectionManager()->ClearSelection();
        W->GetSelectionManager()->SelectActor(Actor);
    }
    
    // Sync with UIManager for gizmo positioning
    if (UIManager)
    {
        UIManager->SetPickedActor(Actor);
        
        if (AGizmoActor* Gizmo = GEngine.GetDefaultWorld()->GetGizmoActor())
        {
            Gizmo->SetActorLocation(Actor->GetActorLocation());
        }
    }
    
    UE_LOG("SceneManager: Selected actor %s", Actor->GetName().ToString().c_str());
}

void USceneManagerWidget::HandleActorVisibilityToggle(AActor* Actor)
{
    if (!Actor)
        return;
    
    // Toggle the actor's actual visibility state
    bool bNewVisible = Actor->GetActorHiddenInGame(); // If hidden, make visible
    Actor->SetActorHiddenInGame(!bNewVisible);
    
    // Update the node to match the actor's state
    FActorTreeNode* Node = FindNodeByActor(Actor);
    if (Node)
    {
        Node->bIsVisible = Actor->IsActorVisible();
        UE_LOG("SceneManager: Toggled visibility for %s: %s", 
               Actor->GetName().ToString().c_str(), Node->bIsVisible ? "Visible" : "Hidden");
    }
}

void USceneManagerWidget::HandleActorRename(AActor* Actor)
{
    // TODO: Implement inline renaming
    UE_LOG("SceneManager: Rename not implemented yet");
}

void USceneManagerWidget::HandleActorDelete(AActor* Actor)
{
    if (!Actor)
        return;
    
    UWorld* World = GetCurrentWorld();
    if (World)
    {
        World->DestroyActor(Actor);
        UE_LOG("SceneManager: Deleted actor %s", Actor->GetName().ToString().c_str());
    }
}

void USceneManagerWidget::HandleActorDuplicate(AActor* Actor)
{
    // TODO: Implement actor duplication
    UE_LOG("SceneManager: Duplicate not implemented yet");
}

void USceneManagerWidget::RenderContextMenu()
{
    if (ImGui::BeginPopup("ActorContextMenu"))
    {
        if (ContextMenuTarget)
        {
            ImGui::Text("Actor: %s", ContextMenuTarget->GetName().ToString().c_str());
            const char* ClassName = "Unknown";
            if (ContextMenuTarget->GetClass())
            {
                ClassName = ContextMenuTarget->GetClass()->Name;
            }
            ImGui::Text("Type: %s", ClassName);
            ImGui::Separator();
            
            // Selection actions
            if (ImGui::MenuItem("Focus in Viewport"))
            {
                HandleActorSelection(ContextMenuTarget);
            }
            
            ImGui::Separator();
            
            // Transform actions
            if (ImGui::MenuItem("Reset Transform"))
            {
                if (ContextMenuTarget)
                {
                    ContextMenuTarget->SetActorLocation(FVector(0, 0, 0));
                    ContextMenuTarget->SetActorRotation(FQuat::Identity());
                    ContextMenuTarget->SetActorScale(FVector(1, 1, 1));
                }
            }
            
            if (ImGui::MenuItem("Reset Position"))
            {
                if (ContextMenuTarget)
                {
                    ContextMenuTarget->SetActorLocation(FVector(0, 0, 0));
                }
            }
            
            ImGui::Separator();
            
            // Edit actions
            if (ImGui::MenuItem("Rename"))
            {
                HandleActorRename(ContextMenuTarget);
            }
            
            if (ImGui::MenuItem("Duplicate"))
            {
                HandleActorDuplicate(ContextMenuTarget);
            }
            
            ImGui::Separator();
            
            // Danger zone
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
            if (ImGui::MenuItem("Delete"))
            {
                HandleActorDelete(ContextMenuTarget);
            }
            ImGui::PopStyleColor();
        }
        
        ImGui::EndPopup();
    }
    else
    {
        bShowContextMenu = false;
        ContextMenuTarget = nullptr;
    }
}

void USceneManagerWidget::RenderToolbar()
{
    // Filter toggles
    ImGui::Checkbox("Selected Only", &bShowOnlySelectedObjects);
    ImGui::SameLine();
    ImGui::Checkbox("Show Hidden", &bShowHiddenObjects);
    
    // View Mode selection
    ImGui::Text("View Mode:");
    ImGui::SameLine();
    
    const char* ViewModeNames[] = { "Lit", "Unlit", "Wireframe" };
    
    UWorld* World = GetCurrentWorld();
    if (World)
    {
        // Convert enum value to UI index (subtract 1 because enum starts with None=0)
EViewModeIndex CurrentEnum = World->GetRenderSettings().GetViewModeIndex();
        int CurrentViewMode = 0; // Default to Lit
        
        switch (CurrentEnum)
        {
        case EViewModeIndex::VMI_Lit:
            CurrentViewMode = 0;
            break;
        case EViewModeIndex::VMI_Unlit:
            CurrentViewMode = 1;
            break;
        case EViewModeIndex::VMI_Wireframe:
            CurrentViewMode = 2;
            break;
        default:
            CurrentViewMode = 0;
            break;
        }
        
        if (ImGui::Combo("##ViewMode", &CurrentViewMode, ViewModeNames, IM_ARRAYSIZE(ViewModeNames)))
        {
            // Convert UI index back to enum value
            EViewModeIndex NewEnum = EViewModeIndex::VMI_Lit;
            switch (CurrentViewMode)
            {
            case 0:
                NewEnum = EViewModeIndex::VMI_Lit;
                break;
            case 1:
                NewEnum = EViewModeIndex::VMI_Unlit;
                break;
            case 2:
                NewEnum = EViewModeIndex::VMI_Wireframe;
                break;
            }
World->GetRenderSettings().SetViewModeIndex(NewEnum);
        }
    }
    else
    {
        // Disabled combo when no world is available
        ImGui::BeginDisabled();
        int DummyViewMode = 0;
        ImGui::Combo("##ViewMode", &DummyViewMode, ViewModeNames, IM_ARRAYSIZE(ViewModeNames));
        ImGui::EndDisabled();
    }
    
    if (ImGui::Button("Refresh"))
    {
        RefreshActorTree();
    }
    
    ImGui::SameLine();
    if (ImGui::Button("Expand All"))
    {
        ExpandAllCategories();
    }
    
    ImGui::SameLine();
    if (ImGui::Button("Collapse All"))
    {
        CollapseAllCategories();
    }
}

void USceneManagerWidget::ClearActorTree()
{
    for (auto* Node : RootNodes)
    {
        delete Node;
    }
    RootNodes.clear();
}

USceneManagerWidget::FActorTreeNode* USceneManagerWidget::FindNodeByActor(AActor* Actor)
{
    if (!Actor)
        return nullptr;
    
    // Search through categories and their children
    for (FActorTreeNode* RootNode : RootNodes)
    {
        if (!RootNode)
            continue;
            
        // Check if this root node is the actor (for backward compatibility)
        if (RootNode->IsActor() && RootNode->Actor == Actor)
            return RootNode;
            
        // Search within category children
        if (RootNode->IsCategory())
        {
            for (FActorTreeNode* Child : RootNode->Children)
            {
                if (Child && Child->IsActor() && Child->Actor == Actor)
                    return Child;
            }
        }
    }
    
    return nullptr;
}

void USceneManagerWidget::SyncSelectionFromViewport()
{
    // SelectionManager에서 null 액터들을 정리
    if (UWorld* W = GetCurrentWorld())
    {
        W->GetSelectionManager()->CleanupInvalidActors();
    }
    
    // 선택된 액터가 null인지 확인
    AActor* SelectedActor = nullptr;
    if (UWorld* W = GetCurrentWorld())
    {
        SelectedActor = W->GetSelectionManager()->GetSelectedActor();
    }
    if (!SelectedActor)
    {
        // 선택된 액터가 null이면 UI를 업데이트
        if (UIManager)
        {
            UIManager->ResetPickedActor();
        }
    }
}

void USceneManagerWidget::SyncSelectionToViewport(AActor* Actor)
{
    // Selection is already handled via SelectionManager
    // The 3D viewport will automatically respond to selection changes
}

// Category Management Implementation
FString USceneManagerWidget::GetActorCategory(AActor* Actor) const
{
    if (!Actor)
        return "Unknown";
    
    FString ActorName = Actor->GetName().ToString();
    
    // Extract category from actor name (assumes format: "Type_Number")
    size_t UnderscorePos = ActorName.find('_');
    if (UnderscorePos != std::string::npos)
    {
        return ActorName.substr(0, UnderscorePos);
    }
    
    // Fallback: use the full name as category if no underscore found
    return ActorName;
}

USceneManagerWidget::FActorTreeNode* USceneManagerWidget::FindOrCreateCategoryNode(const FString& CategoryName)
{
    // Look for existing category node in root nodes
    for (FActorTreeNode* Node : RootNodes)
    {
        if (Node && Node->IsCategory() && Node->CategoryName == CategoryName)
        {
            return Node;
        }
    }
    
    // Create new category node if not found
    FActorTreeNode* CategoryNode = new FActorTreeNode(CategoryName);
    RootNodes.push_back(CategoryNode);
    return CategoryNode;
}

void USceneManagerWidget::BuildCategorizedHierarchy()
{
    UWorld* World = GetCurrentWorld();
    if (!World)
        return;
    
    const TArray<AActor*>& Actors = World->GetActors();
    
    // Group actors by category
    for (AActor* Actor : Actors)
    {
        if (!Actor)
            continue;
            
        FString CategoryName = GetActorCategory(Actor);
        FActorTreeNode* CategoryNode = FindOrCreateCategoryNode(CategoryName);
        
        // Create actor node and add to category
        FActorTreeNode* ActorNode = new FActorTreeNode(Actor);
        ActorNode->Parent = CategoryNode;
        // Initialize node visibility from actor's actual visibility state
        ActorNode->bIsVisible = Actor->IsActorVisible();
        CategoryNode->Children.push_back(ActorNode);
    }
    
    // Initialize category visibility based on child actors
    for (FActorTreeNode* CategoryNode : RootNodes)
    {
        if (CategoryNode && CategoryNode->IsCategory())
        {
            // Category is visible if any child is visible
            bool bAnyCategoryChildVisible = false;
            for (FActorTreeNode* Child : CategoryNode->Children)
            {
                if (Child && Child->bIsVisible)
                {
                    bAnyCategoryChildVisible = true;
                    break;
                }
            }
            CategoryNode->bIsVisible = bAnyCategoryChildVisible;
        }
    }
}

void USceneManagerWidget::HandleCategorySelection(FActorTreeNode* CategoryNode)
{
    if (!CategoryNode || !CategoryNode->IsCategory())
        return;
        
    // Toggle category expansion
    CategoryNode->bIsExpanded = !CategoryNode->bIsExpanded;
    
    UE_LOG("SceneManager: Toggled category %s: %s", 
           CategoryNode->CategoryName.c_str(), 
           CategoryNode->bIsExpanded ? "Expanded" : "Collapsed");
}

void USceneManagerWidget::HandleCategoryVisibilityToggle(FActorTreeNode* CategoryNode)
{
    if (!CategoryNode || !CategoryNode->IsCategory())
        return;
        
    // Toggle category visibility
    CategoryNode->bIsVisible = !CategoryNode->bIsVisible;
    
    // Apply visibility to all child actors
    for (FActorTreeNode* Child : CategoryNode->Children)
    {
        if (Child && Child->IsActor() && Child->Actor)
        {
            Child->Actor->SetActorHiddenInGame(!CategoryNode->bIsVisible);
            Child->bIsVisible = CategoryNode->bIsVisible;
        }
    }
    
    UE_LOG("SceneManager: Toggled category visibility %s: %s", 
           CategoryNode->CategoryName.c_str(), 
           CategoryNode->bIsVisible ? "Visible" : "Hidden");
}

void USceneManagerWidget::RenderCategoryNode(FActorTreeNode* CategoryNode, int32 Depth)
{
    if (!CategoryNode || !CategoryNode->IsCategory())
        return;
        
    ImGuiTreeNodeFlags NodeFlags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
    
    // Categories are always expandable
    if (!CategoryNode->Children.empty())
    {
        if (CategoryNode->bIsExpanded)
        {
            NodeFlags |= ImGuiTreeNodeFlags_DefaultOpen;
        }
    }
    else
    {
        // Empty category - show as leaf
        NodeFlags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
    }
    
    // Create unique ID for ImGui using category name
    ImGui::PushID(CategoryNode->CategoryName.c_str());
    
    // Category visibility toggle button
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
    const char* VisibilityIcon = CategoryNode->bIsVisible ? "O" : "X";
    if (ImGui::SmallButton(VisibilityIcon))
    {
        HandleCategoryVisibilityToggle(CategoryNode);
    }
    ImGui::PopStyleColor();
    
    ImGui::SameLine();
    
    // Category name with object count
    FString DisplayText = CategoryNode->CategoryName + " (" + std::to_string(CategoryNode->Children.size()) + ")";
    bool bNodeOpen = ImGui::TreeNodeEx(DisplayText.c_str(), NodeFlags);
    
    // Handle category click
    if (ImGui::IsItemClicked())
    {
        HandleCategorySelection(CategoryNode);
    }
    
    // Update expansion state based on ImGui tree state
    CategoryNode->bIsExpanded = bNodeOpen;
    
    // Render child actors if category is expanded
    if (bNodeOpen && !CategoryNode->Children.empty())
    {
        for (FActorTreeNode* Child : CategoryNode->Children)
        {
            RenderActorNode(Child, Depth + 1);
        }
        ImGui::TreePop();
    }
    
    ImGui::PopID();
}

void USceneManagerWidget::ExpandAllCategories()
{
    for (FActorTreeNode* RootNode : RootNodes)
    {
        if (RootNode && RootNode->IsCategory())
        {
            RootNode->bIsExpanded = true;
        }
    }
    UE_LOG("SceneManager: Expanded all categories");
}

void USceneManagerWidget::CollapseAllCategories()
{
    for (FActorTreeNode* RootNode : RootNodes)
    {
        if (RootNode && RootNode->IsCategory())
        {
            RootNode->bIsExpanded = false;
        }
    }
    UE_LOG("SceneManager: Collapsed all categories");
}

FString USceneManagerWidget::FActorTreeNode::GetDisplayName() const
{
    if (IsCategory()) return CategoryName;
    if (IsActor() && Actor) return Actor->GetName().ToString();
    return "Unknown";
}
