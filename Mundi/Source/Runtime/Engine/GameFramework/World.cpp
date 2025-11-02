#include "pch.h"
#include "SelectionManager.h"
#include "Picking.h"
#include "CameraActor.h"
#include "StaticMeshActor.h"
#include "CameraComponent.h"
#include "ObjectFactory.h"
#include "TextRenderComponent.h"
#include "FViewport.h"
#include "Windows/SViewportWindow.h"
#include "USlateManager.h"
#include "StaticMesh.h"
#include "ObjManager.h"
#include "WorldPartitionManager.h"
#include "PrimitiveComponent.h"
#include "Octree.h"
#include "BVHierarchy.h"
#include "Frustum.h"
#include "Occlusion.h"
#include "Gizmo/GizmoActor.h"
#include "Grid/GridActor.h"
#include "StaticMeshComponent.h"
#include "DirectionalLightActor.h"
#include "Frustum.h"
#include "Level.h"
#include "LightManager.h"
#include "../Scripting/LuaManager.h"
#include "ShapeComponent.h"

IMPLEMENT_CLASS(UWorld)

UWorld::UWorld() : Partition(new UWorldPartitionManager())
{
	SelectionMgr = std::make_unique<USelectionManager>();
	//PIE의 경우 Initalize 없이 빈 Level 생성만 해야함
	Level = std::make_unique<ULevel>();
	LightManager = std::make_unique<FLightManager>();
	LuaManager = std::make_unique<FLuaManager>();
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

void UWorld::Initialize()
{
	// 최근에 사용한 레벨 불러오기를 시도합니다.
	if (!TryLoadLastUsedLevel())
	{
		// 실패 시 (또는 저장된 레벨이 없을 시) 기본 씬을 생성합니다.
		CreateLevel();
	}

	// 에디터 전용 액터들을 초기화합니다.
	InitializeGrid();
	InitializeGizmo();
}

void UWorld::InitializeGrid()
{
	GridActor = NewObject<AGridActor>();
	GridActor->SetWorld(this);
	GridActor->RegisterAllComponents(this);
	GridActor->Initialize();

	EditorActors.push_back(GridActor);
}

void UWorld::InitializeGizmo()
{
	GizmoActor = NewObject<AGizmoActor>();
	GizmoActor->SetWorld(this);
	GizmoActor->RegisterAllComponents(this);
	GizmoActor->SetActorTransform(FTransform(FVector{ 0, 0, 0 }, FQuat::MakeFromEulerZYX(FVector{ 0, -90, 0 }),
		FVector{ 1, 1, 1 }));

	EditorActors.push_back(GizmoActor);
}

bool UWorld::TryLoadLastUsedLevel()
{
	if (!EditorINI.Contains("LastUsedLevel"))
	{
		return false;
	}

	FWideString LastUsedLevelPath = UTF8ToWide(EditorINI["LastUsedLevel"]);
	
	// 로드 직전: Transform 위젯/선택 초기화
	UUIManager::GetInstance().ClearTransformWidgetSelection();
	GWorld->GetSelectionManager()->ClearSelection();

	std::unique_ptr<ULevel> NewLevel = ULevelService::CreateDefaultLevel();
	JSON LevelJsonData;
	if (FJsonSerializer::LoadJsonFromFile(LevelJsonData, LastUsedLevelPath))
	{
		NewLevel->Serialize(true, LevelJsonData);
	}
	else
	{
		UE_LOG("[error] MainToolbar: Failed To Load Level From: %s", LastUsedLevelPath.c_str());
		return false;
	}

	SetLevel(std::move(NewLevel));

	UE_LOG("MainToolbar: Scene loaded successfully: %s", LastUsedLevelPath.c_str());
	return true;
}

void UWorld::Tick(float DeltaSeconds)
{
    Partition->Update(DeltaSeconds, /*budget*/256);

//순서 바꾸면 안댐
	if (Level)
	{
		// Tick 중에 새로운 actor가 추가될 수도 있어서 복사 후 호출
		TArray<AActor*> LevelActors = Level->GetActors();
		for (AActor* Actor : LevelActors)
		{
			if (Actor && (Actor->CanTickInEditor() || bPie))
			{
				Actor->Tick(DeltaSeconds);
			}
		}
    }
    for (AActor* EditorActor : EditorActors)
    {
        if (EditorActor && !bPie) EditorActor->Tick(DeltaSeconds);
    }

	if (LuaManager && bPie)
	{
		LuaManager->Tick(DeltaSeconds);
	}

    // Update overlaps each tick to ensure symmetry and handle non-moving participants
    //if (Level)
    //{
    //    for (AActor* Actor : Level->GetActors())
    //    {
    //        if (!Actor) continue;
    //        for (USceneComponent* Comp : Actor->GetSceneComponents())
    //        {
    //            if (UShapeComponent* Shape = Cast<UShapeComponent>(Comp))
    //            {
    //                if (Shape->IsActive())
    //                {
    //                    Shape->UpdateOverlaps();
    //                }
    //            }
    //        }
    //    }
    //}
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
	PIEWorld->bPie = true;
	
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

	// 게임 수명 종료
	Actor->EndPlay();

	// 컴포넌트 정리 (등록 해제 → 파괴)
	TArray<USceneComponent*> Components = Actor->GetSceneComponents();
	for(USceneComponent* Comp : Components)
	{
		if (Comp)
		{
			Comp->SetOwner(nullptr); // 소유자 해제
		}
	}

	Actor->UnregisterAllComponents(/*bCallEndPlayOnBegun=*/true);
	Actor->DestroyAllComponents();

	// 레벨에서 제거 시도
	if (Level && Level->RemoveActor(Actor))
	{
		// 메모리 해제
		ObjectFactory::DeleteObject(Actor);

		// 삭제된 액터 정리
		if (SelectionMgr)
		{
			SelectionMgr->CleanupInvalidActors();
			SelectionMgr->ClearSelection();
		}

		return true; // 성공적으로 삭제
	}

	return false; // 레벨에 없는 액터
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
	
	SetLevel(ULevelService::CreateNewLevel());
	// 이름 카운터 초기화: 씬을 새로 시작할 때 각 BaseName 별 suffix를 0부터 다시 시작
	ObjectTypeCounts.clear();
}

//어느 레벨이든 기본적으로 존재하는 엑터(디렉셔널 라이트) 생성
void UWorld::SpawnDefaultActors()
{
	SpawnActor<ADirectionalLightActor>();
}

void UWorld::SetLevel(std::unique_ptr<ULevel> InLevel)
{
    // Make UI/selection safe before destroying previous actors
    if (SelectionMgr) SelectionMgr->ClearSelection();

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
			if (A)
			{
				A->SetWorld(this);
				A->RegisterAllComponents(this);
			}
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

		Actor->SetWorld(this);

		Actor->RegisterAllComponents(this);
	}
}

