#include "pch.h"
#include "SPhysicsAssetEditorWindow.h"
#include "SlateManager.h"
#include "ImGui/imgui.h"
#include "FViewport.h"
#include "FViewportClient.h"
#include <filesystem>
#include "Source/Runtime/Engine/Viewer/PhysicsAssetEditorBootstrap.h"
#include "Source/Runtime/Engine/Viewer/EditorAssetPreviewContext.h"
#include "Source/Runtime/Engine/Physics/PhysicsAsset.h"
#include "Source/Runtime/Engine/Physics/PhysicsTypes.h"
#include "Source/Runtime/Engine/Physics/FBodySetup.h"
#include "Source/Runtime/Engine/Physics/FConstraintSetup.h"
#include "Source/Runtime/Core/Misc/VertexData.h"
#include "SkeletalMeshActor.h"
#include "SkeletalMesh.h"
#include "SkeletalMeshComponent.h"
#include "Source/Runtime/Engine/Components/LineComponent.h"

// Sub Widgets
#include "Source/Slate/Widgets/PhysicsAssetEditor/SkeletonTreeWidget.h"
#include "Source/Slate/Widgets/PhysicsAssetEditor/BodyPropertiesWidget.h"
#include "Source/Slate/Widgets/PhysicsAssetEditor/ConstraintPropertiesWidget.h"

SPhysicsAssetEditorWindow::SPhysicsAssetEditorWindow()
{
	WindowTitle = "Physics Asset Editor";
	bHasBottomPanel = false; // 기본적으로 하단 패널 숨김
}

SPhysicsAssetEditorWindow::~SPhysicsAssetEditorWindow()
{
}

ViewerState* SPhysicsAssetEditorWindow::CreateViewerState(const char* Name, UEditorAssetPreviewContext* Context)
{
	return PhysicsAssetEditorBootstrap::CreateViewerState(Name, World, Device, Context);
}

void SPhysicsAssetEditorWindow::DestroyViewerState(ViewerState*& State)
{
	PhysicsAssetEditorBootstrap::DestroyViewerState(State);
}

void SPhysicsAssetEditorWindow::OpenOrFocusTab(UEditorAssetPreviewContext* Context)
{
	if (!Context) return;

	// 같은 파일 경로의 탭이 있는지 확인
	for (int32 i = 0; i < Tabs.Num(); ++i)
	{
		PhysicsAssetEditorState* State = static_cast<PhysicsAssetEditorState*>(Tabs[i]);
		if (State && State->CurrentFilePath == Context->AssetPath)
		{
			// 기존 탭으로 전환
			ActiveTabIndex = i;
			ActiveState = Tabs[i];
			return;
		}
	}

	// 새 탭 생성
	SViewerWindow::OpenOrFocusTab(Context);
}

