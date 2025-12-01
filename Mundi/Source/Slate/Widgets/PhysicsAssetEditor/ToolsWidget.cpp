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

		float buttonWidth = ImGui::GetContentRegionAvail().x - 8.0f;

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
				EditorWindow->AutoGenerateBodies();
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
