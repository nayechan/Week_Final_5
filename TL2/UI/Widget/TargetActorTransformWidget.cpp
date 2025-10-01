#include "pch.h"
#include "TargetActorTransformWidget.h"
#include "UI/UIManager.h"
#include "ImGui/imgui.h"
#include "Actor.h"
#include "GridActor.h"
#include "World.h"
#include "Vector.h"
#include "GizmoActor.h"
#include <string>
#include "StaticMeshActor.h"    
#include "StaticMeshComponent.h"
#include "ResourceManager.h"    
#include "TextRenderComponent.h"
#include "CameraComponent.h"
#include "BillboardComponent.h"
using namespace std;


// ★ 고정 오더: ZYX (Yaw-Pitch-Roll) ? 기즈모의 Delta 곱(Z * Y * X)과 동일
static inline FQuat QuatFromEulerZYX_Deg(const FVector& Deg)
{
	const float Rx = DegreeToRadian(Deg.X); // Roll (X)
	const float Ry = DegreeToRadian(Deg.Y); // Pitch (Y)
	const float Rz = DegreeToRadian(Deg.Z); // Yaw (Z)

	const FQuat Qx = MakeQuatFromAxisAngle(FVector(1, 0, 0), Rx);
	const FQuat Qy = MakeQuatFromAxisAngle(FVector(0, 1, 0), Ry);
	const FQuat Qz = MakeQuatFromAxisAngle(FVector(0, 0, 1), Rz);
	return Qz * Qy * Qx; // ZYX
}

static inline FVector EulerZYX_DegFromQuat(const FQuat& Q)
{
	// 표준 ZYX(roll=x, pitch=y, yaw=z) 복원식
	// 참고: roll(X), pitch(Y), yaw(Z)
	const float w = Q.W, x = Q.X, y = Q.Y, z = Q.Z;

	const float sinr_cosp = 2.0f * (w * x + y * z);
	const float cosr_cosp = 1.0f - 2.0f * (x * x + y * y);
	float roll = std::atan2(sinr_cosp, cosr_cosp);

	float sinp = 2.0f * (w * y - z * x);
	float pitch;
	if (std::fabs(sinp) >= 1.0f) pitch = std::copysign(HALF_PI, sinp);
	else                          pitch = std::asin(sinp);

	const float siny_cosp = 2.0f * (w * z + x * y);
	const float cosy_cosp = 1.0f - 2.0f * (y * y + z * z);
	float yaw = std::atan2(siny_cosp, cosy_cosp);

	return FVector(RadianToDegree(roll), RadianToDegree(pitch), RadianToDegree(yaw));
}

namespace
{
	struct FAddableComponentDescriptor
	{
		const char* Label;
		UClass* Class;
		const char* Description;
	};

	const TArray<FAddableComponentDescriptor>& GetAddableComponentDescriptors()
	{
		static TArray<FAddableComponentDescriptor> Options = []()
			{
				TArray<FAddableComponentDescriptor> Result;
				Result.push_back({ "Static Mesh Component", UStaticMeshComponent::StaticClass(), "Static mesh 렌더링용 컴포넌트" });
				Result.push_back({ "Billboard Component", UBillboardComponent::StaticClass(), "빌보드 텍스쳐 표시" });
				return Result;
			}();
		return Options;
	}
	// 초기에 들어간 컴포넌트들은 지우지 못하도록
	bool IsProtectedSceneComponent(const AActor& Actor, const USceneComponent* Component)
	{
		if (!Component)
		{
			return false;
		}

		if (Component == Actor.GetRootComponent())
		{
			return true;
		}

		if (Component == Actor.CollisionComponent)
		{
			return true;
		}

		if (Component == Actor.TextComp)
		{
			return true;
		}

		if (const AStaticMeshActor* StaticMeshActor = Cast<AStaticMeshActor>(&Actor))
		{
			if (Component == StaticMeshActor->GetStaticMeshComponent())
			{
				return true;
			}
		}

		return false;
	}

	bool CanRemoveSceneComponent(const AActor& Actor, const USceneComponent* Component)
	{
		return !IsProtectedSceneComponent(Actor, Component);
	}

