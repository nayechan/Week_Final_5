#include "pch.h"
#include "SkeletonTreeWidget.h"
#include "ImGui/imgui.h"
#include "Source/Runtime/Engine/Viewer/PhysicsAssetEditorState.h"
#include "Source/Runtime/Engine/PhysicsEngine/PhysicsAsset.h"
#include "Source/Runtime/Engine/PhysicsEngine/BodySetup.h"
#include "Source/Runtime/Engine/PhysicsEngine/FConstraintSetup.h"
#include "Source/Runtime/Core/Misc/VertexData.h"
#include "SkeletalMesh.h"

IMPLEMENT_CLASS(USkeletonTreeWidget);

USkeletonTreeWidget::USkeletonTreeWidget()
	: UWidget("SkeletonTreeWidget")
{
}

void USkeletonTreeWidget::Initialize()
{
	bCacheValid = false;
}

void USkeletonTreeWidget::Update()
{
	if (!EditorState || !EditorState->CurrentMesh) return;

	const FSkeleton* Skeleton = EditorState->CurrentMesh->GetSkeleton();
	if (!Skeleton) return;

	// 변경 감지: 본/바디/제약조건 수가 변경되면 캐시 무효화
	size_t CurrentBoneCount = Skeleton->Bones.size();
	size_t CurrentBodyCount = EditorState->EditingAsset ? EditorState->EditingAsset->BodySetups.size() : 0;
	size_t CurrentConstraintCount = EditorState->EditingAsset ? EditorState->EditingAsset->ConstraintSetups.size() : 0;

	if (CurrentBoneCount != CachedBoneCount ||
		CurrentBodyCount != CachedBodyCount ||
		CurrentConstraintCount != CachedConstraintCount)
	{
		bCacheValid = false;
		CachedBoneCount = CurrentBoneCount;
		CachedBodyCount = CurrentBodyCount;
		CachedConstraintCount = CurrentConstraintCount;
	}

	// 캐시가 무효화되었으면 재구축
	if (!bCacheValid)
	{
		RebuildCache();
	}
}

void USkeletonTreeWidget::RenderWidget()
{
	if (!EditorState || !EditorState->CurrentMesh) return;

	const FSkeleton* Skeleton = EditorState->CurrentMesh->GetSkeleton();
	if (!Skeleton || Skeleton->Bones.IsEmpty()) return;

	// 루트 본부터 시작 (부모가 -1인 본)
	for (int32 i = 0; i < static_cast<int32>(Skeleton->Bones.size()); ++i)
	{
		if (Skeleton->Bones[i].ParentIndex == -1)
		{
			RenderBoneNode(i, 0);
		}
	}

	// 빈 공간 클릭 시 선택 해제
	if (ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByPopup)
		&& ImGui::IsMouseClicked(ImGuiMouseButton_Left)
		&& !ImGui::IsAnyItemHovered())
	{
		EditorState->ClearSelection();
		EditorState->SelectedBoneIndex = -1;
	}
}

void USkeletonTreeWidget::RebuildCache()
{
	Cache.Clear();

	if (!EditorState || !EditorState->CurrentMesh) return;

	const FSkeleton* Skeleton = EditorState->CurrentMesh->GetSkeleton();
	if (!Skeleton) return;

	// 부모 -> 자식 맵 구축
	for (int32 i = 0; i < static_cast<int32>(Skeleton->Bones.size()); ++i)
	{
		int32 ParentIdx = Skeleton->Bones[i].ParentIndex;
		if (ParentIdx >= 0)
		{
			Cache.ChildrenMap[ParentIdx].push_back(i);
		}
	}

	// 본 -> 바디 맵 구축
	if (EditorState->EditingAsset)
	{
		for (int32 i = 0; i < static_cast<int32>(EditorState->EditingAsset->BodySetups.size()); ++i)
		{
			UBodySetup* BodySetup = EditorState->EditingAsset->BodySetups[i];
			if (BodySetup && BodySetup->BoneIndex >= 0)
			{
				Cache.BoneToBodyMap[BodySetup->BoneIndex] = i;
			}
		}

		// 본 -> 제약조건 맵 구축 (부모/자식 바디 모두에 표시)
		for (int32 i = 0; i < static_cast<int32>(EditorState->EditingAsset->ConstraintSetups.size()); ++i)
		{
			const FConstraintSetup& Constraint = EditorState->EditingAsset->ConstraintSetups[i];
			// 부모 바디의 본에 추가
			if (Constraint.ParentBodyIndex >= 0 && Constraint.ParentBodyIndex < static_cast<int32>(EditorState->EditingAsset->BodySetups.size()))
			{
				UBodySetup* ParentBody = EditorState->EditingAsset->BodySetups[Constraint.ParentBodyIndex];
				if (ParentBody && ParentBody->BoneIndex >= 0)
				{
					Cache.BoneToConstraintMap[ParentBody->BoneIndex].push_back(i);
				}
			}
			// 자식 바디의 본에 추가
			if (Constraint.ChildBodyIndex >= 0 && Constraint.ChildBodyIndex < static_cast<int32>(EditorState->EditingAsset->BodySetups.size()))
			{
				UBodySetup* ChildBody = EditorState->EditingAsset->BodySetups[Constraint.ChildBodyIndex];
				if (ChildBody && ChildBody->BoneIndex >= 0)
				{
					Cache.BoneToConstraintMap[ChildBody->BoneIndex].push_back(i);
				}
			}
		}
	}

	bCacheValid = true;
}

