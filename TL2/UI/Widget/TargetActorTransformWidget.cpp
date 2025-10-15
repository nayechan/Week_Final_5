#include "pch.h"
#include <string>
#include "TargetActorTransformWidget.h"
#include "UI/UIManager.h"
#include "ImGui/imgui.h"
#include "Vector.h"
#include "World.h"
#include "ResourceManager.h"    
#include "WorldPartitionManager.h"
#include "ActorSpawnWidget.h"

#include "Actor.h"
#include "GridActor.h"
#include "GizmoActor.h"
#include "StaticMeshActor.h"    
#include "FakeSpotLightActor.h"    

#include "StaticMeshComponent.h"
#include "TextRenderComponent.h"
#include "CameraComponent.h"
#include "BillboardComponent.h"
#include "DecalComponent.h"
#include "PerspectiveDecalComponent.h"
#include "FireBallComponent.h"

using namespace std;

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
				Result.push_back({ "Decal Component", UDecalComponent::StaticClass(), "데칼" });
				Result.push_back({ "FireBall Component", UFireBallComponent::StaticClass(), "파이어볼" });
				return Result;
			}();
		return Options;
	}

	bool TryAttachComponentToActor(AActor& Actor, UClass* ComponentClass, USceneComponent*& SelectedComponent)
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

		// 씬 컴포넌트라면 SelectedComponent에 붙임
		if (USceneComponent* SceneComp = Cast<USceneComponent>(NewComp))
		{
			if (SelectedComponent)
			{
				SceneComp->SetupAttachment(SelectedComponent, EAttachmentRule::KeepRelative);
			}
			// SelectedComponent가 없으면 루트에 붙이
			else if (USceneComponent* Root = Actor.GetRootComponent())
			{
				SceneComp->SetupAttachment(Root, EAttachmentRule::KeepRelative);
			}

			SelectedComponent = SceneComp;
		}

		// AddOwnedComponent 경유 (Register/Initialize 포함)
		Actor.AddOwnedComponent(NewComp);

		// UStaticMeshComponent라면 World Partition에 추가. (null 체크는 Register 내부에서 수행)
		if (UWorld* ActorWorld = Actor.GetWorld())
		{
			if (UWorldPartitionManager* Partition = ActorWorld->GetPartitionManager())
			{
				Partition->Register(Cast<UStaticMeshComponent>(NewComp));
			}
		}
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
			ImGuiTreeNodeFlags_SpanAvailWidth |
			ImGuiTreeNodeFlags_DefaultOpen;

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
			const bool bCanRemove = !Component->IsNative();
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
	// 1. 마지막 경로 구분자('/' 또는 '\')를 찾아 파일 이름의 시작 위치를 결정합니다.
	const size_t sep = Path.find_last_of("/\\");
	const size_t start = (sep == FString::npos) ? 0 : sep + 1;

	const FString ext = ".obj";
	size_t end = Path.size();

	// 2. 경로의 길이가 비교할 확장자보다 긴지 확인합니다.
	if (end >= ext.size())
	{
		// 3. 경로의 끝에서 확장자 길이만큼의 문자열을 복사합니다.
		FString PathExt = Path.substr(end - ext.size());

		// 4. 복사된 확장자 문자열을 모두 소문자로 변환합니다.
		std::transform(PathExt.begin(), PathExt.end(), PathExt.begin(),
			[](unsigned char c) { return static_cast<char>(std::tolower(c)); });

		// 5. 소문자로 변환된 문자열을 ".obj"와 비교하여 일치하면 파일명의 끝 위치를 조정합니다.
		if (PathExt == ext)
		{
			end -= ext.size();
		}
	}

	// 6. 계산된 시작과 끝 위치를 기반으로 최종 파일 이름을 잘라내어 반환합니다.
	if (start <= end)
	{
		return Path.substr(start, end - start);
	}

	// 예외적인 경우(경로 구분자만 있는 경우 등)를 대비해 안전한 값을 반환합니다.
	return Path.substr(start);
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
	if (!SelectedActor)
	{
		return;
	}

	// 캐시 갱신
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
		return;
	}

	// 1. 헤더 (액터 이름, "+추가" 버튼) 렌더링
	RenderHeader();

	// 2. 컴포넌트 계층 구조 렌더링
	RenderComponentHierarchy();

	// 3. 트랜스폼 편집 UI (Location, Rotation, Scale) 렌더링
	RenderTransformEditor();

	// 4. 선택된 컴포넌트의 상세 정보 렌더링
	RenderSelectedComponentDetails();
}