void SPhysicsAssetEditorWindow::OnRender()
{
	// If window is closed, request cleanup and don't render
	if (!bIsOpen)
	{
		USlateManager::GetInstance().RequestCloseDetachedWindow(this);
		return;
	}

	// Parent detachable window (movable, top-level) with solid background
	ImGuiWindowFlags flags = ImGuiWindowFlags_NoSavedSettings;

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

	// Generate a unique window title to pass to ImGui
	char UniqueTitle[256];
	FString Title = GetWindowTitle();
	if (Tabs.Num() == 1 && ActiveState)
	{
		PhysicsAssetEditorState* PhysState = static_cast<PhysicsAssetEditorState*>(ActiveState);
		if (!PhysState->CurrentFilePath.empty())
		{
			std::filesystem::path fsPath(UTF8ToWide(PhysState->CurrentFilePath));
			FString AssetName = WideToUTF8(fsPath.filename().wstring());
			Title += " - " + AssetName;
		}
	}
	sprintf_s(UniqueTitle, sizeof(UniqueTitle), "%s###%p", Title.c_str(), this);

	bool bViewerVisible = false;
	if (ImGui::Begin(UniqueTitle, &bIsOpen, flags))
	{
		bViewerVisible = true;

		// 입력 라우팅을 위한 hover/focus 상태 캡처
		bIsWindowHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows);
		bIsWindowFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);

		// Render tab bar and switch active state
		RenderTabsAndToolbar(EViewerType::PhysicsAsset);

		// 마지막 탭을 닫은 경우 렌더링 중단
		if (!bIsOpen)
		{
			USlateManager::GetInstance().RequestCloseDetachedWindow(this);
			ImGui::End();
			return;
		}

		ImVec2 pos = ImGui::GetWindowPos();
		ImVec2 size = ImGui::GetWindowSize();
		Rect.Left = pos.x; Rect.Top = pos.y; Rect.Right = pos.x + size.x; Rect.Bottom = pos.y + size.y; Rect.UpdateMinMax();

		ImVec2 contentAvail = ImGui::GetContentRegionAvail();
		float totalWidth = contentAvail.x;
		float totalHeight = contentAvail.y;

		float splitterWidth = 4.0f; // 분할선 두께

		float leftWidth = totalWidth * LeftPanelRatio;
		float rightWidth = totalWidth * RightPanelRatio;
		float centerWidth = totalWidth - leftWidth - rightWidth - (splitterWidth * 2);

		// 중앙 패널이 음수가 되지 않도록 보정 (안전장치)
		if (centerWidth < 0.0f)
		{
			centerWidth = 0.0f;
		}

		// Remove spacing between panels
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));

		// Left panel - Skeleton Tree & Bodies
		ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
		ImGui::BeginChild("LeftPanel", ImVec2(leftWidth, totalHeight), true, ImGuiWindowFlags_NoScrollbar);
		ImGui::PopStyleVar();
		RenderLeftPanel(leftWidth);
		ImGui::EndChild();

		ImGui::SameLine(0, 0); // No spacing between panels

		// Left splitter (드래그 가능한 분할선)
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.3f, 0.3f, 0.5f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.4f, 0.4f, 0.4f, 0.7f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.5f, 0.5f, 0.5f, 0.9f));
		ImGui::Button("##LeftSplitter", ImVec2(splitterWidth, totalHeight));
		ImGui::PopStyleColor(3);

		if (ImGui::IsItemHovered())
			ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);

		if (ImGui::IsItemActive())
		{
			float delta = ImGui::GetIO().MouseDelta.x;
			if (delta != 0.0f)
			{
				float newLeftRatio = LeftPanelRatio + delta / totalWidth;
				float maxLeftRatio = 1.0f - RightPanelRatio - (splitterWidth * 2) / totalWidth;
				LeftPanelRatio = std::max(0.1f, std::min(newLeftRatio, maxLeftRatio));
			}
		}

		ImGui::SameLine(0, 0);

		// Center panel (viewport area)
		if (centerWidth > 0.0f)
		{
			ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
			ImGui::BeginChild("CenterPanel", ImVec2(centerWidth, totalHeight), false,
				ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoNavInputs | ImGuiWindowFlags_NoNavFocus);
			ImGui::PopStyleVar();

			// 뷰어 툴바 렌더링 (뷰포트 상단)
			RenderViewerToolbar();

			// 툴바 아래 뷰포트 영역
			ImVec2 viewportPos = ImGui::GetCursorScreenPos();
			float remainingWidth = ImGui::GetContentRegionAvail().x;
			float remainingHeight = ImGui::GetContentRegionAvail().y;

			// 뷰포트 영역 설정
			CenterRect.Left = viewportPos.x;
			CenterRect.Top = viewportPos.y;
			CenterRect.Right = viewportPos.x + remainingWidth;
			CenterRect.Bottom = viewportPos.y + remainingHeight;
			CenterRect.UpdateMinMax();

			// 뷰포트 렌더링 (텍스처에)
			OnRenderViewport();

			// ImGui::Image로 결과 텍스처 표시
			if (ActiveState && ActiveState->Viewport)
			{
				ID3D11ShaderResourceView* SRV = ActiveState->Viewport->GetSRV();
				if (SRV)
				{
					ImGui::Image((void*)SRV, ImVec2(remainingWidth, remainingHeight));
					ActiveState->Viewport->SetViewportHovered(ImGui::IsItemHovered());
				}
				else
				{
					ImGui::Dummy(ImVec2(remainingWidth, remainingHeight));
					ActiveState->Viewport->SetViewportHovered(false);
				}
			}
			else
			{
				ImGui::Dummy(ImVec2(remainingWidth, remainingHeight));
			}

			ImGui::EndChild(); // CenterPanel

			ImGui::SameLine(0, 0);
		}
		else
		{
			CenterRect = FRect(0, 0, 0, 0);
			CenterRect.UpdateMinMax();
		}

		// Right splitter (드래그 가능한 분할선)
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.3f, 0.3f, 0.5f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.4f, 0.4f, 0.4f, 0.7f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.5f, 0.5f, 0.5f, 0.9f));
		ImGui::Button("##RightSplitter", ImVec2(splitterWidth, totalHeight));
		ImGui::PopStyleColor(3);

		if (ImGui::IsItemHovered())
			ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);

		if (ImGui::IsItemActive())
		{
			float delta = ImGui::GetIO().MouseDelta.x;
			if (delta != 0.0f)
			{
				float newRightRatio = RightPanelRatio - delta / totalWidth;
				float maxRightRatio = 1.0f - LeftPanelRatio - (splitterWidth * 2) / totalWidth;
				RightPanelRatio = std::max(0.1f, std::min(newRightRatio, maxRightRatio));
			}
		}

		ImGui::SameLine(0, 0);

		// Right panel - Properties
		ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
		ImGui::BeginChild("RightPanel", ImVec2(rightWidth, totalHeight), true);
		ImGui::PopStyleVar();
		RenderRightPanel();
		ImGui::EndChild();

		// Pop the ItemSpacing style
		ImGui::PopStyleVar();
	}
	ImGui::End();

	// If collapsed or not visible, clear the center rect
	if (!bViewerVisible)
	{
		CenterRect = FRect(0, 0, 0, 0);
		CenterRect.UpdateMinMax();
		bIsWindowHovered = false;
		bIsWindowFocused = false;
	}

	// If window was closed via X button, notify the manager to clean up
	if (!bIsOpen)
	{
		USlateManager::GetInstance().RequestCloseDetachedWindow(this);
	}

	bRequestFocus = false;
}