void USkeletonTreeWidget::RenderBoneNode(int32 BoneIndex, int32 Depth)
{
	if (!EditorState || !EditorState->CurrentMesh) return;

	const FSkeleton* Skeleton = EditorState->CurrentMesh->GetSkeleton();
	if (!Skeleton || BoneIndex < 0 || BoneIndex >= static_cast<int32>(Skeleton->Bones.size())) return;

	const FBone& Bone = Skeleton->Bones[BoneIndex];

	// 이 본에 연결된 바디 찾기 (캐시 사용 - O(1))
	int32 BodyIndex = -1;
	auto BodyIt = Cache.BoneToBodyMap.find(BoneIndex);
	if (BodyIt != Cache.BoneToBodyMap.end())
	{
		BodyIndex = BodyIt->second;
	}
	bool bHasBody = (BodyIndex >= 0);

	// 자식 본 가져오기 (캐시 사용 - O(1))
	const std::vector<int32>* ChildIndices = nullptr;
	auto ChildIt = Cache.ChildrenMap.find(BoneIndex);
	if (ChildIt != Cache.ChildrenMap.end())
	{
		ChildIndices = &ChildIt->second;
	}
	bool bHasChildBones = (ChildIndices != nullptr && !ChildIndices->empty());

	// 이 본에 연결된 제약조건 가져오기 (캐시 사용 - O(1))
	const std::vector<int32>* ConstraintIndices = nullptr;
	auto ConstraintIt = Cache.BoneToConstraintMap.find(BoneIndex);
	if (ConstraintIt != Cache.BoneToConstraintMap.end())
	{
		ConstraintIndices = &ConstraintIt->second;
	}
	bool bHasConstraints = (ConstraintIndices != nullptr && !ConstraintIndices->empty());

	// 트리 노드 플래그 (자식 본과 제약조건이 모두 없으면 리프)
	bool bHasChildren = bHasChildBones || bHasConstraints;
	ImGuiTreeNodeFlags NodeFlags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
	if (!bHasChildren)
	{
		NodeFlags |= ImGuiTreeNodeFlags_Leaf;
	}
	// 본 선택 표시: 바디 선택 모드이고 해당 본이 선택된 경우
	if (EditorState->bBodySelectionMode && EditorState->SelectedBoneIndex == BoneIndex)
	{
		NodeFlags |= ImGuiTreeNodeFlags_Selected;
	}

	// 확장 상태 확인
	bool bIsExpanded = EditorState->ExpandedBoneIndices.count(BoneIndex) > 0;
	if (bIsExpanded)
	{
		ImGui::SetNextItemOpen(true, ImGuiCond_Once);
	}

	// 노드 색상 (바디가 있으면 녹색으로 강조)
	if (bHasBody)
	{
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.8f, 0.4f, 1.0f));
	}

	// 트리 노드 렌더링 (본 이름만 표시)
	bool bNodeOpen = ImGui::TreeNodeEx((void*)(intptr_t)BoneIndex, NodeFlags, "%s", Bone.Name.c_str());

	if (bHasBody)
	{
		ImGui::PopStyleColor();
	}

	// 클릭 처리: 본 선택 시 해당 바디도 함께 선택 + 그래프 기준 변경
	if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
	{
		EditorState->SelectedBoneIndex = BoneIndex;
		if (bHasBody)
		{
			EditorState->SelectBody(BodyIndex);
			EditorState->GraphPivotBodyIndex = BodyIndex;  // 그래프 기준도 변경
		}
	}

	// 컨텍스트 메뉴
	if (ImGui::BeginPopupContextItem())
	{
		// 우클릭 시 해당 본 선택
		EditorState->SelectedBoneIndex = BoneIndex;
		if (bHasBody)
		{
			EditorState->SelectBody(BodyIndex);
			EditorState->GraphPivotBodyIndex = BodyIndex;  // 그래프 기준도 변경
		}

		if (!bHasBody)
		{
			// === 바디가 없는 본 ===

			// Add Body (기본 캡슐로 생성)
			if (ImGui::MenuItem("Add Body"))
			{
				OnAddBody.Broadcast(BoneIndex);
			}
		}
		else
		{
			// === 바디가 있는 본 ===

			if (ImGui::MenuItem("Select Body"))
			{
				EditorState->SelectBody(BodyIndex);
				EditorState->GraphPivotBodyIndex = BodyIndex;  // 그래프 기준도 변경
			}

			ImGui::Separator();

			// Add Primitive 서브메뉴
			if (ImGui::BeginMenu("Add Primitive"))
			{
				if (ImGui::MenuItem("Box"))
				{
					OnAddPrimitive.Broadcast(BodyIndex, 0);
				}
				if (ImGui::MenuItem("Sphere"))
				{
					OnAddPrimitive.Broadcast(BodyIndex, 1);
				}
				if (ImGui::MenuItem("Capsule"))
				{
					OnAddPrimitive.Broadcast(BodyIndex, 2);
				}
				ImGui::EndMenu();
			}

			ImGui::Separator();

			// Add Constraint to... 서브메뉴 (모든 Body 목록)
			if (ImGui::BeginMenu("Add Constraint to..."))
			{
				RenderConstraintTargetMenu(BodyIndex);
				ImGui::EndMenu();
			}

			ImGui::Separator();

			// Delete Body
			if (ImGui::MenuItem("Delete Body"))
			{
				OnRemoveBody.Broadcast(BodyIndex);
			}
		}
		ImGui::EndPopup();
	}

	if (bNodeOpen)
	{
		EditorState->ExpandedBoneIndices.insert(BoneIndex);

		// 이 본에 연결된 제약조건 먼저 표시
		if (ConstraintIndices)
		{
			for (int32 ConstraintIndex : *ConstraintIndices)
			{
				RenderConstraintNode(ConstraintIndex);
			}
		}

		// 자식 본 렌더링
		if (ChildIndices)
		{
			for (int32 ChildIndex : *ChildIndices)
			{
				RenderBoneNode(ChildIndex, Depth + 1);
			}
		}

		ImGui::TreePop();
	}
	else
	{
		EditorState->ExpandedBoneIndices.erase(BoneIndex);
	}
}