	bool TryAttachComponentToActor(AActor& Actor, UClass* ComponentClass)
	{
		if (!ComponentClass || !ComponentClass->IsChildOf(UActorComponent::StaticClass()))
			return false;

		UObject* RawObject = ObjectFactory::NewObject(ComponentClass);
		if (!RawObject)
		{
			return false;
		}

		UActorComponent* NewComp = Cast<UActorComponent>(RawObject);
		if (!NewComp)
		{
			ObjectFactory::DeleteObject(RawObject);
			return false;
		}

		NewComp->SetOwner(&Actor);

		// 씬 컴포넌트라면 루트에 붙임
		if (USceneComponent* SceneComp = Cast<USceneComponent>(NewComp))
		{
			SceneComp->SetWorldTransform(Actor.GetActorTransform()); // 초기 월드 트랜스폼을 부모와 동일하게

			if (USceneComponent* Root = Actor.GetRootComponent())
			{
				const bool bUsesWorldAttachment =
					SceneComp->IsA(UStaticMeshComponent::StaticClass()) ||
					SceneComp->IsA(UBillboardComponent::StaticClass());   // ← BillBoard도 월드 기준

				const EAttachmentRule AttachRule =
					bUsesWorldAttachment ? EAttachmentRule::KeepWorld : EAttachmentRule::KeepRelative;

				SceneComp->SetupAttachment(Root, AttachRule);
			}
		}

		// AddOwnedComponent 경유 (Register/Initialize 포함)
		Actor.AddOwnedComponent(NewComp);
		Actor.MarkPartitionDirty();
		return true;
	}
	void MarkComponentSubtreeVisited(USceneComponent* Component, TSet<USceneComponent*>& Visited)
	{
		if (!Component || Visited.count(Component))
		{
			return;
		}

		Visited.insert(Component);
		for (USceneComponent* Child : Component->GetAttachChildren())
		{
			MarkComponentSubtreeVisited(Child, Visited);
		}
	}

	void RenderSceneComponentTree(
		USceneComponent* Component,
		AActor& Actor,
		USceneComponent*& SelectedComponent,
		USceneComponent*& ComponentPendingRemoval,
		TSet<USceneComponent*>& Visited)
	{
		if (!Component)
			return;

		Visited.insert(Component);

		const TArray<USceneComponent*>& Children = Component->GetAttachChildren();
		const bool bHasChildren = !Children.IsEmpty();

		ImGuiTreeNodeFlags NodeFlags =
			ImGuiTreeNodeFlags_OpenOnArrow |
			ImGuiTreeNodeFlags_SpanAvailWidth;

		if (!bHasChildren)
		{
			NodeFlags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
		}
		// 선택 하이라이트: 현재 선택된 컴포넌트와 같으면 Selected 플래그
		if (Component == SelectedComponent)
		{
			NodeFlags |= ImGuiTreeNodeFlags_Selected;
		}

		FString Label = Component->GetClass() ? Component->GetName() : "Unknown Component";
		if (Component == Actor.GetRootComponent())
		{
			Label += " (Root)";
		}

		// 트리 노드 그리기 직전에 ID 푸시
		ImGui::PushID(Component);
		const bool bNodeOpen = ImGui::TreeNodeEx(Component, NodeFlags, "%s", Label.c_str());
		// 좌클릭 시 컴포넌트 선택으로 전환(액터 Row 선택 해제)
		if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
		{
			SelectedComponent = Component;
		}

		if (ImGui::BeginPopupContextItem("ComponentContext"))
		{
			const bool bCanRemove = CanRemoveSceneComponent(Actor, Component);
			if (ImGui::MenuItem("삭제", "Delete", false, bCanRemove))
			{
				ComponentPendingRemoval = Component;
			}
			ImGui::EndPopup();
		}

		if (bNodeOpen && bHasChildren)
		{
			for (USceneComponent* Child : Children)
			{
				RenderSceneComponentTree(Child, Actor, SelectedComponent, ComponentPendingRemoval, Visited);
			}
			ImGui::TreePop();
		}
		else if (!bNodeOpen && bHasChildren)
		{
			for (USceneComponent* Child : Children)
			{
				MarkComponentSubtreeVisited(Child, Visited);
			}
		}
		// 항목 처리가 끝나면 반드시 PopID
		ImGui::PopID();
	}
}

// 파일명 스템(Cube 등) 추출 + .obj 확장자 제거
static inline FString GetBaseNameNoExt(const FString& Path)
{
	const size_t sep = Path.find_last_of("/\\");
	const size_t start = (sep == FString::npos) ? 0 : sep + 1;

	const FString ext = ".obj";
	size_t end = Path.size();
	if (end >= ext.size() && Path.compare(end - ext.size(), ext.size(), ext) == 0)
	{
		end -= ext.size();
	}
	if (start <= end) return Path.substr(start, end - start);
	return Path;
}

UTargetActorTransformWidget::UTargetActorTransformWidget()
	: UWidget("Target Actor Transform Widget")
	, UIManager(&UUIManager::GetInstance())
{

}

