#include "pch.h"
#include "WorldPartitionManager.h"
#include "PrimitiveComponent.h"
#include "Actor.h"
#include "AABoundingBoxComponent.h"
#include "World.h"
#include "Octree.h"
#include "StaticMeshActor.h"

namespace {
	inline bool ShouldIndexActor(const AActor* Actor)
	{
		// 현재 Bounding Box가 Primitive Component가 아닌 Actor에 종속
		// 추후 컴포넌트 별 처리 가능하게 수정 필
		return Actor && Actor->IsA<AStaticMeshActor>();
	}
}

UWorldPartitionManager::UWorldPartitionManager()
{
	FBound WorldBounds(FVector(-800.f, -800.f, -800.f), FVector(800.f, 800.f, 800.f));
	SceneOctree = new FOctree(WorldBounds, 0, 8, 8);
}

UWorldPartitionManager::~UWorldPartitionManager()
{
	if (SceneOctree)
	{
		delete SceneOctree;
		SceneOctree = nullptr;
	}
}

void UWorldPartitionManager::Clear()
{
}

void UWorldPartitionManager::Register(AActor* Owner)
{
	if (!Owner) return;
	if (!ShouldIndexActor(Owner)) return;
	if (DirtySet.insert(Owner).second)
	{
		DirtyQueue.push(Owner);
	}
}

void UWorldPartitionManager::BulkRegister(const TArray<AActor*>& Actors)
{
	if (Actors.empty()) return;

	// 첫 번째 액터에서 World 참조 취득
	UWorld* World = nullptr;
	for (AActor* Actor : Actors)
	{
		if (Actor && ShouldIndexActor(Actor))
		{
			World = Actor->GetWorld();
			break;
		}
	}

	if (!World) return;
	//FOctree* Tree = World->GetOctree();
	if (!SceneOctree) return;

	// 읽어와 바운드를 한 번에 수집
	TArray<std::pair<AActor*, FBound>> ActorsAndBounds;
	ActorsAndBounds.reserve(Actors.size());

	for (AActor* Actor : Actors)
	{
		if (Actor && ShouldIndexActor(Actor))
		{
			ActorsAndBounds.push_back({ Actor, Actor->GetBounds() });
		}
	}

	// 벌크 삽입 사용
	SceneOctree->BulkInsert(ActorsAndBounds);
}

void UWorldPartitionManager::Unregister(AActor* Owner)
{
	if (!Owner) return;
	if (!ShouldIndexActor(Owner)) return;
	if (SceneOctree == nullptr) return;
	
	SceneOctree->Remove(Owner);

	if (USceneComponent* Root = Owner->GetRootComponent())
	{
		DirtySet.erase(Owner);
	}
}

void UWorldPartitionManager::MarkDirty(AActor* Owner)
{
	if (!Owner) return;
	if (!ShouldIndexActor(Owner)) return;

	if (DirtySet.insert(Owner).second)
	{
		DirtyQueue.push(Owner);
	}
}

void UWorldPartitionManager::Update(float DeltaTime, uint32 InBugetCount)
{
	// 프레임 히칭 방지를 위해 컴포넌트 카운트 제한
	uint32 processed = 0;
	while (!DirtyQueue.empty() && processed < InBugetCount)
	{
		AActor* Actor = DirtyQueue.front();
		DirtyQueue.pop();
		if (DirtySet.erase(Actor) == 0)
		{
			// 이미 처리되었거나 제거됨
			continue;
		}

		if (!Actor) continue;

		if (UWorld* World = Actor->GetWorld())
		{
			//if (FOctree* Tree = World->GetOctree())
			//{
			SceneOctree->Update(Actor);
			//}
		}

		++processed;
	}
}

void UWorldPartitionManager::ClearSceneOctree()
{
	if (SceneOctree)
	{
		SceneOctree->Clear();
	}
}

void UWorldPartitionManager::Query(FRay InRay)
{

}

void UWorldPartitionManager::Query(FBound InBound)
{
}
