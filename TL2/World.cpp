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

UWorld& UWorld::GetInstance()
{
	static UWorld* Instance = nullptr;
	if (Instance == nullptr)
	{
		Instance = NewObject<UWorld>();
	}
	return *Instance;
}

UWorld::UWorld()
	: Partition(new UWorldPartitionManager())
{
}

UWorld::~UWorld()
{
	for (AActor* Actor : Actors)
	{
		ObjectFactory::DeleteObject(Actor);
	}
	Actors.clear();

	// Grid 정리 
	ObjectFactory::DeleteObject(GridActor);
	GridActor = nullptr;
}

void UWorld::Initialize()
{
	FObjManager::Preload();
	CreateNewScene();

	InitializeGrid();
	InitializeGizmo();
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

void UWorld::Tick(float DeltaSeconds)
{
	Partition->Update(DeltaSeconds, /*budget*/256);

	//순서 바꾸면 안댐
	for (AActor* Actor : Actors)
	{
		if (Actor) Actor->Tick(DeltaSeconds);
	}
	for (AActor* EditorActor : EditorActors)
	{
		if (EditorActor) EditorActor->Tick(DeltaSeconds);
	}
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
	if (!Actor)
	{
		return false; // nullptr 들어옴 → 실패
	}

	// SelectionManager에서 선택 해제 (메모리 해제 전에 하자)
	SELECTION.DeselectActor(Actor);

	// UIManager에서 픽된 액터 정리
	if (UI.GetPickedActor() == Actor)
	{
		UI.ResetPickedActor();
	}

	// 배열에서 제거 시도
	auto it = std::find(Actors.begin(), Actors.end(), Actor);
	if (it != Actors.end())
	{
		// 옥트리에서 제거
		OnActorDestroyed(Actor);

		Actors.erase(it);

		// 메모리 해제
		ObjectFactory::DeleteObject(Actor);

		// 삭제된 액터 정리
		SELECTION.CleanupInvalidActors();

		return true; // 성공적으로 삭제
	}

	return false; // 월드에 없는 액터
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

void UWorld::CreateNewScene()
{
	// Safety: clear interactions that may hold stale pointers
	SELECTION.ClearSelection();
	UI.ResetPickedActor();

	for (AActor* Actor : Actors)
	{
		ObjectFactory::DeleteObject(Actor);
	}
	Actors.Empty();

	// 이름 카운터 초기화: 씬을 새로 시작할 때 각 BaseName 별 suffix를 0부터 다시 시작
	ObjectTypeCounts.clear();

	Partition->Clear();
}

void UWorld::LoadScene(const FString& SceneName)
{
	namespace fs = std::filesystem;
	fs::path path = fs::path("Scene") / SceneName;
	if (path.extension().string() != ".Scene")
	{
		path.replace_extension(".Scene");
	}

	const FString FilePath = path.make_preferred().string();

	// [1] 로드 시작 전 현재 카운터 백업
	const uint32 PreLoadNext = UObject::PeekNextUUID();

	// [2] 파일 NextUUID는 현재보다 클 때만 반영(절대 하향 설정 금지)
	uint32 LoadedNextUUID = 0;
	if (FSceneLoader::TryReadNextUUID(FilePath, LoadedNextUUID))
	{
		if (LoadedNextUUID > UObject::PeekNextUUID())
		{
			UObject::SetNextUUID(LoadedNextUUID);
		}
	}

	// [3] 기존 씬 비우기
	CreateNewScene();

    // [4] 로드
    FPerspectiveCameraData CamData{};
    const TArray<FPrimitiveData>& Primitives = FSceneLoader::Load(FilePath, &CamData);

    // 마우스 델타 초기화
    const FVector2D CurrentMousePos = INPUT.GetMousePosition();
	INPUT.SetLastMousePosition(CurrentMousePos);

    // 카메라 적용
    if (MainCameraActor && MainCameraActor->GetCameraComponent())
    {
        UCameraComponent* Cam = MainCameraActor->GetCameraComponent();

        // 위치/회전(월드 트랜스폼)
        MainCameraActor->SetActorLocation(CamData.Location);
        MainCameraActor->SetActorRotation(FQuat::MakeFromEuler(CamData.Rotation));

        // 입력 경로와 동일한 방식으로 각도/회전 적용
        // 매핑: Pitch = CamData.Rotation.Y, Yaw = CamData.Rotation.Z
        MainCameraActor->SetAnglesImmediate(CamData.Rotation.Y, CamData.Rotation.Z);

		// UIManager의 카메라 회전 상태도 동기화
		UI.UpdateMouseRotation(CamData.Rotation.Y, CamData.Rotation.Z);

        // 프로젝션 파라미터
        Cam->SetFOV(CamData.FOV);
        Cam->SetClipPlanes(CamData.NearClip, CamData.FarClip);

		// UI 위젯에 현재 카메라 상태로 재동기화 요청
		UI.SyncCameraControlFromCamera();
    }

	// 1) 현재 월드에서 이미 사용 중인 UUID 수집(엔진 액터 + 기즈모)
	std::unordered_set<uint32> UsedUUIDs;
	auto AddUUID = [&](AActor* A) { if (A) UsedUUIDs.insert(A->UUID); };
	for (AActor* Eng : EditorActors) 
	{
		AddUUID(Eng);
	}
	AddUUID(GizmoActor); // Gizmo는 EngineActors에 안 들어갈 수 있으므로 명시 추가

	uint32 MaxAssignedUUID = 0;
	// 벌크 삽입을 위해 액터들을 먼저 모두 생성
	TArray<AActor*> SpawnedActors;
	SpawnedActors.reserve(Primitives.size());

	for (const FPrimitiveData& Primitive : Primitives)
	{
		// 스폰 시 필요한 초기 트랜스포은 그대로 넘김
		AStaticMeshActor* StaticMeshActor = SpawnActor<AStaticMeshActor>(
			FTransform(Primitive.Location,
				SceneRotUtil::QuatFromEulerZYX_Deg(Primitive.Rotation),
				Primitive.Scale));

		// 스폰 시점에 자동 발급된 고유 UUID (충돌 시 폴백으로 사용)
		uint32 Assigned = StaticMeshActor->UUID;

		// 우선 스폰된 UUID를 사용 중으로 등록
		UsedUUIDs.insert(Assigned);

		// 2) 파일의 UUID를 우선 적용하되, 충돌이면 스폰된 UUID 유지
		if (Primitive.UUID != 0)
		{
			if (UsedUUIDs.find(Primitive.UUID) == UsedUUIDs.end())
			{
				// 스폰된 ID 등록 해제 후 교체
				UsedUUIDs.erase(Assigned);
				StaticMeshActor->UUID = Primitive.UUID;
				Assigned = Primitive.UUID;
				UsedUUIDs.insert(Assigned);
			}
			else
			{
				// 충돌: 파일 UUID 사용 불가 → 경고 로그 및 스폰된 고유 UUID 유지
				UE_LOG("LoadScene: UUID collision detected (%u). Keeping generated %u for actor.",
					Primitive.UUID, Assigned);
			}
		}

		MaxAssignedUUID = std::max(MaxAssignedUUID, Assigned);

		if (UStaticMeshComponent* SMC = StaticMeshActor->GetStaticMeshComponent())
		{
			FPrimitiveData Temp = Primitive;
			SMC->Serialize(true, Temp);

			FString LoadedAssetPath;
			if (UStaticMesh* Mesh = SMC->GetStaticMesh())
			{
				LoadedAssetPath = Mesh->GetAssetPathFileName();
			}

			if (LoadedAssetPath == "Data/Sphere.obj")
			{
				StaticMeshActor->SetCollisionComponent(EPrimitiveType::Sphere);
			}
			else
			{
				StaticMeshActor->SetCollisionComponent();
			}

			FString BaseName = "StaticMesh";
			if (!LoadedAssetPath.empty())
			{
				BaseName = RemoveObjExtension(LoadedAssetPath);
			}
			StaticMeshActor->SetName(GenerateUniqueActorName(BaseName));
		}
		// 벌크 삽입을 위해 목록에 추가
		SpawnedActors.push_back(StaticMeshActor);
	}
	
	// 모든 액터를 한 번에 벌크 등록 하여 성능 최적화
	if (!SpawnedActors.empty())
	{
		UE_LOG("LoadScene: Using bulk registration for %zu actors\r\n", SpawnedActors.size());
		Partition->BulkRegister(SpawnedActors);
	}

	// 3) 최종 보정: 전역 카운터는 절대 하향 금지 + 현재 사용된 최대값 이후로 설정
	const uint32 DuringLoadNext = UObject::PeekNextUUID();
	const uint32 SafeNext = std::max({ DuringLoadNext, MaxAssignedUUID + 1, PreLoadNext });
	UObject::SetNextUUID(SafeNext);
}

void UWorld::SaveScene(const FString& SceneName)
{
	TArray<FPrimitiveData> Primitives;

    for (AActor* Actor : Actors)
    {
        if (AStaticMeshActor* MeshActor = Cast<AStaticMeshActor>(Actor))
        {
            FPrimitiveData Data;
            Data.UUID = Actor->UUID;
            Data.Type = "StaticMeshComp";
            if (UStaticMeshComponent* SMC = MeshActor->GetStaticMeshComponent())
            {
                SMC->Serialize(false, Data); // 여기서 RotUtil 적용됨(상위 Serialize)
            }
            Primitives.push_back(Data);
        }
        else
        {
            FPrimitiveData Data;
            Data.UUID = Actor->UUID;
            Data.Type = "Actor";

            if (UPrimitiveComponent* Prim = Cast<UPrimitiveComponent>(Actor->GetRootComponent()))
            {
                Prim->Serialize(false, Data); // 여기서 RotUtil 적용됨
            }
            else
            {
                // 루트가 Primitive가 아닐 때도 동일 규칙으로 저장
                Data.Location = Actor->GetActorLocation();
                Data.Rotation = SceneRotUtil::EulerZYX_Deg_FromQuat(Actor->GetActorRotation());
                Data.Scale = Actor->GetActorScale();
            }

            Data.ObjStaticMeshAsset.clear();
            Primitives.push_back(Data);
        }
    }

    // 카메라 데이터 채우기
    const FPerspectiveCameraData* CamPtr = nullptr;
    FPerspectiveCameraData CamData;
    if (MainCameraActor && MainCameraActor->GetCameraComponent())
    {
        UCameraComponent* Cam = MainCameraActor->GetCameraComponent();

        CamData.Location = MainCameraActor->GetActorLocation();

        // 내부 누적 각도로 저장: Pitch=Y, Yaw=Z, Roll=0
        CamData.Rotation.X = 0.0f;
        CamData.Rotation.Y = MainCameraActor->GetCameraPitch();
        CamData.Rotation.Z = MainCameraActor->GetCameraYaw();

        CamData.FOV = Cam->GetFOV();
        CamData.NearClip = Cam->GetNearClip();
        CamData.FarClip = Cam->GetFarClip();
        CamPtr = &CamData;
    }

    // Scene 디렉터리에 저장
    FSceneLoader::Save(Primitives, CamPtr, SceneName);
}

AGizmoActor* UWorld::GetGizmoActor()
{
	return GizmoActor;
}