UTargetActorTransformWidget::~UTargetActorTransformWidget() = default;

void UTargetActorTransformWidget::OnSelectedActorCleared()
{
	// 즉시 내부 캐시/플래그 정리
	SelectedActor = nullptr;
	CachedActorName.clear();
	ResetChangeFlags();
	SelectedComponent = nullptr;
}

void UTargetActorTransformWidget::Initialize()
{
	// UIManager 참조 확보
	UIManager = &UUIManager::GetInstance();

	// Transform 위젯을 UIManager에 등록하여 선택 해제 브로드캐스트를 받을 수 있게 함
	if (UIManager)
	{
		UIManager->RegisterTargetTransformWidget(this);
	}
}

AActor* UTargetActorTransformWidget::GetCurrentSelectedActor() const
{
	if (!UIManager)
		return nullptr;

	return UIManager->GetSelectedActor();
}

USceneComponent* UTargetActorTransformWidget::GetEditingComponent() const
{
	if (!SelectedActor)
		return nullptr;

	USceneComponent* RootComponent = SelectedActor->GetRootComponent();
	if (!SelectedComponent || SelectedComponent == RootComponent)
		return nullptr;

	// 기본 보호 컴포넌트(텍스트, AABB, 초기 스태틱 메쉬 등)는 항상 액터 트랜스폼과 함께 움직인다.
	if (IsProtectedSceneComponent(*SelectedActor, SelectedComponent))
		return nullptr;

	return SelectedComponent;
}

UStaticMeshComponent* UTargetActorTransformWidget::GetEditingStaticMeshComponent() const
{
	if (USceneComponent* EditingComp = GetEditingComponent())
		return Cast<UStaticMeshComponent>(EditingComp);

	if (SelectedActor)
		if (AStaticMeshActor* StaticMeshActor = Cast<AStaticMeshActor>(SelectedActor))
			return StaticMeshActor->GetStaticMeshComponent();

	return nullptr;
}

void UTargetActorTransformWidget::Update()
{
	// UIManager를 통해 현재 선택된 액터 가져오기
	AActor* CurrentSelectedActor = GetCurrentSelectedActor();
	if (SelectedActor != CurrentSelectedActor)
	{
		SelectedActor = CurrentSelectedActor;
		SelectedComponent = nullptr;
		// 새로 선택된 액터의 이름 캐시
		if (SelectedActor)
		{
			try
			{
				CachedActorName = SelectedActor->GetName().ToString();

				// ★ 선택 변경 시 한 번만 초기화
				const FVector S = SelectedActor->GetActorScale();
				bUniformScale = (fabs(S.X - S.Y) < 0.01f && fabs(S.Y - S.Z) < 0.01f);

				// 스냅샷
				UpdateTransformFromActor();
				PrevEditRotationUI = EditRotation; // 회전 UI 기준값 초기화
				bRotationEditing = false;          // 편집 상태 초기화
			}
			catch (...)
			{
				CachedActorName = "[Invalid Actor]";
				SelectedActor = nullptr;
			}
		}
		else
		{
			CachedActorName.clear();
		}
	}

	if (SelectedActor)
	{
		// 액터가 선택되어 있으면 항상 트랜스폼 정보를 업데이트하여
		// 기즈모 조작을 실시간으로 UI에 반영합니다.
		// 회전 필드 편집 중이면 그 프레임은 엔진→UI 역동기화(회전)를 막는다.
		if (!bRotationEditing)
		{
			UpdateTransformFromActor();
			// 회전을 제외하고 위치/스케일도 여기서 갱신하고 싶다면,
			// UpdateTransformFromActor()에서 회전만 건너뛰는 오버로드를 따로 만들어도 OK.
		}
	}
}

