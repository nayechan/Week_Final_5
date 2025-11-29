#include "pch.h"
#include "ConstraintPropertiesWidget.h"
#include "ImGui/imgui.h"
#include "Source/Runtime/Engine/Viewer/PhysicsAssetEditorState.h"
#include "Source/Runtime/Engine/Physics/FConstraintSetup.h"

bool ConstraintPropertiesWidget::Render(PhysicsAssetEditorState* State, FConstraintSetup& Constraint)
{
	if (!State) return false;

	bool bChanged = false;

	ImGui::Text("Constraint: %s", Constraint.JointName.ToString().c_str());
	ImGui::Separator();

	// 제약 조건 타입
	const char* ConstraintTypes[] = { "Ball and Socket", "Hinge" };
	int32 CurrentType = static_cast<int32>(Constraint.ConstraintType);
	if (ImGui::Combo("Type", &CurrentType, ConstraintTypes, IM_ARRAYSIZE(ConstraintTypes)))
	{
		Constraint.ConstraintType = static_cast<EConstraintType>(CurrentType);
		State->bIsDirty = true;
		bChanged = true;
	}

	// 각도 제한
	if (ImGui::CollapsingHeader("Limits", ImGuiTreeNodeFlags_DefaultOpen))
	{
		bChanged |= RenderLimitProperties(State, Constraint);
	}

	// 강도/댐핑
	if (ImGui::CollapsingHeader("Stiffness & Damping"))
	{
		if (ImGui::DragFloat("Stiffness", &Constraint.Stiffness, 0.1f, 0.0f, 10000.0f))
		{
			State->bIsDirty = true;
			bChanged = true;
		}
		if (ImGui::DragFloat("Damping", &Constraint.Damping, 0.1f, 0.0f, 1000.0f))
		{
			State->bIsDirty = true;
			bChanged = true;
		}
	}

	return bChanged;
}

bool ConstraintPropertiesWidget::RenderLimitProperties(PhysicsAssetEditorState* State, FConstraintSetup& Constraint)
{
	bool bChanged = false;

	if (ImGui::DragFloat("Swing1 Limit", &Constraint.Swing1Limit, 1.0f, 0.0f, 180.0f))
	{
		State->bIsDirty = true;
		bChanged = true;
	}

	// BallAndSocket만 Swing2와 Twist 사용
	if (Constraint.ConstraintType == EConstraintType::BallAndSocket)
	{
		if (ImGui::DragFloat("Swing2 Limit", &Constraint.Swing2Limit, 1.0f, 0.0f, 180.0f))
		{
			State->bIsDirty = true;
			bChanged = true;
		}

		if (ImGui::DragFloat("Twist Min", &Constraint.TwistLimitMin, 1.0f, -180.0f, 0.0f))
		{
			State->bIsDirty = true;
			bChanged = true;
		}

		if (ImGui::DragFloat("Twist Max", &Constraint.TwistLimitMax, 1.0f, 0.0f, 180.0f))
		{
			State->bIsDirty = true;
			bChanged = true;
		}
	}

	return bChanged;
}
