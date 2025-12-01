#include "pch.h"
#include "ToolsWidget.h"
#include "ImGui/imgui.h"
#include "Source/Runtime/Engine/Viewer/PhysicsAssetEditorState.h"
#include "Source/Runtime/Engine/PhysicsEngine/PhysicsAsset.h"
#include "Source/Slate/Windows/SPhysicsAssetEditorWindow.h"

IMPLEMENT_CLASS(UToolsWidget);

UToolsWidget::UToolsWidget()
	: UWidget("ToolsWidget")
{
}

void UToolsWidget::Initialize()
{
}

void UToolsWidget::Update()
{
}

const char* UToolsWidget::GetGenerateButtonText() const
{
	if (!EditorState) return "Generate All Bodies";

	// 바디가 선택됨 -> Re-generate Bodies
	if (EditorState->bBodySelectionMode && EditorState->SelectedBodyIndex >= 0)
	{
		return "Re-generate Bodies";
	}

	// 본이 선택됨 (바디 없음) -> Add Bodies
	if (EditorState->SelectedBoneIndex >= 0 && EditorState->EditingAsset)
	{
		int32 BodyIndex = EditorState->EditingAsset->FindBodyIndexByBone(EditorState->SelectedBoneIndex);
		if (BodyIndex < 0)
		{
			return "Add Bodies";
		}
	}

	// 기본 -> Generate All Bodies
	return "Generate All Bodies";
}

bool UToolsWidget::CanGenerate() const
{
	return EditorState && EditorState->CurrentMesh != nullptr;
}

void UToolsWidget::RenderWidget()
{
	if (!EditorState || !EditorWindow) return;

	RenderGenerationSection();
}

void UToolsWidget::RenderGenerationSection()
{
	if (ImGui::CollapsingHeader("Body Generation", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::Indent(8.0f);

		float fullWidth = ImGui::GetContentRegionAvail().x - 8.0f;
		float inputWidth = fullWidth * 0.6f;  // 입력 위젯은 60% 너비
		float buttonWidth = fullWidth;  // 버튼은 전체 너비

		// Primitive Type 콤보박스
		// EAggCollisionShape: Sphere=0, Box=1, Sphyl=2 (Capsule)
		const char* PrimitiveTypes[] = { "Sphere", "Box", "Capsule" };
		int currentType = 0;
		if (SelectedPrimitiveType == EAggCollisionShape::Sphere) currentType = 0;
		else if (SelectedPrimitiveType == EAggCollisionShape::Box) currentType = 1;
		else if (SelectedPrimitiveType == EAggCollisionShape::Sphyl) currentType = 2;

		ImGui::SetNextItemWidth(inputWidth);
		if (ImGui::Combo("Primitive Type", &currentType, PrimitiveTypes, IM_ARRAYSIZE(PrimitiveTypes)))
		{
			switch (currentType)
			{
			case 0: SelectedPrimitiveType = EAggCollisionShape::Sphere; break;
			case 1: SelectedPrimitiveType = EAggCollisionShape::Box; break;
			case 2: SelectedPrimitiveType = EAggCollisionShape::Sphyl; break;
			}
		}

		// Min Bone Size 슬라이더
		ImGui::SetNextItemWidth(inputWidth);
		ImGui::SliderFloat("Min Bone Size", &MinBoneSize, 0.001f, 0.1f, "%.3f");
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("Bodies smaller than this will not be generated");
		}

		ImGui::Spacing();

		// 동적 버튼
		bool bEnabled = CanGenerate();
		if (!bEnabled) ImGui::BeginDisabled();

		const char* ButtonText = GetGenerateButtonText();
		if (ImGui::Button(ButtonText, ImVec2(buttonWidth, 28)))
		{
			if (strcmp(ButtonText, "Re-generate Bodies") == 0)
			{
				EditorWindow->RegenerateSelectedBody();
			}
			else if (strcmp(ButtonText, "Add Bodies") == 0)
			{
				EditorWindow->AddBodyToBone(EditorState->SelectedBoneIndex);
			}
			else  // Generate All Bodies
			{
				EditorWindow->AutoGenerateBodies(SelectedPrimitiveType, MinBoneSize);
			}
		}

		// 툴팁
		if (ImGui::IsItemHovered())
		{
			if (strcmp(ButtonText, "Re-generate Bodies") == 0)
				ImGui::SetTooltip("Re-create the selected body with default shape");
			else if (strcmp(ButtonText, "Add Bodies") == 0)
				ImGui::SetTooltip("Add body to the selected bone");
			else
				ImGui::SetTooltip("Generate bodies for all bones with constraints");
		}

		if (!bEnabled) ImGui::EndDisabled();

		ImGui::Unindent(8.0f);
	}
}
