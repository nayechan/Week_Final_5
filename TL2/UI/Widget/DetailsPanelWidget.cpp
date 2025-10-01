#include "pch.h"
#include "DetailsPanelWidget.h"
#include "../UIManager.h"
#include "../../ImGui/imgui.h"
#include "../../Actor.h"
#include "../../ActorComponent.h"
#include "../../StaticMeshComponent.h"
#include "../../CameraComponent.h"
#include "../../SelectionManager.h"
#include <algorithm>
#include <string>

// UE_LOG 대체 매크로
#define UE_LOG(fmt, ...)

UDetailsPanelWidget::UDetailsPanelWidget()
    : UWidget("Details")
    , UIManager(&UUIManager::GetInstance())
    , SelectionManager(nullptr)
{
}

UDetailsPanelWidget::~UDetailsPanelWidget()
{
}

void UDetailsPanelWidget::Initialize()
{
    UIManager = &UUIManager::GetInstance();
    SelectionManager = nullptr;
}

void UDetailsPanelWidget::Update()
{
    CheckSelectionChanged();
}

void UDetailsPanelWidget::RenderWidget()
{
    ImGui::Text("Details Panel");
    ImGui::Spacing();
    
    // Check if we have a selection
    AActor* SelectedActor = nullptr;
    if (UIManager && UIManager->GetWorld())
    {
        SelectedActor = UIManager->GetWorld()->GetSelectionManager().GetSelectedActor();
    }
    if (!SelectedActor)
    {
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "No object selected");
        ImGui::Text("Select an object in the viewport or World Outliner");
        ImGui::Text("to view and edit its properties.");
        return;
    }
    
    // Render actor details
    RenderActorDetails(SelectedActor);
}

void UDetailsPanelWidget::CheckSelectionChanged()
{
    AActor* NewSelection = nullptr;
    if (UIManager && UIManager->GetWorld())
    {
        NewSelection = UIManager->GetWorld()->GetSelectionManager().GetSelectedActor();
    }
    if (NewSelection != CurrentSelectedActor)
    {
        OnSelectionChanged(NewSelection);
    }
}

void UDetailsPanelWidget::OnSelectionChanged(AActor* NewSelection)
{
    CurrentSelectedActor = NewSelection;
    bNeedsRefresh = true;
    
    if (NewSelection)
    {
        UE_LOG("DetailsPanel: Selection changed to %s", NewSelection->GetName().c_str());
    }
    else
    {
        UE_LOG("DetailsPanel: Selection cleared");
    }
}

void UDetailsPanelWidget::RenderActorDetails(AActor* Actor)
{
    if (!Actor)
        return;
    
    ImGui::PushID(Actor);
    
    // Actor header with name and class
    ImGui::PushStyleColor(ImGuiCol_Header, GetCategoryHeaderColor());
    ImGui::Text("🎭 %s", Actor->GetName().c_str());
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "(%s)", Actor->GetClass()->Name);
    ImGui::PopStyleColor();
    
    ImGui::Separator();
    ImGui::Spacing();
    
    // Transform section
    if (BeginCategory("Transform"))
    {
        RenderTransformSection(Actor);
        EndCategory();
    }
    
    // General properties
    if (BeginCategory("General"))
    {
        RenderGeneralSection(Actor);
        EndCategory();
    }
    
    // Components section
    if (BeginCategory("Components"))
    {
        RenderComponentsSection(Actor);
        EndCategory();
    }
    
    ImGui::PopID();
}