void SPhysicsAssetEditorWindow::OnUpdate(float DeltaSeconds)
{
	SViewerWindow::OnUpdate(DeltaSeconds);

	// Shape 프리뷰 업데이트
	PhysicsAssetEditorState* State = GetActivePhysicsState();
	if (State && State->bShapePreviewDirty)
	{
		UpdateShapePreview();
		State->bShapePreviewDirty = false;
	}
}

void SPhysicsAssetEditorWindow::PreRenderViewportUpdate()
{
	// 뷰포트 렌더 전 처리
}

void SPhysicsAssetEditorWindow::OnSave()
{
	SavePhysicsAsset();
}

void SPhysicsAssetEditorWindow::RenderTabsAndToolbar(EViewerType CurrentViewerType)
{
	SViewerWindow::RenderTabsAndToolbar(EViewerType::PhysicsAsset);
	RenderToolbar();
}

void SPhysicsAssetEditorWindow::RenderLeftPanel(float PanelWidth)
{
	PhysicsAssetEditorState* State = GetActivePhysicsState();
	if (!State) return;

	// 스켈레탈 메시 로드 UI (기본 클래스의 Asset Browser 사용)
	SViewerWindow::RenderLeftPanel(PanelWidth);

	ImGui::Spacing();
	ImGui::Separator();
	ImGui::Spacing();

	// 본/바디 계층 구조 섹션
	ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.30f, 0.30f, 0.30f, 0.8f));
	ImGui::Text("SKELETON / BODIES");
	ImGui::PopStyleColor();
	ImGui::Spacing();
	ImGui::Separator();
	ImGui::Spacing();

	// 본/바디 트리 렌더링 (Sub Widget 사용)
	// 스크롤 가능한 영역으로 감싸기
	float remainingHeight = ImGui::GetContentRegionAvail().y;
	ImGui::BeginChild("SkeletonTreeScroll", ImVec2(0, remainingHeight), false);
	SkeletonTreeWidget::Render(State);
	ImGui::EndChild();
}