void USkeletonTreeWidget::RenderBodyNode(int32 BodyIndex)
{
	if (!EditorState || !EditorState->EditingAsset) return;
	if (BodyIndex < 0 || BodyIndex >= static_cast<int32>(EditorState->EditingAsset->BodySetups.size())) return;

	UBodySetup* Body = EditorState->EditingAsset->BodySetups[BodyIndex];
	if (!Body) return;

	ImGuiTreeNodeFlags NodeFlags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_SpanAvailWidth;
	if (EditorState->bBodySelectionMode && EditorState->SelectedBodyIndex == BodyIndex)
	{
		NodeFlags |= ImGuiTreeNodeFlags_Selected;
	}

	// Shape 개수 표시
	int32 ShapeCount = Body->AggGeom.GetElementCount();

	ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.8f, 1.0f, 1.0f));
	bool bNodeOpen = ImGui::TreeNodeEx((void*)(intptr_t)(1000 + BodyIndex), NodeFlags, "[%d shapes]", ShapeCount);
	ImGui::PopStyleColor();

	if (ImGui::IsItemClicked())
	{
		EditorState->SelectBody(BodyIndex);
		EditorState->GraphPivotBodyIndex = BodyIndex;  // 그래프 기준도 변경
	}

	if (bNodeOpen)
	{
		ImGui::TreePop();
	}
}