// -----------------------------------------------------------------------------
// 헤더 UI 렌더링
// -----------------------------------------------------------------------------
void UTargetActorTransformWidget::RenderHeader()
{
	ImGui::Text(CachedActorName.c_str());
	ImGui::SameLine();

	const float ButtonWidth = 60.0f;
	const float ButtonHeight = 25.0f;
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
				if (TryAttachComponentToActor(*SelectedActor, Descriptor.Class, SelectedComponent))
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
}

// -----------------------------------------------------------------------------
// [신규 분리 함수 2] 컴포넌트 계층 구조 UI 렌더링
// -----------------------------------------------------------------------------
void UTargetActorTransformWidget::RenderComponentHierarchy()
{
	AActor* ActorPendingRemoval = nullptr;
	USceneComponent* ComponentPendingRemoval = nullptr;

	// 컴포넌트 트리 박스 크기 관련
	static float PaneHeight = 120.0f;
	const float SplitterThickness = 6.0f;
	const float MinTop = 1.0f;
	const float MinBottom = 0.0f;
	const float WindowAvailY = ImGui::GetContentRegionAvail().y;
	PaneHeight = std::clamp(PaneHeight, MinTop, std::max(MinTop, WindowAvailY - (MinBottom + SplitterThickness)));

	ImGui::BeginChild("ComponentBox", ImVec2(0, PaneHeight), true);

	// 액터 행 렌더링
	ImGui::PushID("ActorDisplay");
	ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.85f, 0.3f, 1.0f));
	const bool bActorSelected = (SelectedComponent == nullptr);
	if (ImGui::Selectable(CachedActorName.c_str(), bActorSelected, ImGuiSelectableFlags_SelectOnClick | ImGuiSelectableFlags_SpanAvailWidth))
	{
		SelectedComponent = nullptr;
	}
	ImGui::PopStyleColor();
	if (ImGui::BeginPopupContextItem("ActorContextMenu"))
	{
		if (ImGui::MenuItem("삭제", "Delete", false, true))
		{
			ActorPendingRemoval = SelectedActor;
		}
		ImGui::EndPopup();
	}
	ImGui::PopID();

	// 컴포넌트 트리 렌더링
	USceneComponent* RootComponent = SelectedActor->GetRootComponent();
	if (!RootComponent)
	{
		ImGui::BulletText("Root component not found.");
	}
	else
	{
		TSet<USceneComponent*> VisitedComponents;
		RenderSceneComponentTree(RootComponent, *SelectedActor, SelectedComponent, ComponentPendingRemoval, VisitedComponents);
		// ... (루트에 붙지 않은 씬 컴포넌트 렌더링 로직은 생략 가능성 있음, 엔진 설계에 따라)
	}

	// 선택 변경 시 트랜스폼 값 업데이트
	static USceneComponent* PreviousSelectedComponent = nullptr;
	if (PreviousSelectedComponent != SelectedComponent)
	{
		UpdateTransformFromActor();
		PrevEditRotationUI = EditRotation;
		bRotationEditing = false;
		const FVector ScaleRef = EditScale;
		bUniformScale = (std::fabs(ScaleRef.X - ScaleRef.Y) < 0.01f && std::fabs(ScaleRef.Y - ScaleRef.Z) < 0.01f);
	}
	PreviousSelectedComponent = SelectedComponent;

	// 삭제 입력 처리
	const bool bDeletePressed = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && ImGui::IsKeyPressed(ImGuiKey_Delete);
	if (bDeletePressed)
	{
		if (bActorSelected) ActorPendingRemoval = SelectedActor;
		else if (SelectedComponent && !SelectedComponent->IsNative()) ComponentPendingRemoval = SelectedComponent;
	}

	// 컴포넌트 삭제 실행
	if (ComponentPendingRemoval)
	{
		if (SelectedComponent == ComponentPendingRemoval)
			SelectedComponent = nullptr;

		if (ComponentPendingRemoval->GetAttachParent())
		{
			SelectedComponent = ComponentPendingRemoval->GetAttachParent();
		}
		else
		{
			SelectedComponent = ComponentPendingRemoval->GetOwner()->RootComponent;
		}

		SelectedActor->RemoveOwnedComponent(ComponentPendingRemoval);
		ComponentPendingRemoval = nullptr;
	}

	// 액터 삭제 실행
	if (ActorPendingRemoval)
	{
		if (UWorld* World = ActorPendingRemoval->GetWorld()) World->DestroyActor(ActorPendingRemoval);
		else ActorPendingRemoval->Destroy();
		OnSelectedActorCleared(); // 여기서 SelectedActor가 nullptr이 되므로 루프가 안전하게 종료됨
	}

	ImGui::EndChild();

	// 스플리터 렌더링
	ImGui::InvisibleButton("VerticalSplitter", ImVec2(-FLT_MIN, SplitterThickness));
	if (ImGui::IsItemHovered() || ImGui::IsItemActive()) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
	if (ImGui::IsItemActive()) PaneHeight += ImGui::GetIO().MouseDelta.y;
	ImVec2 Min = ImGui::GetItemRectMin(), Max = ImGui::GetItemRectMax();
	ImGui::GetWindowDrawList()->AddLine(ImVec2(Min.x, (Min.y + Max.y) * 0.5f), ImVec2(Max.x, (Min.y + Max.y) * 0.5f), ImGui::GetColorU32(ImGuiCol_Separator), 1.0f);
	ImGui::Spacing();
}