void SPhysicsAssetEditorWindow::RenderRightPanel()
{
	PhysicsAssetEditorState* State = GetActivePhysicsState();
	if (!State || !State->EditingAsset) return;

	// 패널 헤더
	ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 6);
	ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.30f, 0.30f, 0.30f, 0.8f));
	ImGui::Text("PROPERTIES");
	ImGui::PopStyleColor();
	ImGui::Spacing();
	ImGui::Separator();
	ImGui::Spacing();

	// 선택된 바디 또는 제약 조건의 속성 표시 (Sub Widget 사용)
	if (State->bBodySelectionMode && State->SelectedBodyIndex >= 0)
	{
		if (State->SelectedBodyIndex < static_cast<int32>(State->EditingAsset->BodySetups.size()))
		{
			FBodySetup& Body = State->EditingAsset->BodySetups[State->SelectedBodyIndex];
			BodyPropertiesWidget::Render(State, Body);
		}
	}
	else if (!State->bBodySelectionMode && State->SelectedConstraintIndex >= 0)
	{
		if (State->SelectedConstraintIndex < static_cast<int32>(State->EditingAsset->ConstraintSetups.size()))
		{
			FConstraintSetup& Constraint = State->EditingAsset->ConstraintSetups[State->SelectedConstraintIndex];
			ConstraintPropertiesWidget::Render(State, Constraint);
		}
	}
	else
	{
		ImGui::TextDisabled("Select a body or constraint to edit properties");
	}
}

void SPhysicsAssetEditorWindow::RenderBottomPanel()
{
	if (!bShowBottomPanel) return;

	PhysicsAssetEditorState* State = GetActivePhysicsState();
	if (!State) return;

	ImGui::BeginChild("ConstraintGraphPanel", ImVec2(0, BottomPanelHeight), true);

	ImGui::Text("Constraint Graph");
	ImGui::Separator();

	RenderConstraintGraph();

	ImGui::EndChild();
}

void SPhysicsAssetEditorWindow::OnSkeletalMeshLoaded(ViewerState* State, const FString& Path)
{
	PhysicsAssetEditorState* PhysicsState = static_cast<PhysicsAssetEditorState*>(State);
	if (!PhysicsState || !PhysicsState->EditingAsset) return;

	// Physics Asset에 스켈레탈 메시 경로 저장
	PhysicsState->EditingAsset->SkeletalMeshPath = Path;
	PhysicsState->bIsDirty = true;
	PhysicsState->bShapePreviewDirty = true;

	UE_LOG("[SPhysicsAssetEditorWindow] Skeletal mesh loaded: %s", Path.c_str());
}

// ────────────────────────────────────────────────────────────────
// 툴바
// ────────────────────────────────────────────────────────────────

void SPhysicsAssetEditorWindow::RenderToolbar()
{
	PhysicsAssetEditorState* State = GetActivePhysicsState();
	if (!State) return;

	// 저장 버튼
	if (ImGui::Button("Save"))
	{
		SavePhysicsAsset();
	}
	ImGui::SameLine();

	if (ImGui::Button("Save As"))
	{
		SavePhysicsAssetAs();
	}
	ImGui::SameLine();

	ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
	ImGui::SameLine();

	// 바디 추가/제거 버튼
	bool bCanAddBody = State->SelectedBoneIndex >= 0;
	if (!bCanAddBody) ImGui::BeginDisabled();
	if (ImGui::Button("Add Body"))
	{
		AddBodyToBone(State->SelectedBoneIndex);
	}
	if (!bCanAddBody) ImGui::EndDisabled();
	ImGui::SameLine();

	bool bCanRemoveBody = State->bBodySelectionMode && State->SelectedBodyIndex >= 0;
	if (!bCanRemoveBody) ImGui::BeginDisabled();
	if (ImGui::Button("Remove Body"))
	{
		RemoveSelectedBody();
	}
	if (!bCanRemoveBody) ImGui::EndDisabled();
	ImGui::SameLine();

	ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
	ImGui::SameLine();

	// 제약 조건 제거 버튼
	bool bCanRemoveConstraint = !State->bBodySelectionMode && State->SelectedConstraintIndex >= 0;
	if (!bCanRemoveConstraint) ImGui::BeginDisabled();
	if (ImGui::Button("Remove Constraint"))
	{
		RemoveSelectedConstraint();
	}
	if (!bCanRemoveConstraint) ImGui::EndDisabled();
	ImGui::SameLine();

	ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
	ImGui::SameLine();

	// 표시 옵션
	ImGui::Checkbox("Bodies", &State->bShowBodies);
	ImGui::SameLine();
	ImGui::Checkbox("Constraints", &State->bShowConstraints);
	ImGui::SameLine();

	// 제약 조건 그래프 토글
	if (ImGui::Checkbox("Graph", &bShowBottomPanel))
	{
		bHasBottomPanel = bShowBottomPanel;
	}
}

