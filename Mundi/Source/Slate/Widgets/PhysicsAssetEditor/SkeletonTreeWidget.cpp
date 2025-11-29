#include "pch.h"
#include "SkeletonTreeWidget.h"
#include "ImGui/imgui.h"
#include "Source/Runtime/Engine/Viewer/PhysicsAssetEditorState.h"
#include "Source/Runtime/Engine/Physics/PhysicsAsset.h"
#include "Source/Runtime/Engine/Physics/FBodySetup.h"
#include "Source/Runtime/Engine/Physics/FConstraintSetup.h"
#include "Source/Runtime/Core/Misc/VertexData.h"
#include "SkeletalMesh.h"

void SkeletonTreeWidget::Render(PhysicsAssetEditorState* State)
{
	if (!State || !State->CurrentMesh) return;

	const FSkeleton* Skeleton = State->CurrentMesh->GetSkeleton();
	if (!Skeleton || Skeleton->Bones.IsEmpty()) return;

	// 루트 본부터 시작 (부모가 -1인 본)
	for (int32 i = 0; i < static_cast<int32>(Skeleton->Bones.size()); ++i)
	{
		if (Skeleton->Bones[i].ParentIndex == -1)
		{
			RenderBoneNode(State, i, 0);
		}
	}
}

void SkeletonTreeWidget::RenderBoneNode(PhysicsAssetEditorState* State, int32 BoneIndex, int32 Depth)
{
	if (!State || !State->CurrentMesh || !State->EditingAsset) return;

	const FSkeleton* Skeleton = State->CurrentMesh->GetSkeleton();
	if (!Skeleton || BoneIndex < 0 || BoneIndex >= static_cast<int32>(Skeleton->Bones.size())) return;

	const FBone& Bone = Skeleton->Bones[BoneIndex];
	UPhysicsAsset* Asset = State->EditingAsset;

	// 이 본에 연결된 바디 찾기
	int32 BodyIndex = Asset->FindBodyIndexByBone(BoneIndex);
	bool bHasBody = (BodyIndex >= 0);

	// 자식 본 찾기
	TArray<int32> ChildIndices;
	for (int32 i = 0; i < static_cast<int32>(Skeleton->Bones.size()); ++i)
	{
		if (Skeleton->Bones[i].ParentIndex == BoneIndex)
		{
			ChildIndices.Add(i);
		}
	}

	// 트리 노드 플래그
	ImGuiTreeNodeFlags NodeFlags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
	if (ChildIndices.IsEmpty() && !bHasBody)
	{
		NodeFlags |= ImGuiTreeNodeFlags_Leaf;
	}
	if (State->SelectedBoneIndex == BoneIndex)
	{
		NodeFlags |= ImGuiTreeNodeFlags_Selected;
	}

	// 확장 상태 확인
	bool bIsExpanded = State->ExpandedBoneIndices.count(BoneIndex) > 0;
	if (bIsExpanded)
	{
		ImGui::SetNextItemOpen(true, ImGuiCond_Once);
	}

	// 노드 색상 (바디가 있으면 강조)
	if (bHasBody)
	{
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.8f, 0.4f, 1.0f));
	}

	// 트리 노드 렌더링
	FString NodeLabel = Bone.Name;
	if (bHasBody)
	{
		NodeLabel += " [Body]";
	}

	bool bNodeOpen = ImGui::TreeNodeEx((void*)(intptr_t)BoneIndex, NodeFlags, "%s", NodeLabel.c_str());

	if (bHasBody)
	{
		ImGui::PopStyleColor();
	}

	// 클릭 처리
	if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
	{
		State->SelectedBoneIndex = BoneIndex;
		if (bHasBody)
		{
			State->SelectBody(BodyIndex);
		}
	}

	// 컨텍스트 메뉴 (바디 추가/제거는 외부에서 처리하므로 여기서는 표시만)
	if (ImGui::BeginPopupContextItem())
	{
		if (!bHasBody)
		{
			if (ImGui::MenuItem("Add Body"))
			{
				// 외부에서 처리하도록 상태만 설정
				State->SelectedBoneIndex = BoneIndex;
			}
		}
		else
		{
			if (ImGui::MenuItem("Select Body"))
			{
				State->SelectBody(BodyIndex);
			}
		}
		ImGui::EndPopup();
	}

	if (bNodeOpen)
	{
		State->ExpandedBoneIndices.insert(BoneIndex);

		// 바디 노드 렌더링
		if (bHasBody)
		{
			RenderBodyNode(State, BodyIndex);

			// 이 바디와 연결된 제약 조건 렌더링
			for (int32 i = 0; i < static_cast<int32>(Asset->ConstraintSetups.size()); ++i)
			{
				const FConstraintSetup& Constraint = Asset->ConstraintSetups[i];
				if (Constraint.ChildBodyIndex == BodyIndex)
				{
					RenderConstraintNode(State, i);
				}
			}
		}

		// 자식 본 렌더링
		for (int32 ChildIndex : ChildIndices)
		{
			RenderBoneNode(State, ChildIndex, Depth + 1);
		}

		ImGui::TreePop();
	}
	else
	{
		State->ExpandedBoneIndices.erase(BoneIndex);
	}
}