void UDetailsPanelWidget::RenderTransformSection(AActor* Actor)
{
    if (!Actor)
        return;
    
    // Get current transform
    FTransform CurrentTransform = Actor->GetActorTransform();
    FVector Location = CurrentTransform.Location;
    FQuat Rotation = CurrentTransform.Rotation;
    FVector Scale = CurrentTransform.Scale;
    
    // Store original values for comparison
    FVector OriginalLocation = Location;
    FQuat OriginalRotation = Rotation;
    FVector OriginalScale = Scale;
    
    // Location
    RenderVectorProperty("Location", Location, 0.1f);
    
    // Rotation (as Euler angles)
    RenderQuaternionAsEuler("Rotation", Rotation);
    
    // Scale
    RenderVectorProperty("Scale", Scale, 0.01f);
    
    // Apply changes if any
    bool bChanged = false;
    if (Location != OriginalLocation || Rotation != OriginalRotation || Scale != OriginalScale)
    {
        FTransform NewTransform(Location, Rotation, Scale);
        Actor->SetActorTransform(NewTransform);
        bChanged = true;
    }
    
    // Reset buttons
    ImGui::Spacing();
    if (ImGui::Button("Reset Location"))
    {
        Actor->SetActorLocation(FVector(0, 0, 0));
        bChanged = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset Rotation"))
    {
        Actor->SetActorRotation(FQuat::Identity());
        bChanged = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset Scale"))
    {
        Actor->SetActorScale(FVector(1, 1, 1));
        bChanged = true;
    }
    
    if (bChanged)
    {
        UE_LOG("DetailsPanel: Transform changed for %s", Actor->GetName().c_str());
    }
}

void UDetailsPanelWidget::RenderGeneralSection(AActor* Actor)
{
    if (!Actor)
        return;
    
    // Actor Name (editable)
    FString ActorName = Actor->GetName();
    RenderStringProperty("Name", ActorName);
    if (ActorName != Actor->GetName())
    {
        Actor->SetName(ActorName);
    }
    
    // Object ID (read-only)
    char UUIDBuffer[32];
    sprintf_s(UUIDBuffer, "%u", Actor->UUID);
    RenderReadOnlyProperty("Object ID", UUIDBuffer);
    
    // Class name (read-only)
    RenderReadOnlyProperty("Class", Actor->GetClass()->Name);
    
    // World matrix info (read-only, for debugging)
    FMatrix WorldMatrix = Actor->GetWorldMatrix();
    char MatrixBuffer[256];
    sprintf_s(MatrixBuffer, "[%.2f, %.2f, %.2f, %.2f]", 
             WorldMatrix.M[0][0], WorldMatrix.M[0][1], WorldMatrix.M[0][2], WorldMatrix.M[0][3]);
    RenderReadOnlyProperty("World Matrix Row 0", MatrixBuffer);
}

void UDetailsPanelWidget::RenderComponentsSection(AActor* Actor)
{
    if (!Actor)
        return;
    
    const TArray<USceneComponent*>& Components = Actor->GetComponents();
    
    if (Components.empty())
    {
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "No components");
        return;
    }
    
    for (int32 i = 0; i < Components.size(); ++i)
    {
        USceneComponent* Component = Components[i];
        if (!Component)
            continue;
        
        ImGui::PushID(i);
        
        // Component header
        const char* ComponentTypeName = GetComponentTypeName(Component);
        bool bIsRootComponent = (Component == Actor->RootComponent);
        
        ImGuiTreeNodeFlags NodeFlags = ImGuiTreeNodeFlags_DefaultOpen;
        
        FString ComponentLabel = FString(ComponentTypeName);
        if (bIsRootComponent)
        {
            ComponentLabel += " (Root)";
        }
        
        bool bNodeOpen = ImGui::TreeNodeEx(ComponentLabel.c_str(), NodeFlags);
        
        if (bNodeOpen)
        {
            RenderComponentDetails(Component);
            ImGui::TreePop();
        }
        
        ImGui::PopID();
    }
}

void UDetailsPanelWidget::RenderComponentDetails(UActorComponent* Component)
{
    if (!Component)
        return;
    
    // Component UUID
    char ComponentIDBuffer[32];
    sprintf_s(ComponentIDBuffer, "%u", Component->UUID);
    RenderReadOnlyProperty("Component ID", ComponentIDBuffer);
    
    // Active state
    bool bIsActive = Component->IsActive();
    RenderBoolProperty("Active", bIsActive);
    if (bIsActive != Component->IsActive())
    {
        Component->SetActive(bIsActive);
    }
    
    // Can Ever Tick
    bool bCanTick = Component->CanEverTick();
    RenderBoolProperty("Can Tick", bCanTick);
    if (bCanTick != Component->CanEverTick())
    {
        Component->SetTickEnabled(bCanTick);
    }
    
    // Type-specific properties
    if (UStaticMeshComponent* MeshComp = Cast<UStaticMeshComponent>(Component))
    {
        RenderStaticMeshComponentProperties(MeshComp);
    }
    else if (UCameraComponent* CameraComp = Cast<UCameraComponent>(Component))
    {
        RenderCameraComponentProperties(CameraComp);
    }
}

const char* UDetailsPanelWidget::GetComponentTypeName(UActorComponent* Component)
{
    if (!Component)
        return "Unknown";
    
    return Component->GetClass()->Name;
}

void UDetailsPanelWidget::RenderStaticMeshComponentProperties(UStaticMeshComponent* MeshComp)
{
    if (!MeshComp)
        return;
    
    RenderPropertySeparator();
    ImGui::Text("Static Mesh Properties:");
    
    // Mesh asset info
    if (auto* StaticMesh = MeshComp->GetStaticMesh())
    {
        RenderReadOnlyProperty("Mesh Asset", StaticMesh->GetFilePath().c_str());
    }
    else
    {
        RenderReadOnlyProperty("Mesh Asset", "None");
    }
    
    // Material info (simplified)
    RenderReadOnlyProperty("Material", "Default Material");
}

void UDetailsPanelWidget::RenderCameraComponentProperties(UCameraComponent* CameraComp)
{
    if (!CameraComp)
        return;
    
    RenderPropertySeparator();
    ImGui::Text("Camera Properties:");
    
    // FOV, Near/Far plane, etc. would go here
    RenderReadOnlyProperty("Field of View", "90.0");
    RenderReadOnlyProperty("Near Plane", "0.1");
    RenderReadOnlyProperty("Far Plane", "1000.0");
}

bool UDetailsPanelWidget::BeginCategory(const FString& CategoryName, bool bDefaultExpanded)
{
    ImGui::Spacing();
    
    ImGuiTreeNodeFlags NodeFlags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed;
    if (!bDefaultExpanded)
    {
        NodeFlags &= ~ImGuiTreeNodeFlags_DefaultOpen;
    }
    
    ImGui::PushStyleColor(ImGuiCol_Header, GetCategoryHeaderColor());
    bool bIsOpen = ImGui::TreeNodeEx(CategoryName.c_str(), NodeFlags);
    ImGui::PopStyleColor();
    
    if (bIsOpen)
    {
        ImGui::Indent();
    }
    
    return bIsOpen;
}

void UDetailsPanelWidget::EndCategory()
{
    ImGui::Unindent();
    ImGui::TreePop();
}

void UDetailsPanelWidget::RenderVectorProperty(const char* Label, FVector& Value, float Speed, const char* Format)
{
    ImGui::Text(Label);
    ImGui::SameLine(120); // Align values
    ImGui::SetNextItemWidth(-1);
    
    float Values[3] = { Value.X, Value.Y, Value.Z };
    if (ImGui::DragFloat3(("##" + std::string(Label)).c_str(), Values, Speed, 0.0f, 0.0f, Format))
    {
        Value = FVector(Values[0], Values[1], Values[2]);
    }
}

void UDetailsPanelWidget::RenderQuaternionAsEuler(const char* Label, FQuat& Quat)
{
    // Convert quaternion to Euler angles for editing
    FVector EulerDegrees = Quat.ToEuler() * (180.0f / 3.14159f); // Convert to degrees
    
    ImGui::Text(Label);
    ImGui::SameLine(120);
    ImGui::SetNextItemWidth(-1);
    
    float Values[3] = { EulerDegrees.X, EulerDegrees.Y, EulerDegrees.Z };
    if (ImGui::DragFloat3(("##" + std::string(Label)).c_str(), Values, 1.0f, -180.0f, 180.0f, "%.1f°"))
    {
        // Convert back to quaternion
        FVector NewEulerRad = FVector(Values[0], Values[1], Values[2]) * (3.14159f / 180.0f);
        Quat = FQuat::MakeFromEuler(NewEulerRad);
    }
}

void UDetailsPanelWidget::RenderFloatProperty(const char* Label, float& Value, float Speed, float Min, float Max)
{
    ImGui::Text(Label);
    ImGui::SameLine(120);
    ImGui::SetNextItemWidth(-1);
    
    if (Min != Max)
    {
        ImGui::DragFloat(("##" + std::string(Label)).c_str(), &Value, Speed, Min, Max);
    }
    else
    {
        ImGui::DragFloat(("##" + std::string(Label)).c_str(), &Value, Speed);
    }
}

void UDetailsPanelWidget::RenderStringProperty(const char* Label, FString& Value)
{
    ImGui::Text(Label);
    ImGui::SameLine(120);
    ImGui::SetNextItemWidth(-1);
    
    char Buffer[256];
    strcpy_s(Buffer, Value.c_str());
    
    if (ImGui::InputText(("##" + std::string(Label)).c_str(), Buffer, sizeof(Buffer)))
    {
        Value = Buffer;
    }
}

void UDetailsPanelWidget::RenderBoolProperty(const char* Label, bool& Value)
{
    ImGui::Text(Label);
    ImGui::SameLine(120);
    
    ImGui::Checkbox(("##" + std::string(Label)).c_str(), &Value);
}

void UDetailsPanelWidget::RenderReadOnlyProperty(const char* Label, const char* Value)
{
    ImGui::Text(Label);
    ImGui::SameLine(120);
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), Value);
}

void UDetailsPanelWidget::RenderPropertySeparator()
{
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
}