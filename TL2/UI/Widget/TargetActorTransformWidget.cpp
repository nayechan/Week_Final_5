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
using namespace std;

//// UE_LOG 대체 매크로
//#define UE_LOG(fmt, ...)


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
				Result.push_back({ "Camera Component", UCameraComponent::StaticClass(), "카메라 뷰/프로젝션 제공" });
				Result.push_back({ "Text Render Component", UTextRenderComponent::StaticClass(), "빌보드 텍스트 표시" });
				Result.push_back({ "Line Component", ULineComponent::StaticClass(), "라인/디버그 드로잉" });
				Result.push_back({ "AABB Component", UAABoundingBoxComponent::StaticClass(), "바운딩 박스 시각화" });
				return Result;
			}();
		return Options;
	}
	bool TryAttachComponentToActor(AActor& Actor, UClass* ComponentClass)
	{
		if (!ComponentClass || !ComponentClass->IsChildOf(USceneComponent::StaticClass()))
		{
			return false;
		}

		UObject* RawObject = ObjectFactory::NewObject(ComponentClass);
		if (!RawObject)
		{
			return false;
		}

		USceneComponent* NewComponent = Cast<USceneComponent>(RawObject);
		if (!NewComponent)
		{
			ObjectFactory::DeleteObject(RawObject);
			return false;
		}

		NewComponent->SetOwner(&Actor);
		NewComponent->SetWorldTransform(Actor.GetActorTransform());
		Actor.AddComponent(NewComponent);

		if (USceneComponent* const Root = Actor.GetRootComponent())
		{
			if (Root != NewComponent)
			{
				NewComponent->SetupAttachment(Root, EAttachmentRule::KeepRelative);
			}
		}

		NewComponent->InitializeComponent();
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

		FString Label = Component->GetClass() ? Component->GetClass()->Name : "Unknown Component";
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
			const bool bCanRemove = (Component != Actor.GetRootComponent());
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
	
	// GizmoActor 참조 획득
	GizmoActor = UIManager->GetGizmoActor();
	
	// 초기 기즈모 스페이스 모드 설정
	if (GizmoActor)
	{
		CurrentGizmoSpace = GizmoActor->GetSpace();
	}

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
			}
			catch (...)
			{
				CachedActorName = "[Invalid Actor]";
				SelectedActor = nullptr;
			}
		}
		else
		{
			CachedActorName = "";
		}
	}

	// GizmoActor 참조 업데이트
	if (!GizmoActor && UIManager)
	{
		GizmoActor = UIManager->GetGizmoActor();
	}

	if (SelectedActor)
	{
		// 액터가 선택되어 있으면 항상 트랜스폼 정보를 업데이트하여
		// 기즈모 조작을 실시간으로 UI에 반영합니다.
		UpdateTransformFromActor();
	}
	
	// 월드 정보 업데이트 (옵션)
	if (UIManager && UIManager->GetWorld())
	{
		UWorld* World = UIManager->GetWorld();
		WorldActorCount = static_cast<uint32>(World->GetActors().size());
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
		// 월드 정보 표시
		ImGui::Text("World Information");
		ImGui::Text("Actor Count: %u", WorldActorCount);
		ImGui::Separator();

		AGridActor* gridActor = UIManager->GetWorld()->GetGridActor();
		if (gridActor)
		{
			float currentLineSize = gridActor->GetLineSize();
			if (ImGui::DragFloat("Grid Spacing", &currentLineSize, 0.1f, 0.1f, 1000.0f))
			{
				gridActor->SetLineSize(currentLineSize);
				EditorINI["GridSpacing"] = std::to_string(currentLineSize);
			}
		}
		else
		{
			ImGui::Text("GridActor not found in the world.");
		}

		ImGui::Text("Transform Editor");

		SelectedActor = GetCurrentSelectedActor();


		// 기즈모 스페이스 모드 선택
		if (GizmoActor)
		{
			const char* spaceItems[] = { "World", "Local" };
			int currentSpaceIndex = static_cast<int>(CurrentGizmoSpace);

			if (ImGui::Combo("Gizmo Space", &currentSpaceIndex, spaceItems, IM_ARRAYSIZE(spaceItems)))
			{
				CurrentGizmoSpace = static_cast<EGizmoSpace>(currentSpaceIndex);

				GizmoActor->SetSpaceWorldMatrix(CurrentGizmoSpace, SelectedActor);
			}
			ImGui::Separator();
		}
	}
	// 컴포넌트 관리 UI
	if (SelectedActor)
	{
		ImGui::Text("Components");
		ImGui::SameLine();
		if (ImGui::Button("+ Add"))
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
		const bool bActorSelected = (SelectedActor != nullptr && SelectedComponent == nullptr);

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

			const TArray<USceneComponent*>& AllComponents = SelectedActor->GetComponents();
			for (USceneComponent* Component : AllComponents)
			{
				if (!Component || VisitedComponents.count(Component))
				{
					//UE_LOG("Skipping already visited or null component.");
					continue;
				}

				ImGui::PushID(Component);
				const bool bSelected = (Component == SelectedComponent);
				if (ImGui::Selectable(Component->GetClass()->Name, bSelected))
				{
					SelectedComponent = Component; // 하이라이트 유지
				}

				if (ImGui::BeginPopupContextItem("ComponentContext"))
				{
					// 루트 컴포넌트가 아닌 경우에만 제거 가능
					const bool bCanRemove = (Component != RootComponent);
					if (ImGui::MenuItem("삭제", "Delete", false, bCanRemove))
					{
						ComponentPendingRemoval = Component;
					}
					ImGui::EndPopup();
				}
				ImGui::PopID();
			}
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
			else if (SelectedComponent != RootComponent)
			{
				// 컴포넌트 선택 상태(루트 아님) → 해당 컴포넌트 삭제
				ComponentPendingRemoval = SelectedComponent;
			}
			else
			{
				// 루트 컴포넌트가 선택된 상태면 삭제 불가
			}
		}

		if (ComponentPendingRemoval)
		{
			SelectedActor->RemoveComponent(ComponentPendingRemoval);
			if (SelectedComponent == ComponentPendingRemoval)
			{
				SelectedComponent = nullptr;
			}
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


		ImGui::Separator();
		ImGui::Spacing();
		
		// Location 편집
		if (ImGui::DragFloat3("Location", &EditLocation.X, 0.1f))
		{
			bPositionChanged = true;
		}
		
		// Rotation 편집 (Euler angles)
		if (ImGui::DragFloat3("Rotation", &EditRotation.X, 0.5f))
		{
			bRotationChanged = true;
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
		
		// 실시간 적용 버튼
		if (ImGui::Button("Apply Transform"))
		{
			ApplyTransformToActor();
		}
		
		ImGui::SameLine();
		if (ImGui::Button("Reset Transform"))
		{
			UpdateTransformFromActor();
			ResetChangeFlags();
		}
		
		// 기즈모 스페이스 빠른 전환 버튼
		if (GizmoActor)
		{
			ImGui::Separator();
			const char* buttonText = CurrentGizmoSpace == EGizmoSpace::World ? 
				"Switch to Local" : "Switch to World";
			
			if (ImGui::Button(buttonText))
			{
				// 스페이스 모드 전환
				CurrentGizmoSpace = (CurrentGizmoSpace == EGizmoSpace::World) ? 
					EGizmoSpace::Local : EGizmoSpace::World;
				
				// 기즈모 액터에 스페이스 설정 적용
				GizmoActor->SetSpaceWorldMatrix(CurrentGizmoSpace, SelectedActor);
			}
			
			ImGui::SameLine();
			ImGui::Text("Current: %s", 
				CurrentGizmoSpace == EGizmoSpace::World ? "World" : "Local");
		}
		
		ImGui::Spacing();
		ImGui::Separator();

		// Actor가 AStaticMeshActor인 경우 StaticMesh 변경 UI
		{
			if (AStaticMeshActor* SMActor = Cast<AStaticMeshActor>(SelectedActor))
			{
				UStaticMeshComponent* SMC = SMActor->GetStaticMeshComponent();

				ImGui::Text("Static Mesh Override");
				if (!SMC)
				{
					ImGui::TextColored(ImVec4(1, 0.6f, 0.6f, 1), "StaticMeshComponent not found.");
				}
				else
				{
					// 현재 메시 경로 표시
					FString CurrentPath;
					UStaticMesh* CurMesh = SMC->GetStaticMesh();
					if (CurMesh)
					{
						CurrentPath = CurMesh->GetAssetPathFileName();
						ImGui::Text("Current: %s", CurrentPath.c_str());
					}
					else
					{
						ImGui::Text("Current: <None>");
					}

					// 리소스 매니저에서 로드된 모든 StaticMesh 경로 수집
					auto& RM = UResourceManager::GetInstance();
					TArray<FString> Paths = RM.GetAllStaticMeshFilePaths();

					if (Paths.empty())
					{
						ImGui::TextColored(ImVec4(1, 0.6f, 0.6f, 1), "No StaticMesh resources loaded.");
					}
					else
					{
						// 표시용 이름(파일명 스템)
						TArray<FString> DisplayNames;
						DisplayNames.reserve(Paths.size());
						for (const FString& p : Paths)
							DisplayNames.push_back(GetBaseNameNoExt(p));

						// ImGui 콤보 아이템 배열
						TArray<const char*> Items;
						Items.reserve(DisplayNames.size());
						for (const FString& n : DisplayNames)
							Items.push_back(n.c_str());

						// 선택 인덱스 유지
						static int SelectedMeshIdx = -1;

						// 기본 선택: Cube가 있으면 자동 선택
						if (SelectedMeshIdx == -1)
						{
							for (int i = 0; i < static_cast<int>(Paths.size()); ++i)
							{
								if (DisplayNames[i] == "Cube" || Paths[i] == "Data/Cube.obj")
								{
									SelectedMeshIdx = i;
									break;
								}
							}
						}

						ImGui::SetNextItemWidth(240);
						ImGui::Combo("StaticMesh", &SelectedMeshIdx, Items.data(), static_cast<int>(Items.size()));
						ImGui::SameLine();
						if (ImGui::Button("Apply Mesh"))
						{
							if (SelectedMeshIdx >= 0 && SelectedMeshIdx < static_cast<int>(Paths.size()))
							{
								const FString& NewPath = Paths[SelectedMeshIdx];
								SMC->SetStaticMesh(NewPath);

								// Sphere 충돌 특례
								if (GetBaseNameNoExt(NewPath) == "Sphere")
									SMActor->SetCollisionComponent(EPrimitiveType::Sphere);
								else
									SMActor->SetCollisionComponent();

								UE_LOG("Applied StaticMesh: %s", NewPath.c_str());
							}
						}

						// 현재 메시로 선택 동기화 버튼 (옵션)
						ImGui::SameLine();
						if (ImGui::Button("Select Current"))
						{
							SelectedMeshIdx = -1;
							if (!CurrentPath.empty())
							{
								for (int i = 0; i < static_cast<int>(Paths.size()); ++i)
								{
									if (Paths[i] == CurrentPath ||
										DisplayNames[i] == GetBaseNameNoExt(CurrentPath))
									{
										SelectedMeshIdx = i;
										break;
									}
								}
							}
						}
					}

					// Material 설정
					ImGui::Separator();

					const TArray<FString> MaterialNames = UResourceManager::GetInstance().GetAllFilePaths<UMaterial>();
					// ImGui 콤보 아이템 배열
					TArray<const char*> MaterialNamesCharP;
					MaterialNamesCharP.reserve(MaterialNames.size());
					for (const FString& n : MaterialNames)
						MaterialNamesCharP.push_back(n.c_str());

					if (CurMesh)
					{
						const uint64 MeshGroupCount = CurMesh->GetMeshGroupCount();

						static TArray<int32> SelectedMaterialIdxAt; // i번 째 Material Slot이 가지고 있는 MaterialName이 MaterialNames의 몇번쩨 값인지.
						if (SelectedMaterialIdxAt.size() < MeshGroupCount)
						{
							SelectedMaterialIdxAt.resize(MeshGroupCount);
						}

						// 현재 SMC의 MaterialSlots 정보를 UI에 반영
						const TArray<FMaterialSlot>& MaterialSlots = SMC->GetMaterailSlots();
						for (uint64 MaterialSlotIndex = 0; MaterialSlotIndex < MeshGroupCount; ++MaterialSlotIndex)
						{
							for (uint32 MaterialIndex = 0; MaterialIndex < MaterialNames.size(); ++MaterialIndex)
							{
								if (MaterialSlots[MaterialSlotIndex].MaterialName == MaterialNames[MaterialIndex])
								{
									SelectedMaterialIdxAt[MaterialSlotIndex] = MaterialIndex;
								}
							}
						}

						// Material 선택
						for (uint64 MaterialSlotIndex = 0; MaterialSlotIndex < MeshGroupCount; ++MaterialSlotIndex)
						{
							ImGui::PushID(static_cast<int>(MaterialSlotIndex));
							if (ImGui::Combo("Material", &SelectedMaterialIdxAt[MaterialSlotIndex], MaterialNamesCharP.data(), static_cast<int>(MaterialNamesCharP.size())))
							{
								SMC->SetMaterialByUser(static_cast<uint32>(MaterialSlotIndex), MaterialNames[SelectedMaterialIdxAt[MaterialSlotIndex]]);
							}
							ImGui::PopID();
						}
					}
				}
			}
			else
			{
				ImGui::Text("Selected actor is not a StaticMeshActor.");
			}
		}
	}
	else
	{
		ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "No Actor Selected");
		ImGui::TextUnformatted("Select an actor to edit its transform.");
	}
	
	ImGui::Separator();
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
		
	// 액터의 현재 트랜스폼을 UI 변수로 복사
	EditLocation = SelectedActor->GetActorLocation();
	EditRotation = SelectedActor->GetActorRotation().ToEuler();
	EditScale = SelectedActor->GetActorScale();
	
	// 균등 스케일 여부 판단
	bUniformScale = (abs(EditScale.X - EditScale.Y) < 0.01f && 
	                abs(EditScale.Y - EditScale.Z) < 0.01f);
	
	ResetChangeFlags();
}

