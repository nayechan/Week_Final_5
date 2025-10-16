#include "pch.h"
#include "GizmoRotateComponent.h"

IMPLEMENT_CLASS(UGizmoRotateComponent)

UGizmoRotateComponent::UGizmoRotateComponent()
{
    SetStaticMesh("Data/Gizmo/RotationHandle.obj");
    SetMaterial("Shaders/StaticMesh/StaticMeshShader.hlsl", EVertexLayoutType::PositionColorTexturNormal);
}

UGizmoRotateComponent::~UGizmoRotateComponent()
{
}

void UGizmoRotateComponent::DuplicateSubObjects()
{
    Super::DuplicateSubObjects();
}
