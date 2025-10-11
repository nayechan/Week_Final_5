#include "pch.h"
#include "WorldPartitionManager.h"
#include "PrimitiveComponent.h"
#include "Actor.h"
#include "AABoundingBoxComponent.h"
#include "World.h"
#include "Octree.h"
#include "BVHierarchy.h"
#include "StaticMeshActor.h"
#include "StaticMeshComponent.h"
#include "Frustum.h"


namespace
{
	inline UStaticMeshComponent* ResolveStaticMeshComponent(AActor* Actor)
	{
		if (!Actor)
		{
			return nullptr;
		}

		if (AStaticMeshActor* StaticMeshActor = Cast<AStaticMeshActor>(Actor))
		{
			return StaticMeshActor->GetStaticMeshComponent();
		}
		return nullptr;
	}
}

UWorldPartitionManager::UWorldPartitionManager()
{
	//FBound WorldBounds(FVector(-50, -50, -50), FVector(50, 50, 50));
	FAABB WorldBounds(FVector(-50, -50, -50), FVector(50, 50, 50));
	SceneOctree = new FOctree(WorldBounds, 0, 8, 10);
	// BVH도 동일 월드 바운드로 초기화 (더 깊고 작은 리프 설정)
	//BVH = new FBVHierachy(FBound(), 0, 5, 1); 
	BVH = new FBVHierarchy(FAABB(), 0, 8, 1); 
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
	ClearBVHierarchy();

	ComponentDirtyQueue.Empty();
	ComponentDirtySet.Empty();
}

void UWorldPartitionManager::Register(AActor* Actor)
{
	if (!Actor) return;
	if (UStaticMeshComponent* Component = ResolveStaticMeshComponent(Actor))
	{
		MarkDirty(Component);
	}
}

void UWorldPartitionManager::BulkRegister(const TArray<AActor*>& Actors)
{
	if (Actors.empty()) return;

	TArray<std::pair<UStaticMeshComponent*, FAABB>> ComponentsAndBounds;
	ComponentsAndBounds.reserve(Actors.size());

	for (AActor* Actor : Actors)
	{
		if (UStaticMeshComponent* Component = ResolveStaticMeshComponent(Actor))
		{
			ComponentsAndBounds.push_back({ Component, Component->GetWorldAABB() });
			ComponentDirtySet.erase(Component);
		}
	}

	// Octree: 기존 대량 삽입
	//if (SceneOctree) SceneOctree->BulkInsert(ActorsAndBounds);
	if (BVH) BVH->BulkInsert(ComponentsAndBounds);
}

void UWorldPartitionManager::Unregister(AActor* Actor)
{
	if (!Actor) return;
	if (UStaticMeshComponent* Component = ResolveStaticMeshComponent(Actor))
	{
		//if (SceneOctree) SceneOctree->Remove(Owner);
		if (BVH) BVH->Remove(Component);

		ComponentDirtySet.erase(Component);
	}
}

void UWorldPartitionManager::MarkDirty(AActor* Actor)
{
	if (!Actor) return;
	if (UStaticMeshComponent* Component = ResolveStaticMeshComponent(Actor))
	{
		MarkDirty(Component);
	}
}

void UWorldPartitionManager::MarkDirty(UStaticMeshComponent* Component)
{
	if (!Component)
	{
		return;
	}

	if (ComponentDirtySet.insert(Component).second)
	{
		ComponentDirtyQueue.push(Component);
	}
}

void UWorldPartitionManager::Update(float DeltaTime, const uint32 BudgetCount)
{
	// 프레임 히칭 방지를 위해 컴포넌트 카운트 제한
	uint32 processed = 0;
	while (processed < BudgetCount)
	{
		UStaticMeshComponent* Component = nullptr;
		if (!ComponentDirtyQueue.Dequeue(Component))
		{
			break;
		}

		if (ComponentDirtySet.erase(Component) == 0)
		{
			// 이미 처리되었거나 제거됨
			continue;
		}

		if (!Component) continue;
		if (BVH) BVH->Update(Component);

		++processed;
	}

	if (BVH)
	{
		BVH->FlushRebuild();
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

void UWorldPartitionManager::ClearBVHierarchy()
{
	if (BVH)
	{
		BVH->Clear();
	}
}
