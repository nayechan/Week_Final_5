#include "pch.h"
#include "BodyPropertiesWidget.h"
#include "ImGui/imgui.h"
#include "Source/Runtime/Engine/Viewer/PhysicsAssetEditorState.h"
#include "Source/Runtime/Engine/Physics/FBodySetup.h"

bool BodyPropertiesWidget::Render(PhysicsAssetEditorState* State, FBodySetup& Body)
{
	if (!State) return false;

	bool bChanged = false;

	ImGui::Text("Body: %s", Body.BoneName.ToString().c_str());
	ImGui::Separator();

	// Shape 속성
	if (ImGui::CollapsingHeader("Shape", ImGuiTreeNodeFlags_DefaultOpen))
	{
		bChanged |= RenderShapeProperties(State, Body);
	}

	// 물리 속성
	if (ImGui::CollapsingHeader("Physics Properties"))
	{
		bChanged |= RenderPhysicsProperties(State, Body);
	}

	return bChanged;
}

bool BodyPropertiesWidget::RenderShapeProperties(PhysicsAssetEditorState* State, FBodySetup& Body)
{
	bool bChanged = false;

	// Shape 타입 선택
	const char* ShapeTypes[] = { "Sphere", "Capsule", "Box" };
	int32 CurrentType = static_cast<int32>(Body.ShapeType);
	if (ImGui::Combo("Shape Type", &CurrentType, ShapeTypes, IM_ARRAYSIZE(ShapeTypes)))
	{
		Body.ShapeType = static_cast<EPhysicsShapeType>(CurrentType);
		State->bIsDirty = true;
		State->bShapePreviewDirty = true;
		bChanged = true;
	}

	// Shape 파라미터 (타입에 따라)
	switch (Body.ShapeType)
	{
	case EPhysicsShapeType::Sphere:
		if (ImGui::DragFloat("Radius", &Body.Radius, 0.1f, 0.1f, 1000.0f))
		{
			State->bIsDirty = true;
			State->bShapePreviewDirty = true;
			bChanged = true;
		}
		break;

	case EPhysicsShapeType::Capsule:
		if (ImGui::DragFloat("Radius", &Body.Radius, 0.1f, 0.1f, 1000.0f))
		{
			State->bIsDirty = true;
			State->bShapePreviewDirty = true;
			bChanged = true;
		}
		if (ImGui::DragFloat("Half Height", &Body.HalfHeight, 0.1f, 0.1f, 1000.0f))
		{
			State->bIsDirty = true;
			State->bShapePreviewDirty = true;
			bChanged = true;
		}
		break;

	case EPhysicsShapeType::Box:
		float Extent[3] = { Body.Extent.X, Body.Extent.Y, Body.Extent.Z };
		if (ImGui::DragFloat3("Extent", Extent, 0.1f, 0.1f, 1000.0f))
		{
			Body.Extent = FVector(Extent[0], Extent[1], Extent[2]);
			State->bIsDirty = true;
			State->bShapePreviewDirty = true;
			bChanged = true;
		}
		break;
	}

	// 로컬 트랜스폼
	ImGui::Separator();
	ImGui::Text("Local Transform");

	FVector Location = Body.LocalTransform.Translation;
	float Loc[3] = { Location.X, Location.Y, Location.Z };
	if (ImGui::DragFloat3("Location", Loc, 0.1f))
	{
		Body.LocalTransform.Translation = FVector(Loc[0], Loc[1], Loc[2]);
		State->bIsDirty = true;
		State->bShapePreviewDirty = true;
		bChanged = true;
	}

	FVector RotEuler = Body.LocalTransform.Rotation.ToEulerZYXDeg();
	float Rot[3] = { RotEuler.X, RotEuler.Y, RotEuler.Z };
	if (ImGui::DragFloat3("Rotation", Rot, 1.0f))
	{
		Body.LocalTransform.Rotation = FQuat::MakeFromEulerZYX(FVector(Rot[0], Rot[1], Rot[2]));
		State->bIsDirty = true;
		State->bShapePreviewDirty = true;
		bChanged = true;
	}

	return bChanged;
}

bool BodyPropertiesWidget::RenderPhysicsProperties(PhysicsAssetEditorState* State, FBodySetup& Body)
{
	bool bChanged = false;

	if (ImGui::DragFloat("Mass", &Body.Mass, 0.1f, 0.01f, 10000.0f))
	{
		State->bIsDirty = true;
		bChanged = true;
	}

	if (ImGui::DragFloat("Linear Damping", &Body.LinearDamping, 0.01f, 0.0f, 100.0f))
	{
		State->bIsDirty = true;
		bChanged = true;
	}

	if (ImGui::DragFloat("Angular Damping", &Body.AngularDamping, 0.01f, 0.0f, 100.0f))
	{
		State->bIsDirty = true;
		bChanged = true;
	}

	if (ImGui::Checkbox("Enable Gravity", &Body.bEnableGravity))
	{
		State->bIsDirty = true;
		bChanged = true;
	}

	return bChanged;
}
