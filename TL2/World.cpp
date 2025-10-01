#include "pch.h"
#include "SelectionManager.h"
#include "Picking.h"
#include "SceneLoader.h"
#include "CameraActor.h"
#include "StaticMeshActor.h"
#include "CameraComponent.h"
#include "ObjectFactory.h"
#include "TextRenderComponent.h"
#include "AABoundingBoxComponent.h"
#include "FViewport.h"
#include "SViewportWindow.h"
#include "USlateManager.h"
#include "StaticMesh.h"
#include "ObjManager.h"
#include "SceneRotationUtils.h"
#include "WorldPartitionManager.h"
#include "PrimitiveComponent.h"
#include "Octree.h"
#include "BVHierachy.h"
#include "Frustum.h"
#include "Occlusion.h"
#include "GizmoActor.h"
#include "GridActor.h"
#include "StaticMeshComponent.h"
#include "Frustum.h"
#include "Level.h"

UWorld::UWorld()
	: Partition(new UWorldPartitionManager())
{
	SelectionMgr = std::make_unique<USelectionManager>();
	Level = std::make_unique<ULevel>();
	//FObjManager::Preload();
	CreateLevel();

	InitializeGrid();
	InitializeGizmo();
}

UWorld::~UWorld()
{
if (Level)
	{
		for (AActor* Actor : Level->GetActors())
		{
			ObjectFactory::DeleteObject(Actor);
		}
		Level->Clear();
	}
	for (AActor* Actor : EditorActors)
	{
		ObjectFactory::DeleteObject(Actor);
	}
	EditorActors.clear();

	GridActor = nullptr;
	GizmoActor = nullptr;
}

void UWorld::InitializeGrid()
{
	GridActor = NewObject<AGridActor>();
	GridActor->SetWorld(this);
	GridActor->Initialize();

	EditorActors.push_back(GridActor);
}

void UWorld::InitializeGizmo()
{
	GizmoActor = NewObject<AGizmoActor>();
	GizmoActor->SetWorld(this);
	GizmoActor->SetActorTransform(FTransform(FVector{ 0, 0, 0 }, FQuat::MakeFromEuler(FVector{ 0, -90, 0 }),
		FVector{ 1, 1, 1 }));

	EditorActors.push_back(GizmoActor);
}

void UWorld::Tick(float DeltaSeconds, EWorldType InWorldType)
{
	Partition->Update(DeltaSeconds, /*budget*/256);

//순서 바꾸면 안댐
	if (Level)
	{
		for (AActor* Actor : Level->GetActors())
		{
			if (Actor && (Actor->CanTickInEditor() || InWorldType == EWorldType::Game))
			{
				Actor->Tick(DeltaSeconds);
			}
		}
	}
	for (AActor* EditorActor : EditorActors)
	{
		if (EditorActor && InWorldType == EWorldType::Editor) EditorActor->Tick(DeltaSeconds);
	}
}

UWorld* UWorld::DuplicateWorldForPIE(UWorld* InEditorWorld)
{
	// 레벨 새로 생성
	// 월드 카피 및 월드에 이 새로운 레벨 할당
	// 월드 컨텍스트 새로 생성(월드타입, 카피한 월드)
	// 월드의 레벨에 원래 Actor들 다 복사
	// 해당 월드의 Initialize 호출?

	//ULevel* NewLevel = ULevelService::CreateNewLevel();
	UWorld* PIEWorld = NewObject<UWorld>(); // 레벨도 새로 생성됨
	
	FWorldContext PIEWorldContext = FWorldContext(PIEWorld, EWorldType::Game);
	GEngine.AddWorldContext(PIEWorldContext);

	const TArray<AActor*>& SourceActors = InEditorWorld->GetLevel()->GetActors();
	for (AActor* SourceActor : SourceActors)
	{
		if (!SourceActor)
		{
			UE_LOG("Duplicate failed: SourceActor is nullptr");
			continue;
		}

		AActor* NewActor = SourceActor->Duplicate();
		if (!NewActor)
		{
			UE_LOG("Duplicate failed: NewActor is nullptr");
			continue;
		}
		PIEWorld->AddActorToLevel(NewActor);
	}

	return PIEWorld;
}