void SPhysicsAssetEditorWindow::LoadToolbarIcons()
{
	// 아이콘 로드 (추후 구현)
}

// ────────────────────────────────────────────────────────────────
// 하단 패널 (제약 조건 그래프)
// ────────────────────────────────────────────────────────────────

void SPhysicsAssetEditorWindow::RenderConstraintGraph()
{
	PhysicsAssetEditorState* State = GetActivePhysicsState();
	if (!State || !State->EditingAsset) return;

	// 간단한 리스트 형태로 제약 조건 표시 (추후 노드 그래프로 확장 가능)
	ImGui::TextDisabled("Constraint Graph (simplified list view)");

	for (int32 i = 0; i < static_cast<int32>(State->EditingAsset->ConstraintSetups.size()); ++i)
	{
		const FConstraintSetup& Constraint = State->EditingAsset->ConstraintSetups[i];

		FString ParentName = "?";
		FString ChildName = "?";

		if (Constraint.ParentBodyIndex >= 0 && Constraint.ParentBodyIndex < static_cast<int32>(State->EditingAsset->BodySetups.size()))
		{
			ParentName = State->EditingAsset->BodySetups[Constraint.ParentBodyIndex].BoneName.ToString();
		}
		if (Constraint.ChildBodyIndex >= 0 && Constraint.ChildBodyIndex < static_cast<int32>(State->EditingAsset->BodySetups.size()))
		{
			ChildName = State->EditingAsset->BodySetups[Constraint.ChildBodyIndex].BoneName.ToString();
		}

		bool bSelected = (!State->bBodySelectionMode && State->SelectedConstraintIndex == i);
		char LabelBuf[256];
		sprintf_s(LabelBuf, "%s -> %s (%s)", ParentName.c_str(), ChildName.c_str(), GetConstraintTypeName(Constraint.ConstraintType));
		if (ImGui::Selectable(LabelBuf, bSelected))
		{
			State->SelectConstraint(i);
		}
	}
}

// ────────────────────────────────────────────────────────────────
// 뷰포트
// ────────────────────────────────────────────────────────────────

void SPhysicsAssetEditorWindow::RenderViewportArea(float width, float height)
{
	// SViewerWindow의 기본 뷰포트 렌더링 사용
}