AActor* UWorld::SpawnActor(UClass* Class, const FTransform& Transform)
{
	// 유효성 검사: Class가 유효하고 AActor를 상속했는지 확인
	if (!Class || !Class->IsChildOf(AActor::StaticClass()))
	{
		UE_LOG("SpawnActor failed: Invalid class provided.");
		return nullptr;
	}

	// ObjectFactory를 통해 UClass*로부터 객체 인스턴스 생성
	AActor* NewActor = Cast<AActor>(ObjectFactory::NewObject(Class));
	if (!NewActor)
	{
		UE_LOG("SpawnActor failed: ObjectFactory could not create an instance of");
		return nullptr;
	}

	// 초기 트랜스폼 적용
	NewActor->SetActorTransform(Transform);

	// 현재 레벨에 액터 등록
	AddActorToLevel(NewActor);

	if (this->bPie)
	{
		NewActor->BeginPlay();
	}

	return NewActor;
}

AActor* UWorld::SpawnActor(UClass* Class)
{
	// 기본 Transform(원점)으로 스폰하는 메인 함수를 호출합니다.
	return SpawnActor(Class, FTransform());
}

AActor* UWorld::SpawnPrefabActor(const FWideString& PrefabPath)
{
	JSON ActorDataJson;

	if (FJsonSerializer::LoadJsonFromFile(ActorDataJson, PrefabPath))
	{
		// Pair.first는 ID 문자열, Pair.second는 단일 프리미티브의 JSON 데이터입니다.

		FString TypeString;
		if (FJsonSerializer::ReadString(ActorDataJson, "Type", TypeString))
		{
			//UClass* NewClass = FActorTypeMapper::TypeToActor(TypeString);
			UClass* NewClass = UClass::FindClass(TypeString);

			UWorld* World = GWorld;

			// 유효성 검사: Class가 유효하고 AActor를 상속했는지 확인
			if (!NewClass || !NewClass->IsChildOf(AActor::StaticClass()))
			{
				UE_LOG("[error] SpawnActor failed: Invalid class provided.");
				return nullptr;
			}

			// ObjectFactory를 통해 UClass*로부터 객체 인스턴스 생성
			AActor* NewActor = Cast<AActor>(ObjectFactory::NewObject(NewClass));
			if (!NewActor)
			{
				UE_LOG("[error] SpawnActor failed: ObjectFactory could not create an instance of");
				return nullptr;
			}
			// 월드 참조 설정
			NewActor->Serialize(true, ActorDataJson);
			AddActorToLevel(NewActor);

			if (this->bPie)
			{
				NewActor->BeginPlay();
			}

			return NewActor;
		}
	}
	else
	{
		UE_LOG("[error] 존재하지 않는 Prefab 경로입니다. - %s", WideToUTF8(PrefabPath).c_str());
	}

	return nullptr;
}