void SkeletonTreeWidget::RenderBodyNode(PhysicsAssetEditorState* State, int32 BodyIndex)
{
	if (!State || !State->EditingAsset) return;
	if (BodyIndex < 0 || BodyIndex >= static_cast<int32>(State->EditingAsset->BodySetups.size())) return;

	const FBodySetup& Body = State->EditingAsset->BodySetups[BodyIndex];

	ImGuiTreeNodeFlags NodeFlags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_SpanAvailWidth;
	if (State->bBodySelectionMode && State->SelectedBodyIndex == BodyIndex)
	{
		NodeFlags |= ImGuiTreeNodeFlags_Selected;
	}

	// Shape 타입 아이콘/이름
	const char* ShapeName = GetShapeTypeName(Body.ShapeType);

	ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.8f, 1.0f, 1.0f));
	bool bNodeOpen = ImGui::TreeNodeEx((void*)(intptr_t)(1000 + BodyIndex), NodeFlags, "[%s]", ShapeName);
	ImGui::PopStyleColor();

	if (ImGui::IsItemClicked())
	{
		State->SelectBody(BodyIndex);
	}

	if (bNodeOpen)
	{
		ImGui::TreePop();
	}
}

void SkeletonTreeWidget::RenderConstraintNode(PhysicsAssetEditorState* State, int32 ConstraintIndex)
{
	if (!State || !State->EditingAsset) return;
	if (ConstraintIndex < 0 || ConstraintIndex >= static_cast<int32>(State->EditingAsset->ConstraintSetups.size())) return;

	const FConstraintSetup& Constraint = State->EditingAsset->ConstraintSetups[ConstraintIndex];

	ImGuiTreeNodeFlags NodeFlags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_SpanAvailWidth;
	if (!State->bBodySelectionMode && State->SelectedConstraintIndex == ConstraintIndex)
	{
		NodeFlags |= ImGuiTreeNodeFlags_Selected;
	}

	const char* ConstraintName = GetConstraintTypeName(Constraint.ConstraintType);

	ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.8f, 0.4f, 1.0f));
	bool bNodeOpen = ImGui::TreeNodeEx((void*)(intptr_t)(2000 + ConstraintIndex), NodeFlags, "[%s] %s",
		ConstraintName, Constraint.JointName.ToString().c_str());
	ImGui::PopStyleColor();

	if (ImGui::IsItemClicked())
	{
		State->SelectConstraint(ConstraintIndex);
	}

	if (bNodeOpen)
	{
		ImGui::TreePop();
	}
}