void SPhysicsAssetEditorWindow::UpdateShapePreview()
{
	PhysicsAssetEditorState* State = GetActivePhysicsState();
	if (!State || !State->EditingAsset) return;

	ULineComponent* BodyLineComp = State->BodyPreviewLineComponent;
	ULineComponent* ConstraintLineComp = State->ConstraintPreviewLineComponent;

	if (BodyLineComp)
	{
		BodyLineComp->ClearLines();
		BodyLineComp->SetLineVisible(State->bShowBodies);

		// 각 바디의 Shape 렌더링
		for (int32 i = 0; i < static_cast<int32>(State->EditingAsset->BodySetups.size()); ++i)
		{
			const FBodySetup& Body = State->EditingAsset->BodySetups[i];

			// 선택된 바디는 다른 색상
			FVector4 Color = (State->bBodySelectionMode && State->SelectedBodyIndex == i)
				? FVector4(0.0f, 1.0f, 0.0f, 1.0f)  // 선택: 녹색
				: FVector4(0.0f, 0.5f, 1.0f, 1.0f); // 기본: 파랑

			// 본 트랜스폼 가져오기
			FTransform BoneTransform;
			if (State->CurrentMesh && Body.BoneIndex >= 0)
			{
				// 본 월드 트랜스폼 계산 (단순화된 버전)
				if (State->PreviewActor)
				{
					ASkeletalMeshActor* SkelActor = static_cast<ASkeletalMeshActor*>(State->PreviewActor);
					if (SkelActor->GetSkeletalMeshComponent())
					{
						BoneTransform = SkelActor->GetSkeletalMeshComponent()->GetBoneWorldTransform(Body.BoneIndex);
					}
				}
			}

			FTransform ShapeTransform = BoneTransform.GetWorldTransform(Body.LocalTransform);
			FVector Center = ShapeTransform.Translation;

			// Shape 타입에 따라 와이어프레임 렌더링
			switch (Body.ShapeType)
			{
			case EPhysicsShapeType::Sphere:
				// 구체 와이어프레임 (간단한 원 3개)
				for (int32 j = 0; j < 32; ++j)
				{
					float Angle1 = (j / 32.0f) * 2.0f * 3.14159f;
					float Angle2 = ((j + 1) / 32.0f) * 2.0f * 3.14159f;

					// XY 평면
					BodyLineComp->AddLine(
						Center + FVector(cos(Angle1) * Body.Radius, sin(Angle1) * Body.Radius, 0),
						Center + FVector(cos(Angle2) * Body.Radius, sin(Angle2) * Body.Radius, 0),
						Color);
					// XZ 평면
					BodyLineComp->AddLine(
						Center + FVector(cos(Angle1) * Body.Radius, 0, sin(Angle1) * Body.Radius),
						Center + FVector(cos(Angle2) * Body.Radius, 0, sin(Angle2) * Body.Radius),
						Color);
					// YZ 평면
					BodyLineComp->AddLine(
						Center + FVector(0, cos(Angle1) * Body.Radius, sin(Angle1) * Body.Radius),
						Center + FVector(0, cos(Angle2) * Body.Radius, sin(Angle2) * Body.Radius),
						Color);
				}
				break;

			case EPhysicsShapeType::Capsule:
				// 캡슐 와이어프레임 (간단화)
				{
					FVector Up = ShapeTransform.Rotation.RotateVector(FVector(0, 0, 1));
					FVector Top = Center + Up * Body.HalfHeight;
					FVector Bottom = Center - Up * Body.HalfHeight;

					// 수직 라인
					for (int32 j = 0; j < 8; ++j)
					{
						float Angle = (j / 8.0f) * 2.0f * 3.14159f;
						FVector Dir = ShapeTransform.Rotation.RotateVector(FVector(cos(Angle), sin(Angle), 0));
						BodyLineComp->AddLine(Top + Dir * Body.Radius, Bottom + Dir * Body.Radius, Color);
					}

					// 상단/하단 원
					for (int32 j = 0; j < 32; ++j)
					{
						float Angle1 = (j / 32.0f) * 2.0f * 3.14159f;
						float Angle2 = ((j + 1) / 32.0f) * 2.0f * 3.14159f;
						FVector Dir1 = ShapeTransform.Rotation.RotateVector(FVector(cos(Angle1), sin(Angle1), 0));
						FVector Dir2 = ShapeTransform.Rotation.RotateVector(FVector(cos(Angle2), sin(Angle2), 0));
						BodyLineComp->AddLine(Top + Dir1 * Body.Radius, Top + Dir2 * Body.Radius, Color);
						BodyLineComp->AddLine(Bottom + Dir1 * Body.Radius, Bottom + Dir2 * Body.Radius, Color);
					}
				}
				break;

			case EPhysicsShapeType::Box:
				// 박스 와이어프레임
				{
					FVector E = Body.Extent;
					FVector Corners[8] = {
						Center + ShapeTransform.Rotation.RotateVector(FVector(-E.X, -E.Y, -E.Z)),
						Center + ShapeTransform.Rotation.RotateVector(FVector( E.X, -E.Y, -E.Z)),
						Center + ShapeTransform.Rotation.RotateVector(FVector( E.X,  E.Y, -E.Z)),
						Center + ShapeTransform.Rotation.RotateVector(FVector(-E.X,  E.Y, -E.Z)),
						Center + ShapeTransform.Rotation.RotateVector(FVector(-E.X, -E.Y,  E.Z)),
						Center + ShapeTransform.Rotation.RotateVector(FVector( E.X, -E.Y,  E.Z)),
						Center + ShapeTransform.Rotation.RotateVector(FVector( E.X,  E.Y,  E.Z)),
						Center + ShapeTransform.Rotation.RotateVector(FVector(-E.X,  E.Y,  E.Z)),
					};

					// 하단 사각형
					BodyLineComp->AddLine(Corners[0], Corners[1], Color);
					BodyLineComp->AddLine(Corners[1], Corners[2], Color);
					BodyLineComp->AddLine(Corners[2], Corners[3], Color);
					BodyLineComp->AddLine(Corners[3], Corners[0], Color);
					// 상단 사각형
					BodyLineComp->AddLine(Corners[4], Corners[5], Color);
					BodyLineComp->AddLine(Corners[5], Corners[6], Color);
					BodyLineComp->AddLine(Corners[6], Corners[7], Color);
					BodyLineComp->AddLine(Corners[7], Corners[4], Color);
					// 수직 라인
					BodyLineComp->AddLine(Corners[0], Corners[4], Color);
					BodyLineComp->AddLine(Corners[1], Corners[5], Color);
					BodyLineComp->AddLine(Corners[2], Corners[6], Color);
					BodyLineComp->AddLine(Corners[3], Corners[7], Color);
				}
				break;
			}
		}
	}

	// 제약 조건 시각화 (두 바디 사이의 연결선)
	if (ConstraintLineComp)
	{
		ConstraintLineComp->ClearLines();
		ConstraintLineComp->SetLineVisible(State->bShowConstraints);

		for (int32 i = 0; i < static_cast<int32>(State->EditingAsset->ConstraintSetups.size()); ++i)
		{
			const FConstraintSetup& Constraint = State->EditingAsset->ConstraintSetups[i];

			if (Constraint.ParentBodyIndex < 0 || Constraint.ChildBodyIndex < 0) continue;
			if (Constraint.ParentBodyIndex >= static_cast<int32>(State->EditingAsset->BodySetups.size())) continue;
			if (Constraint.ChildBodyIndex >= static_cast<int32>(State->EditingAsset->BodySetups.size())) continue;

			// 선택된 제약 조건은 다른 색상
			FVector4 Color = (!State->bBodySelectionMode && State->SelectedConstraintIndex == i)
				? FVector4(1.0f, 1.0f, 0.0f, 1.0f)   // 선택: 노랑
				: FVector4(1.0f, 0.5f, 0.0f, 1.0f);  // 기본: 주황

			// 바디 위치 가져오기 (단순화: 본 위치 사용)
			FVector ParentPos, ChildPos;

			const FBodySetup& ParentBody = State->EditingAsset->BodySetups[Constraint.ParentBodyIndex];
			const FBodySetup& ChildBody = State->EditingAsset->BodySetups[Constraint.ChildBodyIndex];

			if (State->PreviewActor)
			{
				ASkeletalMeshActor* SkelActor = static_cast<ASkeletalMeshActor*>(State->PreviewActor);
				if (SkelActor->GetSkeletalMeshComponent())
				{
					ParentPos = SkelActor->GetSkeletalMeshComponent()->GetBoneWorldTransform(ParentBody.BoneIndex).Translation;
					ChildPos = SkelActor->GetSkeletalMeshComponent()->GetBoneWorldTransform(ChildBody.BoneIndex).Translation;
				}
			}

			ConstraintLineComp->AddLine(ParentPos, ChildPos, Color);
		}
	}
}