void UTargetActorTransformWidget::RenderWidget()
{
	{
		/*
			컨트롤 패널에서 선택한 직후, Update()가 아직 돌기 전에
			RenderWidget()이 먼저 실행되면 CachedActorName이 빈 상태인 채로 Selectable을
			그리게 되기 때문에 여기서도 캐시 갱신
		*/

		// 캐시 갱신을 RenderWidget에서도 보장
		if (SelectedActor)
		{
			try
			{
				const FString LatestName = SelectedActor->GetName().ToString();
				if (CachedActorName != LatestName)
				{
					CachedActorName = LatestName;
				}
			}
			catch (...)
			{
				CachedActorName.clear();
				SelectedActor = nullptr;
			}
		}
		else
		{
			CachedActorName.clear();
		}

	}
	// 컴포넌트 관리 UI
	if (SelectedActor)
	{
		ImGui::Text(CachedActorName.c_str());
		ImGui::SameLine();
		// 버튼 크기
		const float ButtonWidth = 60.0f;
		const float ButtonHeight = 25.0f;
		// 남은 영역에서 버튼 폭만큼 오른쪽으로 밀기 (클램프)
		float Avail = ImGui::GetContentRegionAvail().x;
		float NewX = ImGui::GetCursorPosX() + std::max(0.0f, Avail - ButtonWidth);
		ImGui::SetCursorPosX(NewX);
		if (ImGui::Button("+ 추가", ImVec2(ButtonWidth, ButtonHeight)))
		{
			ImGui::OpenPopup("AddComponentPopup");
		}

		if (ImGui::BeginPopup("AddComponentPopup"))
		{
			ImGui::TextUnformatted("Add Component");
			ImGui::Separator();

			for (const FAddableComponentDescriptor& Descriptor : GetAddableComponentDescriptors())
			{
				ImGui::PushID(Descriptor.Label);
				if (ImGui::Selectable(Descriptor.Label))
				{
					if (TryAttachComponentToActor(*SelectedActor, Descriptor.Class))
					{
						ImGui::CloseCurrentPopup();
					}
				}
				if (Descriptor.Description && ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("%s", Descriptor.Description);
				}
				ImGui::PopID();
			}

			ImGui::EndPopup();
		}
		ImGui::Spacing();


		AActor* ActorPendingRemoval = nullptr;
		USceneComponent* ComponentPendingRemoval = nullptr;
		USceneComponent* RootComponent = SelectedActor->GetRootComponent();
		USceneComponent* PreviousSelectedComponent = SelectedComponent;   // ← 추가
		USceneComponent* EditingComponent = GetEditingComponent();
		const bool bActorSelected = (SelectedActor != nullptr && SelectedComponent == nullptr);

		// 컴포넌트 트리 박스 크기 관련
		static float PaneHeight = 120.0f;        // 초기값
		const float SplitterThickness = 6.0f;    // 드래그 핸들 두께
		const float MinTop = 1.0f;             // 위 박스 최소 높이
		const float MinBottom = 0.0f;          // 아래 영역 최소 높이

		// 현재 창의 남은 영역 높이(이 함수 블록의 시작 시점 기준)
		const float WindowAvailY = ImGui::GetContentRegionAvail().y;

		// 창이 줄어들었을 때 위/아래 최소 높이를 고려해 클램프
		PaneHeight = std::clamp(PaneHeight, MinTop, std::max(MinTop, WindowAvailY - (MinBottom + SplitterThickness)));

		ImGui::BeginChild("ComponentBox", ImVec2(0, PaneHeight), true);
		ImGui::PushID("ActorDisplay");
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.85f, 0.3f, 1.0f));

		// 액터 이름 표시 
		const bool bActorClicked =
			ImGui::Selectable(CachedActorName.c_str(), bActorSelected, ImGuiSelectableFlags_SelectOnClick | ImGuiSelectableFlags_SpanAvailWidth);
		ImGui::PopStyleColor();

		if (bActorClicked && SelectedActor)
		{
			SelectedComponent = nullptr;
		}

		if (ImGui::BeginPopupContextItem("ActorContextMenu"))
		{
			if (ImGui::MenuItem("삭제", "Delete", false, SelectedActor != nullptr))
			{
				ActorPendingRemoval = SelectedActor;
			}
			ImGui::EndPopup();
		}
		ImGui::PopID();

		if (!RootComponent)
		{
			ImGui::BulletText("Root component not found.");
		}
		else
		{
			TSet<USceneComponent*> VisitedComponents;
			// 아직 루트 컴포넌트에 붙은 컴포넌트는 static component밖에 없음.
			RenderSceneComponentTree(RootComponent, *SelectedActor, SelectedComponent, ComponentPendingRemoval, VisitedComponents);

			const TArray<USceneComponent*>& AllComponents = SelectedActor->GetSceneComponents();
			for (USceneComponent* Component : AllComponents)
			{
				if (!Component || VisitedComponents.count(Component))
				{
					//UE_LOG("Skipping already visited or null component.");
					continue;
				}

				ImGui::PushID(Component);
				const bool bSelected = (Component == SelectedComponent);
				if (ImGui::Selectable(Component->GetName().c_str(), bSelected))
				{
					SelectedComponent = Component; // 하이라이트 유지
				}

				if (ImGui::BeginPopupContextItem("ComponentContext"))
				{
					// 루트 컴포넌트가 아닌 경우에만 제거 가능
					const bool bCanRemove = CanRemoveSceneComponent(*SelectedActor, Component);
					if (ImGui::MenuItem("삭제", "Delete", false, bCanRemove))
					{
						ComponentPendingRemoval = Component;
					}
					ImGui::EndPopup();
				}
				ImGui::PopID();
			}
		}

		if (PreviousSelectedComponent != SelectedComponent)
		{
			UpdateTransformFromActor();          // 새 대상을 기준으로 Location/Rotation/Scale 갱신
			PrevEditRotationUI = EditRotation;   // 회전 슬라이더 기준값 초기화
			bRotationEditing = false;

			const FVector ScaleRef = EditScale;
			bUniformScale = (std::fabs(ScaleRef.X - ScaleRef.Y) < 0.01f &&
				std::fabs(ScaleRef.Y - ScaleRef.Z) < 0.01f);
		}

		const bool bDeletePressed =
			ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
			ImGui::IsKeyPressed(ImGuiKey_Delete);

		if (bDeletePressed && SelectedActor)
		{
			if (SelectedComponent == nullptr)
			{
				// 액터 Row가 선택된 상태 → 액터 삭제
				ActorPendingRemoval = SelectedActor;
			}
			else if (SelectedComponent && CanRemoveSceneComponent(*SelectedActor, SelectedComponent))
			{
				// 컴포넌트 선택 상태(루트 아님) → 해당 컴포넌트 삭제
				ComponentPendingRemoval = SelectedComponent;
			}
			else
			{
				// 루트 컴포넌트가 선택된 상태면 삭제 불가
			}
		}

		if (ComponentPendingRemoval && SelectedActor)
		{
			if (CanRemoveSceneComponent(*SelectedActor, ComponentPendingRemoval))
			{
				SelectedActor->RemoveOwnedComponent(ComponentPendingRemoval);
				if (SelectedComponent == ComponentPendingRemoval)
				{
					SelectedComponent = nullptr;
				}
			}
			ComponentPendingRemoval = nullptr;
		}

		if (ActorPendingRemoval)
		{
			UWorld* World = ActorPendingRemoval->GetWorld();
			if (World)
			{
				World->DestroyActor(ActorPendingRemoval);
			}
			else
			{
				ActorPendingRemoval->Destroy();
			}
			OnSelectedActorCleared();
			return; // 방금 제거된 액터에 대한 나머지 UI 갱신 건너뜀
		}

		ImGui::EndChild();
		// 2) 스플리터(드래그 핸들)
		// 화면 가로로 꽉 차는 보이지 않는 버튼을 만든다.
		ImGui::PushStyleColor(ImGuiCol_Separator, ImGui::GetStyle().Colors[ImGuiCol_Separator]);
		ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0, 0)); // 영향 거의 없음, 습관적 가드
		ImGui::InvisibleButton("VerticalSplitter", ImVec2(-FLT_MIN, SplitterThickness));
		bool SplitterHovered = ImGui::IsItemHovered();
		bool SplitterActive = ImGui::IsItemActive();

		// 커서 모양 변경
		if (SplitterHovered || SplitterActive)
			ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);

		// 드래그 중이면 높이 조정
		if (SplitterActive)
		{
			PaneHeight += ImGui::GetIO().MouseDelta.y;
			// 다시 클램프 (위/아래 최소 높이 유지)
			PaneHeight = std::clamp(PaneHeight, MinTop, std::max(MinTop, ImGui::GetContentRegionAvail().y + PaneHeight /* 아래 계산상 보정 */
				+ SplitterThickness - MinBottom));
		}

		// 얇은 선을 그려 시각적 구분(선택)
		{
			ImVec2 Min = ImGui::GetItemRectMin();
			ImVec2 Max = ImGui::GetItemRectMax();
			float Y = 0.5f * (Min.y + Max.y);
			ImGui::GetWindowDrawList()->AddLine(ImVec2(Min.x, Y), ImVec2(Max.x, Y),
				ImGui::GetColorU32(ImGuiCol_Separator), 1.0f);
		}
		ImGui::PopStyleVar();
		ImGui::PopStyleColor();
		ImGui::Spacing();

		// Location 편집
		if (ImGui::DragFloat3("Location", &EditLocation.X, 0.1f))
		{
			bPositionChanged = true;
		}

		// ───────── Rotation: DragFloat3 하나로 "드래그=증분", "입력=절대" 처리 ─────────
		{
			// 1) 컨트롤 그리기 전에 이전값 스냅
			const FVector Before = EditRotation;

			// 2) DragFloat3 호출
			//    (키보드 입력도 허용되는 컨트롤이므로 라벨에 ZYX 오더 명시 권장)
			bool ChangedThisFrame = ImGui::DragFloat3("Rotation", &EditRotation.X, 0.5f);
			// 3) 컨트롤 상태 읽기
			ImGuiIO& IO = ImGui::GetIO();
			const bool Activated = ImGui::IsItemActivated();              // 이번 프레임에 포커스/활성 시작
			const bool Active = ImGui::IsItemActive();                 // 현재 편집 중
			const bool Edited = ImGui::IsItemEdited();                 // 이번 프레임에 값이 변함
			const bool Deactivated = ImGui::IsItemDeactivatedAfterEdit();   // 편집 종료(값 변함 포함)

			// "드래그 중" 판정: 마우스 좌클릭이 눌린 상태에서 활성이고, 실제 드래그가 발생
			const bool MouseHeld = ImGui::IsMouseDown(ImGuiMouseButton_Left);
			const bool MouseDrag = ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0.0f) ||
				(std::fabs(IO.MouseDelta.x) + std::fabs(IO.MouseDelta.y) > 0.0f);
			const bool Dragging = Active && MouseHeld && MouseDrag;

			// 4) 편집 세션 시작 시(이번 프레임 Activate) 기준값 스냅
			//    - 절대 적용용으로 쓸 시작 회전과 UI 기준값 저장
			static FQuat StartQuatOnEdit;
			if (Activated)
			{
				StartQuatOnEdit = SelectedActor ? SelectedActor->GetActorRotation() : FQuat();
				PrevEditRotationUI = EditRotation;
				bRotationEditing = true;   // Update() 역동기화 방지
			}

			// 5) 값이 변한 프레임에 처리
			if (Edited && (SelectedActor || EditingComponent))
			{
				if (Dragging)
				{
					FVector DeltaEuler = EditRotation - PrevEditRotationUI;
					auto Wrap = [](float a)->float { while (a > 180.f) a -= 360.f; while (a < -180.f) a += 360.f; return a; };
					DeltaEuler.X = Wrap(DeltaEuler.X);
					DeltaEuler.Y = Wrap(DeltaEuler.Y);
					DeltaEuler.Z = Wrap(DeltaEuler.Z);

					const FQuat Qx = MakeQuatFromAxisAngle(FVector(1, 0, 0), DegreeToRadian(DeltaEuler.X));
					const FQuat Qy = MakeQuatFromAxisAngle(FVector(0, 1, 0), DegreeToRadian(DeltaEuler.Y));
					const FQuat Qz = MakeQuatFromAxisAngle(FVector(0, 0, 1), DegreeToRadian(DeltaEuler.Z));
					const FQuat DeltaQuat = (Qz * Qy * Qx).GetNormalized();

					if (EditingComponent)
					{
						FQuat Cur = EditingComponent->GetRelativeRotation();
						EditingComponent->SetRelativeRotation((DeltaQuat * Cur).GetNormalized());
						if (SelectedActor) SelectedActor->MarkPartitionDirty();
					}
					else if (SelectedActor)
					{
						FQuat Cur = SelectedActor->GetActorRotation();
						SelectedActor->SetActorRotation(DeltaQuat * Cur);
					}

					PrevEditRotationUI = EditRotation;
					bRotationChanged = false; // PostProcess에서 다시 적용하지 않도록
				}
				else
				{
					const FQuat NewQ = QuatFromEulerZYX_Deg(EditRotation).GetNormalized();
					if (EditingComponent)
					{
						EditingComponent->SetRelativeRotation(NewQ);
						if (SelectedActor) SelectedActor->MarkPartitionDirty();
					}
					else if (SelectedActor)
					{
						SelectedActor->SetActorRotation(NewQ);
					}

					EditRotation = EulerZYX_DegFromQuat(NewQ);
					PrevEditRotationUI = EditRotation;
					bRotationChanged = false;
				}
			}

			// 6) 편집 종료 시(포커스 빠짐) 최종 스냅 & 상태 리셋
			if (Deactivated)
			{
				if (EditingComponent)
				{
					EditRotation = EulerZYX_DegFromQuat(EditingComponent->GetRelativeRotation());
					PrevEditRotationUI = EditRotation;
				}
				else if (SelectedActor)
				{
					EditRotation = EulerZYX_DegFromQuat(SelectedActor->GetActorRotation());
					PrevEditRotationUI = EditRotation;
				}
				bRotationEditing = false;
				bRotationChanged = false;
			}
		}


		// Scale 편집
		ImGui::Checkbox("Uniform Scale", &bUniformScale);

		if (bUniformScale)
		{
			float UniformScale = EditScale.X;
			if (ImGui::DragFloat("Scale", &UniformScale, 0.01f, 0.01f, 10.0f))
			{
				EditScale = FVector(UniformScale, UniformScale, UniformScale);
				bScaleChanged = true;
			}
		}
		else
		{
			if (ImGui::DragFloat3("Scale", &EditScale.X, 0.01f, 0.01f, 10.0f))
			{
				bScaleChanged = true;
			}
		}

		ImGui::Spacing();

		ImGui::Spacing();
		ImGui::Separator();

		// BillboardComponent 전용 텍스처 선택 드롭다운
		if (USceneComponent* EditingCompBB = GetEditingComponent())
		{
			if (UBillboardComponent* Billboard = Cast<UBillboardComponent>(EditingCompBB))
			{
				ImGui::Text("Billboard Texture");

				int currentIdx = -1;
				const FString& cur = Billboard->GetTextureName();
				for (int i = 0; i < 3; ++i)
				{
					const FString item = kDisplayItems[i];
					if (cur.size() >= item.size() && cur.compare(cur.size() - item.size(), item.size(), item) == 0)
					{
						currentIdx = i;
						break;
					}
				}
				ImGui::SetNextItemWidth(240);
				if (ImGui::Combo("Texture", &currentIdx, kDisplayItems, 3))
				{
					if (currentIdx >= 0 && currentIdx < 3)
					{
						Billboard->SetTextureName(kFullPaths[currentIdx]);
					}
				}
				ImGui::Separator();
			}
		}

		// Actor가 AStaticMeshActor인 경우 StaticMesh 변경 UI
		if (UStaticMeshComponent* TargetSMC = GetEditingStaticMeshComponent())
		{
			ImGui::Separator();
			ImGui::Text("Static Mesh Override");

			FString CurrentPath;
			if (UStaticMesh* CurMesh = TargetSMC->GetStaticMesh())
			{
				CurrentPath = ToUtf8(CurMesh->GetAssetPathFileName());
				ImGui::Text("Current: %s", CurrentPath.c_str());
			}
			else
			{
				ImGui::Text("Current: <None>");
			}

			auto& RM = UResourceManager::GetInstance();
			TArray<FString> Paths = RM.GetAllStaticMeshFilePaths();

			if (Paths.empty())
			{
				ImGui::TextColored(ImVec4(1, 0.6f, 0.6f, 1), "No StaticMesh resources loaded.");
			}
			else
			{
				TArray<FString> DisplayNames;
				DisplayNames.reserve(Paths.size());
				for (const FString& p : Paths)
					DisplayNames.push_back(GetBaseNameNoExt(p));

				TArray<const char*> Items;
				Items.reserve(DisplayNames.size());
				for (const FString& n : DisplayNames)
					Items.push_back(n.c_str());

				static int SelectedMeshIdx = -1;
				if (SelectedMeshIdx == -1 && !CurrentPath.empty())
				{
					for (int i = 0; i < static_cast<int>(Paths.size()); ++i)
					{
						if (Paths[i] == CurrentPath || DisplayNames[i] == GetBaseNameNoExt(CurrentPath))
						{
							SelectedMeshIdx = i;
							break;
						}
					}
				}

				ImGui::SetNextItemWidth(240);
				ImGui::Combo("StaticMesh", &SelectedMeshIdx, Items.data(), static_cast<int>(Items.size()));
				if (ImGui::Button("Apply Mesh"))
				{
					if (SelectedMeshIdx >= 0 && SelectedMeshIdx < static_cast<int>(Paths.size()))
					{
						const FString& NewPath = Paths[SelectedMeshIdx];
						TargetSMC->SetStaticMesh(NewPath);

						if (AStaticMeshActor* SMActorOwner = Cast<AStaticMeshActor>(SelectedActor))
						{
							if (GetBaseNameNoExt(NewPath) == "Sphere")
								SMActorOwner->SetCollisionComponent(EPrimitiveType::Sphere);
							else
								SMActorOwner->SetCollisionComponent();
						}
						const FString LogPath = ToUtf8(NewPath);
						UE_LOG("Applied StaticMesh: %s", LogPath.c_str());
					}
				}

				ImGui::SameLine();
				if (ImGui::Button("Select Current"))
				{
					SelectedMeshIdx = -1;
					if (!CurrentPath.empty())
					{
						for (int i = 0; i < static_cast<int>(Paths.size()); ++i)
						{
							if (Paths[i] == CurrentPath || DisplayNames[i] == GetBaseNameNoExt(CurrentPath))
							{
								SelectedMeshIdx = i;
								break;
							}
						}
					}
				}
			}

			ImGui::Separator();

			// ---- 기존 Material UI 유지 ----
			const TArray<FString> MaterialNames = UResourceManager::GetInstance().GetAllFilePaths<UMaterial>();
			TArray<const char*> MaterialNamesCharP;
			MaterialNamesCharP.reserve(MaterialNames.size());
			for (const FString& n : MaterialNames)
				MaterialNamesCharP.push_back(n.c_str());

			if (UStaticMesh* CurMesh = TargetSMC->GetStaticMesh())
			{
				const TArray<FGroupInfo>& GroupInfos = CurMesh->GetMeshGroupInfo();
				const uint32 NumGroupInfos = static_cast<uint32>(GroupInfos.size());

				for (uint32 i = 0; i < NumGroupInfos; ++i)
				{
					ImGui::PushID(i);
					const char* Label = GroupInfos[i].InitialMaterialName.c_str();
					int SelectedMaterialIdx = -1;

					if (i < TargetSMC->GetMaterailSlots().size())
					{
						const FString& AssignedName = TargetSMC->GetMaterailSlots()[i].MaterialName.ToString();
						for (int idx = 0; idx < static_cast<int>(MaterialNames.size()); ++idx)
							if (MaterialNames[idx] == AssignedName)
							{
								SelectedMaterialIdx = idx;
								break;
							}
					}

					ImGui::SetNextItemWidth(240);
					if (ImGui::Combo(Label, &SelectedMaterialIdx, MaterialNamesCharP.data(),
						static_cast<int>(MaterialNamesCharP.size())))
					{
						if (SelectedMaterialIdx >= 0 && SelectedMaterialIdx < static_cast<int>(MaterialNames.size()))
						{
							TargetSMC->SetMaterialByUser(i, MaterialNames[SelectedMaterialIdx]);
							UE_LOG("Set material slot %u to %s", i, MaterialNames[SelectedMaterialIdx].c_str());
						}
					}
					ImGui::PopID();
				}
			}
		}
	}
}

