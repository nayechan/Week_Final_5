#include "pch.h"
#include "WorldPartitionManager.h"
#include "PrimitiveComponent.h"
#include "Actor.h"
#include "AABoundingBoxComponent.h"
#include "World.h"
#include "Octree.h"
#include "BVHierachy.h"
#include "StaticMeshActor.h"
#include "Frustum.h"


namespace 
{
	inline bool ShouldIndexActor(const AActor* Actor)
	{
		// 현재 Bounding Box가 Primitive Component가 아닌 Actor에 종속
		// 추후 컴포넌트 별 처리 가능하게 수정 필
		return Actor && Actor->IsA<AStaticMeshActor>();
	}
}

UWorldPartitionManager::UWorldPartitionManager()
{
	//FBound WorldBounds(FVector(-50, -50, -50), FVector(50, 50, 50));
	FAABB WorldBounds(FVector(-50, -50, -50), FVector(50, 50, 50));
	SceneOctree = new FOctree(WorldBounds, 0, 8, 10);
	// BVH도 동일 월드 바운드로 초기화 (더 깊고 작은 리프 설정)
	//BVH = new FBVHierachy(FBound(), 0, 5, 1); 
	BVH = new FBVHierachy(FAABB(), 0, 8, 1); 
	//BVH = new FBVHierachy(FBound(), 0, 10, 3);
}

UWorldPartitionManager::~UWorldPartitionManager()
{
	if (SceneOctree)
	{
		delete SceneOctree;
		SceneOctree = nullptr;
	}
	if (BVH)
	{
		delete BVH;
		BVH = nullptr;
	}
}

void UWorldPartitionManager::Clear()
{
	//ClearSceneOctree();
	ClearBVHierachy();

	DirtyQueue.Empty();
	DirtySet.Empty();
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

	TArray<std::pair<AActor*, FAABB>> ActorsAndBounds;
	ActorsAndBounds.reserve(Actors.size());

	for (AActor* Actor : Actors)
	{
		if (Actor && ShouldIndexActor(Actor))
		{
			ActorsAndBounds.push_back({ Actor, Actor->GetBounds() });
		}
		DirtySet.erase(Actor);
	}

	// Octree: 기존 대량 삽입
	//if (SceneOctree) SceneOctree->BulkInsert(ActorsAndBounds);
	if (BVH) BVH->BulkInsert(ActorsAndBounds);
}

void UWorldPartitionManager::Unregister(AActor* Owner)
{
	if (!Owner) return;
	if (!ShouldIndexActor(Owner)) return;
	
	//if (SceneOctree) SceneOctree->Remove(Owner);
	if (BVH) BVH->Remove(Owner);

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
		//if (SceneOctree) SceneOctree->Update(Actor);
		if (BVH) BVH->Update(Actor);

		++processed;
	}
}

//void UWorldPartitionManager::RayQueryOrdered(FRay InRay, OUT TArray<std::pair<AActor*, float>>& Candidates)
//{
//    if (SceneOctree)
//    {
//        SceneOctree->QueryRayOrdered(InRay, Candidates);
//    }
//}

void UWorldPartitionManager::RayQueryClosest(FRay InRay, OUT AActor*& OutActor, OUT float& OutBestT)
{
    OutActor = nullptr;
    //if (SceneOctree)
    //{
    //    SceneOctree->QueryRayClosest(InRay, OutActor, OutBestT);
    //}
	if (BVH)
	{
		BVH->QueryRayClosest(InRay, OutActor, OutBestT);
	}
}

void UWorldPartitionManager::FrustumQuery(Frustum InFrustum)
{
	if(BVH)
	{
		BVH->QueryFrustum(InFrustum);
	}
}

void UWorldPartitionManager::ClearSceneOctree()
{
	if (SceneOctree)
	{
		SceneOctree->Clear();
	}
}

void UWorldPartitionManager::ClearBVHierachy()
{
	if (BVH)
	{
		BVH->Clear();
	}
}