// ────────────────────────────────────────────────────────────────
// 파일 작업
// ────────────────────────────────────────────────────────────────

void SPhysicsAssetEditorWindow::SavePhysicsAsset()
{
	PhysicsAssetEditorState* State = GetActivePhysicsState();
	if (!State || !State->EditingAsset) return;

	if (State->CurrentFilePath.empty())
	{
		SavePhysicsAssetAs();
		return;
	}

	if (PhysicsAssetEditorBootstrap::SavePhysicsAsset(State->EditingAsset, State->CurrentFilePath))
	{
		State->bIsDirty = false;
	}
}

void SPhysicsAssetEditorWindow::SavePhysicsAssetAs()
{
	// 파일 다이얼로그 (추후 구현)
	// 현재는 기본 경로로 저장
	PhysicsAssetEditorState* State = GetActivePhysicsState();
	if (!State || !State->EditingAsset) return;

	FString DefaultPath = "Data/PhysicsAssets/NewPhysicsAsset.json";
	if (PhysicsAssetEditorBootstrap::SavePhysicsAsset(State->EditingAsset, DefaultPath))
	{
		State->CurrentFilePath = DefaultPath;
		State->bIsDirty = false;
	}
}

void SPhysicsAssetEditorWindow::LoadPhysicsAsset()
{
	// 파일 다이얼로그 (추후 구현)
}