void UTargetActorTransformWidget::PostProcess()
{
	// 자동 적용이 활성화된 경우 변경사항을 즉시 적용
	if (bPositionChanged || bRotationChanged || bScaleChanged)
	{
		ApplyTransformToActor();
		ResetChangeFlags(); // 적용 후 플래그 리셋
	}
}

void UTargetActorTransformWidget::UpdateTransformFromActor()
{
	if (!SelectedActor)
		return;

	if (USceneComponent* EditingComponent = GetEditingComponent())
	{
		EditLocation = EditingComponent->GetRelativeLocation();
		EditRotation = EulerZYX_DegFromQuat(EditingComponent->GetRelativeRotation());
		EditScale = EditingComponent->GetRelativeScale();
	}
	else
	{
		EditLocation = SelectedActor->GetActorLocation();
		EditRotation = EulerZYX_DegFromQuat(SelectedActor->GetActorRotation());
		EditScale = SelectedActor->GetActorScale();
	}

	ResetChangeFlags();
}

void UTargetActorTransformWidget::ApplyTransformToActor() const
{
	if (!SelectedActor)
		return;

	if (USceneComponent* EditingComponent = GetEditingComponent())
	{
		bool bDirty = false;

		if (bPositionChanged)
		{
			EditingComponent->SetRelativeLocation(EditLocation);
			bDirty = true;
		}

		if (bRotationChanged)
		{
			FQuat NewRotation = FQuat::MakeFromEuler(EditRotation);
			EditingComponent->SetRelativeRotation(NewRotation);
			bDirty = true;
		}

		if (bScaleChanged)
		{
			EditingComponent->SetRelativeScale(EditScale);
			bDirty = true;
		}

		if (bDirty)
		{
			SelectedActor->MarkPartitionDirty();
		}
		return;
	}

	// 기존 액터 적용 분기
	if (bPositionChanged)
	{
		SelectedActor->SetActorLocation(EditLocation);
	}

	if (bRotationChanged)
	{
		FQuat NewRotation = FQuat::MakeFromEuler(EditRotation);
		SelectedActor->SetActorRotation(NewRotation);
	}

	if (bScaleChanged)
	{
		SelectedActor->SetActorScale(EditScale);
	}
}

void UTargetActorTransformWidget::ResetChangeFlags()
{
	bPositionChanged = false;
	bRotationChanged = false;
	bScaleChanged = false;
}