FString UWorld::GenerateUniqueActorName(const FString& ActorType)
{
	// GetInstance current count for this type
	int32& CurrentCount = ObjectTypeCounts[ActorType];
	FString UniqueName = ActorType + "_" + std::to_string(CurrentCount);
	CurrentCount++;
	return UniqueName;
}

//
// 액터 제거
//
bool UWorld::DestroyActor(AActor* Actor)
{
	if (!Actor) return false;

	// 재진입 가드
	if (Actor->IsPendingDestroy()) return false;
	Actor->MarkPendingDestroy();

	// 선택/UI 해제
	if (SelectionMgr) SelectionMgr->DeselectActor(Actor);
	if (UI.GetPickedActor() == Actor)
		UI.ResetPickedActor();

	// 게임 수명 종료
	Actor->EndPlay(EEndPlayReason::Destroyed);

	// 컴포넌트 정리 (등록 해제 → 파괴)
	Actor->UnregisterAllComponents(/*bCallEndPlayOnBegun=*/true);
	Actor->DestroyAllComponents();
	Actor->ClearSceneComponentCaches();

	// 월드 자료구조에서 제거 (옥트리/파티션/렌더 캐시 등)
	OnActorDestroyed(Actor);

// 레벨에서 제거 시도
	if (Level && Level->RemoveActor(Actor))
	{
		// 옥트리에서 제거
		OnActorDestroyed(Actor);

		// 메모리 해제
		ObjectFactory::DeleteObject(Actor);

		// 삭제된 액터 정리
		if (SelectionMgr) SelectionMgr->CleanupInvalidActors();

		return true; // 성공적으로 삭제
	}

	return false; // 레벨에 없는 액터
}

void UWorld::OnActorSpawned(AActor* Actor)
{
	if (Actor)
	{
		Partition->Register(Actor);
	}
}

void UWorld::OnActorDestroyed(AActor* Actor)
{
	if (Actor)
	{
		Partition->Unregister(Actor);
	}
}

inline FString RemoveObjExtension(const FString& FileName)
{
	const FString Extension = ".obj";

	// 마지막 경로 구분자 위치 탐색 (POSIX/Windows 모두 지원)
	const uint64 Sep = FileName.find_last_of("/\\");
	const uint64 Start = (Sep == FString::npos) ? 0 : Sep + 1;

	// 확장자 제거 위치 결정
	uint64 End = FileName.size();
	if (End >= Extension.size() &&
		FileName.compare(End - Extension.size(), Extension.size(), Extension) == 0)
	{
		End -= Extension.size();
	}

	// 베이스 이름(확장자 없는 파일명) 반환
	if (Start <= End)
		return FileName.substr(Start, End - Start);

	// 비정상 입력 시 원본 반환 (안전장치)
	return FileName;
}

void UWorld::CreateLevel()
{
	if (SelectionMgr) SelectionMgr->ClearSelection();
	UI.ResetPickedActor();
	 
	SetLevel(ULevelService::CreateNewLevel());
	// 이름 카운터 초기화: 씬을 새로 시작할 때 각 BaseName 별 suffix를 0부터 다시 시작
	ObjectTypeCounts.clear();
}

void UWorld::SetLevel(std::unique_ptr<ULevel> InLevel)
{
    // Make UI/selection safe before destroying previous actors
    if (SelectionMgr) SelectionMgr->ClearSelection();
    UI.ResetPickedActor();

    // Cleanup current
    if (Level)
    {
        for (AActor* Actor : Level->GetActors())
        {
            ObjectFactory::DeleteObject(Actor);
        }
        Level->Clear();
    }
    // Clear spatial indices
    Partition->Clear();

    Level = std::move(InLevel);

    // Adopt actors: set world and register
    if (Level)
    {
		Partition->BulkRegister(Level->GetActors());
        for (AActor* A : Level->GetActors())
        {
            if (!A) continue;
            A->SetWorld(this);
        }
    }

    // Clean any dangling selection references just in case
    if (SelectionMgr) SelectionMgr->CleanupInvalidActors();
}

void UWorld::AddActorToLevel(AActor* Actor)
{
	if (Level) 
	{
		Level->AddActor(Actor);
		Partition->Register(Actor);
	}
}