// ────────────────────────────────────────────────────────────────
// 바디/제약 조건 작업
// ────────────────────────────────────────────────────────────────

void SPhysicsAssetEditorWindow::AddBodyToBone(int32 BoneIndex)
{
	PhysicsAssetEditorState* State = GetActivePhysicsState();
	if (!State || !State->EditingAsset || !State->CurrentMesh) return;
	if (BoneIndex < 0) return;

	const FSkeleton* Skeleton = State->CurrentMesh->GetSkeleton();
	if (!Skeleton || BoneIndex >= static_cast<int32>(Skeleton->Bones.size())) return;

	// 이미 바디가 있는지 확인
	if (State->EditingAsset->FindBodyIndexByBone(BoneIndex) >= 0)
	{
		UE_LOG("[SPhysicsAssetEditorWindow] Bone already has a body");
		return;
	}

	FName BoneName = Skeleton->Bones[BoneIndex].Name;
	int32 NewBodyIndex = State->EditingAsset->AddBody(BoneName, BoneIndex);

	if (NewBodyIndex >= 0)
	{
		// 부모 본의 바디 찾기
		int32 ParentBoneIndex = Skeleton->Bones[BoneIndex].ParentIndex;
		if (ParentBoneIndex >= 0)
		{
			int32 ParentBodyIndex = State->EditingAsset->FindBodyIndexByBone(ParentBoneIndex);
			if (ParentBodyIndex >= 0)
			{
				// 자동으로 제약 조건 생성
				char JointNameBuf[128];
				sprintf_s(JointNameBuf, "Joint_%s", BoneName.ToString().c_str());
				State->EditingAsset->AddConstraint(FName(JointNameBuf), ParentBodyIndex, NewBodyIndex);
			}
		}

		State->SelectBody(NewBodyIndex);
		State->bIsDirty = true;
		State->bShapePreviewDirty = true;
	}
}

void SPhysicsAssetEditorWindow::RemoveSelectedBody()
{
	PhysicsAssetEditorState* State = GetActivePhysicsState();
	if (!State || !State->EditingAsset) return;
	if (State->SelectedBodyIndex < 0) return;

	if (State->EditingAsset->RemoveBody(State->SelectedBodyIndex))
	{
		State->ClearSelection();
		State->bIsDirty = true;
		State->bShapePreviewDirty = true;
	}
}

void SPhysicsAssetEditorWindow::AddConstraintBetweenBodies(int32 ParentBodyIndex, int32 ChildBodyIndex)
{
	PhysicsAssetEditorState* State = GetActivePhysicsState();
	if (!State || !State->EditingAsset) return;

	char JointNameBuf[64];
	sprintf_s(JointNameBuf, "Constraint_%d_%d", ParentBodyIndex, ChildBodyIndex);
	int32 NewIndex = State->EditingAsset->AddConstraint(FName(JointNameBuf), ParentBodyIndex, ChildBodyIndex);

	if (NewIndex >= 0)
	{
		State->SelectConstraint(NewIndex);
		State->bIsDirty = true;
		State->bShapePreviewDirty = true;
	}
}

void SPhysicsAssetEditorWindow::RemoveSelectedConstraint()
{
	PhysicsAssetEditorState* State = GetActivePhysicsState();
	if (!State || !State->EditingAsset) return;
	if (State->SelectedConstraintIndex < 0) return;

	if (State->EditingAsset->RemoveConstraint(State->SelectedConstraintIndex))
	{
		State->ClearSelection();
		State->bIsDirty = true;
		State->bShapePreviewDirty = true;
	}
}