void USkeletonTreeWidget::RenderConstraintNode(int32 ConstraintIndex)
{
	if (!EditorState || !EditorState->EditingAsset) return;
	if (ConstraintIndex < 0 || ConstraintIndex >= static_cast<int32>(EditorState->EditingAsset->ConstraintSetups.size())) return;

	const FConstraintSetup& Constraint = EditorState->EditingAsset->ConstraintSetups[ConstraintIndex];

	ImGuiTreeNodeFlags NodeFlags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_SpanAvailWidth;
	if (!EditorState->bBodySelectionMode && EditorState->SelectedConstraintIndex == ConstraintIndex)
	{
		NodeFlags |= ImGuiTreeNodeFlags_Selected;
	}

	// 부모-자식 바디 이름 가져오기
	FString ParentName = "?";
	FString ChildName = "?";
	if (Constraint.ParentBodyIndex >= 0 && Constraint.ParentBodyIndex < static_cast<int32>(EditorState->EditingAsset->BodySetups.size()))
	{
		UBodySetup* ParentBody = EditorState->EditingAsset->BodySetups[Constraint.ParentBodyIndex];
		if (ParentBody) ParentName = ParentBody->BoneName.ToString();
	}
	if (Constraint.ChildBodyIndex >= 0 && Constraint.ChildBodyIndex < static_cast<int32>(EditorState->EditingAsset->BodySetups.size()))
	{
		UBodySetup* ChildBody = EditorState->EditingAsset->BodySetups[Constraint.ChildBodyIndex];
		if (ChildBody) ChildName = ChildBody->BoneName.ToString();
	}

	// 선택 여부에 따라 색상 변경
	bool bSelected = (!EditorState->bBodySelectionMode && EditorState->SelectedConstraintIndex == ConstraintIndex);
	ImGui::PushStyleColor(ImGuiCol_Text, bSelected ? ImVec4(1.0f, 1.0f, 0.4f, 1.0f) : ImVec4(1.0f, 0.7f, 0.4f, 1.0f));
	bool bNodeOpen = ImGui::TreeNodeEx((void*)(intptr_t)(2000 + ConstraintIndex), NodeFlags, "[Constraint] %s -> %s",
		ParentName.c_str(), ChildName.c_str());
	ImGui::PopStyleColor();

	if (ImGui::IsItemClicked())
	{
		EditorState->SelectConstraint(ConstraintIndex);
	}

	// 컨텍스트 메뉴
	if (ImGui::BeginPopupContextItem())
	{
		// 우클릭 시 해당 컨스트레인트 선택
		EditorState->SelectConstraint(ConstraintIndex);

		if (ImGui::MenuItem("Delete Constraint"))
		{
			OnRemoveConstraint.Broadcast(ConstraintIndex);
		}
		ImGui::EndPopup();
	}

	if (bNodeOpen)
	{
		ImGui::TreePop();
	}
}

void USkeletonTreeWidget::RenderConstraintTargetMenu(int32 SourceBodyIndex)
{
	if (!EditorState || !EditorState->EditingAsset) return;

	bool bHasItems = false;

	// 모든 Body 목록 표시
	for (int32 i = 0; i < static_cast<int32>(EditorState->EditingAsset->BodySetups.size()); ++i)
	{
		if (i == SourceBodyIndex) continue;  // 자기 자신 제외

		UBodySetup* TargetBody = EditorState->EditingAsset->BodySetups[i];
		if (!TargetBody) continue;

		bHasItems = true;

		// 이미 Constraint가 있는지 확인 (양방향)
		int32 ExistingConstraint = EditorState->EditingAsset->FindConstraintIndex(SourceBodyIndex, i);
		int32 ExistingConstraintReverse = EditorState->EditingAsset->FindConstraintIndex(i, SourceBodyIndex);
		bool bAlreadyConnected = (ExistingConstraint >= 0 || ExistingConstraintReverse >= 0);

		// 메뉴 아이템
		FString Label = TargetBody->BoneName.ToString();
		if (bAlreadyConnected)
		{
			Label += " (connected)";
			ImGui::BeginDisabled();
		}

		if (ImGui::MenuItem(Label.c_str()))
		{
			OnAddConstraint.Broadcast(SourceBodyIndex, i);
		}

		if (bAlreadyConnected)
		{
			ImGui::EndDisabled();
		}
	}

	if (!bHasItems)
	{
		ImGui::TextDisabled("No other bodies available");
	}
}