void UTargetActorTransformWidget::ApplyTransformToActor() const
{
	if (!SelectedActor)
		return;
		
	// 변경사항이 있는 경우에만 적용
	if (bPositionChanged)
	{
		SelectedActor->SetActorLocation(EditLocation);
		UE_LOG("Transform: Applied location (%.2f, %.2f, %.2f)", 
		       EditLocation.X, EditLocation.Y, EditLocation.Z);
	}
	
	if (bRotationChanged)
	{
		FQuat NewRotation = FQuat::MakeFromEuler(EditRotation);
		SelectedActor->SetActorRotation(NewRotation);
		UE_LOG("Transform: Applied rotation (%.1f, %.1f, %.1f)", 
		       EditRotation.X, EditRotation.Y, EditRotation.Z);
	}
	
	if (bScaleChanged)
	{
		SelectedActor->SetActorScale(EditScale);
		UE_LOG("Transform: Applied scale (%.2f, %.2f, %.2f)", 
		       EditScale.X, EditScale.Y, EditScale.Z);
	}
	
	// 플래그 리셋은 const 메서드에서 할 수 없으므로 PostProcess에서 처리
}

void UTargetActorTransformWidget::ResetChangeFlags()
{
	bPositionChanged = false;
	bRotationChanged = false;
	bScaleChanged = false;
}
