#include "pch.h"
#include "ActorSpawnWidget.h"
#include "UIManager.h" // UIManager 경로 수정 필요 시 확인
#include "ImGui/imgui.h"
#include "World.h"
#include "StaticMeshActor.h"
#include "FakeSpotLightActor.h"
#include "StaticMeshComponent.h"
#include "CameraActor.h"
#include "DecalActor.h"
#include "AmbientLightActor.h"
#include "DirectionalLightActor.h"
#include "PointLightActor.h"
#include "SpotLightActor.h"
#include "SelectionManager.h"
#include <ctime>
#include <cstdlib>

IMPLEMENT_CLASS(UActorSpawnWidget)

namespace
{
	// 스폰 가능한 액터 정보를 담는 구조체 (변경 없음)
	struct FSpawnableActorDescriptor
	{
		const char* Label;
		UClass* Class;
		const char* Description;
	};

	// 스폰 가능한 액터 클래스 목록을 반환하는 함수 (리플렉션 기반 자동 수집)
	const TArray<FSpawnableActorDescriptor>& GetSpawnableActorDescriptors()
	{
		static TArray<FSpawnableActorDescriptor> Options = []()
			{
				TArray<FSpawnableActorDescriptor> Result;

				// 리플렉션 시스템을 통해 자동으로 스폰 가능한 액터 목록 가져오기
				TArray<UClass*> SpawnableActors = UClass::GetAllSpawnableActors();

				for (UClass* Class : SpawnableActors)
				{
					if (Class && Class->bIsSpawnable && Class->DisplayName)
					{
						Result.push_back({
							Class->DisplayName,
							Class,
							Class->Description ? Class->Description : ""
						});
					}
				}

				// 리플렉션에 등록되지 않은 레거시 액터들 (하위 호환성)
				// 리플렉션 시스템이 완전히 적용되면 이 부분은 제거 가능
				if (Result.IsEmpty())
				{
					Result.push_back({ "Empty Actor", AActor::StaticClass(), "컴포넌트가 없는 기본 액터입니다." });
					Result.push_back({ "Static Mesh Actor", AStaticMeshActor::StaticClass(), "스태틱 메시를 표시하는 액터입니다. (기본 Cube 메시로 생성)" });
					Result.push_back({ "Decal Actor", ADecalActor::StaticClass(), "데칼 액터입니다." });
					Result.push_back({ "Fake Spot Light Actor", AFakeSpotLightActor::StaticClass(), "가짜 Spot Light 액터입니다." });
					Result.push_back({ "Ambient Light Actor", AAmbientLightActor::StaticClass(), "전역 환경광 액터입니다. 씬 전체에 균일하게 적용됩니다." });
					Result.push_back({ "Directional Light Actor", ADirectionalLightActor::StaticClass(), "방향성 조명 액터입니다. 태양광처럼 평행한 빛을 생성합니다." });
					Result.push_back({ "Point Light Actor", APointLightActor::StaticClass(), "점광원 액터입니다. 모든 방향으로 균등하게 빛을 방출합니다." });
					Result.push_back({ "Spot Light Actor", ASpotLightActor::StaticClass(), "스포트라이트 액터입니다. 원뿔 형태로 빛을 방출합니다." });
				}

				return Result;
			}();
		return Options;
	}
}

UActorSpawnWidget::UActorSpawnWidget()
	: UWidget("Actor Spawn Widget")
{
	// 랜덤 시드 초기화
	srand(static_cast<unsigned int>(time(nullptr)));

	UIManager = &UUIManager::GetInstance();
}

UActorSpawnWidget::~UActorSpawnWidget() = default;

void UActorSpawnWidget::Initialize()
{
}

void UActorSpawnWidget::Update()
{
	// 필요 시 업데이트 로직 추가
}