// -----------------------------------------------------------------------------
// 트랜스폼 편집 UI 렌더링
// -----------------------------------------------------------------------------
void UTargetActorTransformWidget::RenderTransformEditor()
{
	// Location 편집
	if (ImGui::DragFloat3("Location", &EditLocation.X, 0.1f))
	{
		bPositionChanged = true;
	}

	// Rotation 편집
	// ───────── Rotation: DragFloat3 하나로 "드래그=증분", "입력=절대" 처리 ─────────
	{
		USceneComponent* EditingComponent = GetEditingComponent();

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

				const FQuat Qx = FQuat::FromAxisAngle(FVector(1, 0, 0), DegreesToRadians(DeltaEuler.X));
				const FQuat Qy = FQuat::FromAxisAngle(FVector(0, 1, 0), DegreesToRadians(DeltaEuler.Y));
				const FQuat Qz = FQuat::FromAxisAngle(FVector(0, 0, 1), DegreesToRadians(DeltaEuler.Z));
				const FQuat DeltaQuat = (Qz * Qy * Qx).GetNormalized();

				if (EditingComponent)
				{
					FQuat Cur = EditingComponent->GetRelativeRotation();
					EditingComponent->SetRelativeRotation((DeltaQuat * Cur).GetNormalized());
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
				const FQuat NewQ = FQuat::MakeFromEulerZYX(EditRotation).GetNormalized();
				if (EditingComponent)
				{
					EditingComponent->SetRelativeRotation(NewQ);
				}
				else if (SelectedActor)
				{
					SelectedActor->SetActorRotation(NewQ);
				}

				EditRotation = NewQ.ToEulerZYXDeg();
				PrevEditRotationUI = EditRotation;
				bRotationChanged = false;
			}
		}

		// 6) 편집 종료 시(포커스 빠짐) 최종 스냅 & 상태 리셋
		if (Deactivated)
		{
			if (EditingComponent)
			{
				EditRotation = EditingComponent->GetRelativeRotation().ToEulerZYXDeg();
				PrevEditRotationUI = EditRotation;
			}
			else if (SelectedActor)
			{
				EditRotation = SelectedActor->GetActorRotation().ToEulerZYXDeg();
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
}

// -----------------------------------------------------------------------------
// 선택된 컴포넌트 상세 정보 UI 렌더링
// -----------------------------------------------------------------------------
void UTargetActorTransformWidget::RenderSelectedComponentDetails()
{
	ImGui::Spacing();
	ImGui::Separator();

	USceneComponent* TargetComponentForDetails = SelectedComponent;

	// 액터가 선택되 경우 액터의 중요 컴포넌트를 출력
	if (!TargetComponentForDetails && SelectedActor)
	{
		if (AStaticMeshActor* StaticMeshActor = Cast<AStaticMeshActor>(SelectedActor))
		{
			TargetComponentForDetails = StaticMeshActor->GetStaticMeshComponent();
		}
		else if (AFakeSpotLightActor* FakeSpotLightActor = Cast<AFakeSpotLightActor>(SelectedActor))
		{
			TargetComponentForDetails = FakeSpotLightActor->GetDecalComponent();
		}
		else
		{
			TargetComponentForDetails = SelectedActor->GetRootComponent();
		}
	}

	if (!TargetComponentForDetails) return;

	// BillboardComponent UI
	if (UBillboardComponent* Billboard = Cast<UBillboardComponent>(TargetComponentForDetails))
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

	// StaticMeshComponent UI
	if (UStaticMeshComponent* TargetSMC = Cast<UStaticMeshComponent>(TargetComponentForDetails))
	{
		ImGui::Separator();
		ImGui::Text("Static Mesh Override");

		FString CurrentPath;
		if (UStaticMesh* CurMesh = TargetSMC->GetStaticMesh())
		{
			CurrentPath = ToUtf8(CurMesh->GetAssetPathFileName());
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

			// 매 프레임마다 현재 메시에 맞는 인덱스를 찾습니다.
			int SelectedMeshIdx = -1;
			if (!CurrentPath.empty())
			{
				for (int i = 0; i < static_cast<int>(Paths.size()); ++i)
				{
					if (Paths[i] == CurrentPath)
					{
						SelectedMeshIdx = i;
						break;
					}
				}
			}

			ImGui::SetNextItemWidth(240);

			// ImGui::Combo가 true를 반환하면(선택이 바뀌면) 즉시 메시를 적용합니다.
			if (ImGui::Combo("StaticMesh", &SelectedMeshIdx, Items.data(), static_cast<int>(Items.size())))
			{
				if (SelectedMeshIdx >= 0 && SelectedMeshIdx < static_cast<int>(Paths.size()))
				{
					const FString& NewPath = Paths[SelectedMeshIdx];
					TargetSMC->SetStaticMesh(NewPath);

					const FString LogPath = ToUtf8(NewPath);
					UE_LOG("Applied StaticMesh: %s", LogPath.c_str());
				}
			}
		}

		ImGui::Separator();

		// Material UI
		if (UStaticMesh* CurMesh = TargetSMC->GetStaticMesh())
		{
			const TArray<FString> MaterialNames = UResourceManager::GetInstance().GetAllFilePaths<UMaterial>();
			TArray<const char*> MaterialNamesCharP;
			MaterialNamesCharP.reserve(MaterialNames.size());
			for (const FString& n : MaterialNames)
				MaterialNamesCharP.push_back(n.c_str());

			const TArray<FGroupInfo>& GroupInfos = CurMesh->GetMeshGroupInfo();
			const uint32 NumGroupInfos = static_cast<uint32>(GroupInfos.size());

			for (uint32 i = 0; i < NumGroupInfos; ++i)
			{
				ImGui::PushID(static_cast<int>(i));
				const char* Label = GroupInfos[i].InitialMaterialName.c_str();
				int SelectedMaterialIdx = -1;

				if (i < TargetSMC->GetMaterialSlots().size())
				{
					const FString& AssignedName = TargetSMC->GetMaterialSlots()[i].MaterialName.ToString();
					for (int idx = 0; idx < static_cast<int>(MaterialNames.size()); ++idx)
					{
						if (MaterialNames[idx] == AssignedName)
						{
							SelectedMaterialIdx = idx;
							break;
						}
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

	// DecalComponent UI
	if (UDecalComponent* DecalCmp = Cast<UDecalComponent>(TargetComponentForDetails))
	{
		ImGui::Separator();
		ImGui::Text("Set Decal Texture");

		FString CurrentPath;
		if (UTexture* Texture = DecalCmp->GetDecalTexture())
		{
			CurrentPath = ToUtf8(Texture->GetTextureName());
			ImGui::Text("Current: %s", CurrentPath.c_str());
		}
		else
		{
			ImGui::Text("Current: <None>");
		}

		const TArray<FString> TexturePaths = UResourceManager::GetInstance().GetAllFilePaths<UTexture>();

		if (TexturePaths.empty())
		{
			ImGui::TextColored(ImVec4(1, 0.6f, 0.6f, 1), "No Texture resources loaded.");
		}
		else
		{
			// const char* 배열 생성
			TArray<const char*> TexturePathsCharP;
			TexturePathsCharP.reserve(TexturePaths.size());
			for (const FString& n : TexturePaths)
				TexturePathsCharP.push_back(n.c_str());

			// CurrentPath와 일치하는 인덱스를 찾습니다.
			int SelectedTextureIdx = -1;
			for (int i = 0; i < TexturePaths.size(); ++i)
			{
				if (TexturePaths[i] == CurrentPath)
				{
					SelectedTextureIdx = i;
					break; // 찾았으면 루프 종료
				}
			}

			// Texture 목록 출력
			if (ImGui::Combo("Texture", &SelectedTextureIdx, TexturePathsCharP.data(),
				static_cast<int>(TexturePathsCharP.size())))
			{
				if (SelectedTextureIdx >= 0 && SelectedTextureIdx < static_cast<int>(TexturePaths.size()))
				{
					DecalCmp->SetDecalTexture(TexturePaths[SelectedTextureIdx]);
					UE_LOG("Set Decal Texture to %s", TexturePaths[SelectedTextureIdx].c_str());
				}
			}
		}

		ImGui::Separator();
		ImGui::Text("Set Opacity");

		float DecalBlendFactor = DecalCmp->GetOpacity();
		ImGui::SliderFloat("Blend Factor", &DecalBlendFactor, 0.0f, 1.0f);
		DecalCmp->SetOpacity(DecalBlendFactor);
	}

	// PerspectiveDecalComponent UI
	if (UPerspectiveDecalComponent* PerDecalComp = Cast<UPerspectiveDecalComponent>(TargetComponentForDetails))
	{
		ImGui::Separator();
		// UI 헤더 텍스트를 "원뿔 데칼 속성"으로 변경
		ImGui::Text("원뿔 데칼 속성 (Perspective Decal)");

		// 1. Getter를 호출하여 현재 FovY 값을 가져옵니다. 이 값이 원뿔의 꼭지각(apex angle)입니다.
		float coneAngle = PerDecalComp->GetFovY();

		// 2. ImGui 슬라이더의 라벨을 "원뿔 각도"로 변경하고, 로컬 변수 coneAngle의 주소를 전달합니다.
		//    사용자가 더 다양한 형태를 만들 수 있도록 범위를 1.0 ~ 80.0으로 확장합니다.
		if (ImGui::SliderFloat("Cone Angle", &coneAngle, 1.0f, 80.0f, "%.1f deg"))
		{
			// 3. 값이 변경되면 Setter를 호출하여 컴포넌트의 실제 각도를 업데이트합니다.
			PerDecalComp->SetFovY(coneAngle);
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
		EditRotation = EditingComponent->GetRelativeRotation().ToEulerZYXDeg();
		EditScale = EditingComponent->GetRelativeScale();
	}
	else
	{
		EditLocation = SelectedActor->GetActorLocation();
		EditRotation = SelectedActor->GetActorRotation().ToEulerZYXDeg();
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
			FQuat NewRotation = FQuat::MakeFromEulerZYX(EditRotation);
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
			SelectedActor->GetRootComponent()->OnTransformUpdated();
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
		FQuat NewRotation = FQuat::MakeFromEulerZYX(EditRotation);
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