// RenderWidget 함수를 새 UI 흐름에 맞게 재작성
void UActorSpawnWidget::RenderWidget()
{
	// 버튼 크기
	const float ButtonWidth = 60.0f;
	const float ButtonHeight = 25.0f;

	// '+ 추가' 버튼
	if (ImGui::Button("+ 액터", ImVec2(ButtonWidth, ButtonHeight)))
	{
		ImGui::OpenPopup("AddActorPopup");
	}

	// 팝업 메뉴 정의
	if (ImGui::BeginPopup("AddActorPopup"))
	{
		ImGui::TextUnformatted("Add Actor");
		ImGui::Separator();

		// 스폰 가능한 액터 목록을 순회하며 메뉴 아이템 생성
		for (const FSpawnableActorDescriptor& Descriptor : GetSpawnableActorDescriptors())
		{
			ImGui::PushID(Descriptor.Label); // 각 항목에 고유 ID 부여
			if (ImGui::Selectable(Descriptor.Label))
			{
				// 항목 선택 시 액터 스폰 시도
				if (TrySpawnActor(Descriptor.Class))
				{
					ImGui::CloseCurrentPopup(); // 성공 시 팝업 닫기
				}
			}
			// 항목에 마우스를 올렸을 때 설명 툴팁 표시
			if (Descriptor.Description && ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("%s", Descriptor.Description);
			}
			ImGui::PopID();
		}
		ImGui::EndPopup();
	}
}

bool UActorSpawnWidget::TrySpawnActor(UClass* ActorClass)
{
	UWorld* World = GWorld;
	if (!World)
	{
		UE_LOG("ActorSpawn: World is not available.");
		return false;
	}

	if (!ActorClass)
	{
		UE_LOG("ActorSpawn: Actor class is null.");
		return false;
	}

	// 액터 스폰 위치 및 기본 트랜스폼 설정
	FTransform SpawnTransform(GenerateRandomLocation(), FQuat::Identity(), FVector::One());

	// UClass와 Transform을 사용하여 액터 스폰
	AActor* NewActor = World->SpawnActor(ActorClass, SpawnTransform);

	if (NewActor)
	{
		// 액터에 고유 이름 부여
		const FSpawnableActorDescriptor* Desc = nullptr;
		for (const auto& ActorDesc : GetSpawnableActorDescriptors())
		{
			if (ActorDesc.Class == ActorClass)
			{
				Desc = &ActorDesc;
				break;
			}
		}

		FString ActorName = World->GenerateUniqueActorName(Desc ? Desc->Label : "Actor");
		NewActor->SetName(ActorName);

		// 월드 파티션 시스템에 등록 (중요)
		World->OnActorSpawned(NewActor); 
		HandleActorSelection(NewActor);
		UE_LOG("ActorSpawn: Successfully spawned %s.", ActorName.c_str());
		return true;
	}
	else
	{
		UE_LOG("ActorSpawn: Failed to spawn actor.");
		return false;
	}
}

// 액터 선택 처리
void UActorSpawnWidget::HandleActorSelection(AActor* Actor)
{
	if (!Actor)
		return;

	UWorld* World = UIManager ? UIManager->GetWorld() : nullptr;

	if (World)
	{
		// Clear previous selection and select this actor
		World->GetSelectionManager()->ClearSelection();
		World->GetSelectionManager()->SelectActor(Actor);
	}

	// Sync with UIManager for gizmo positioning
	//if (UIManager)
	//{
	//	UIManager->SetPickedActor(Actor);

	//	if (AGizmoActor* Gizmo = GEngine.GetDefaultWorld()->GetGizmoActor())
	//	{
	//		Gizmo->SetActorLocation(Actor->GetActorLocation());
	//	}
	//}

	UE_LOG("SceneManager: Selected actor %s", Actor->GetName().ToString().c_str());
}

FVector UActorSpawnWidget::GenerateRandomLocation() const
{
	float RandomX = SpawnRangeMin + (static_cast<float>(rand()) / RAND_MAX) * (SpawnRangeMax - SpawnRangeMin);
	float RandomY = SpawnRangeMin + (static_cast<float>(rand()) / RAND_MAX) * (SpawnRangeMax - SpawnRangeMin);
	float RandomZ = SpawnRangeMin + (static_cast<float>(rand()) / RAND_MAX) * (SpawnRangeMax - SpawnRangeMin);
	return FVector(RandomX, RandomY, RandomZ);
}

void UActorSpawnWidget::PostProcess()
{
	// 렌더링 후 처리 로직 (필요 시 작성)
}