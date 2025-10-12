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
#include "GizmoArrowComponent.h"

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

void UWorldPartitionManager::Register(UStaticMeshComponent* Smc)
{
	if (!Smc) return;
	MarkDirty(Smc);
}


void UWorldPartitionManager::Register(AActor* Actor)
{
	if (!Actor) return;

	TArray<AActor*> EditorActors = GWorld->GetEditorActors();
	auto it = std::find(EditorActors.begin(), EditorActors.end(), Actor);
	if (it != EditorActors.end())
		return; // 에디터 액터는 포함하지 않는다.
	
	const TArray<USceneComponent*> Components = Actor->GetSceneComponents();
	for (USceneComponent* Component : Components)
	{
		if (UStaticMeshComponent* Smc = Cast<UStaticMeshComponent>(Component))
		{
			MarkDirty(Smc);
		}
	}
}

void UWorldPartitionManager::BulkRegister(const TArray<AActor*>& Actors)
{
	if (Actors.empty()) return;

	TArray<std::pair<UStaticMeshComponent*, FAABB>> ComponentsAndBounds;
	ComponentsAndBounds.reserve(Actors.size());

	for (AActor* Actor : Actors)
	{
		TArray<AActor*> EditorActors = GWorld->GetEditorActors();
		auto it = std::find(EditorActors.begin(), EditorActors.end(), Actor);
		if (it != EditorActors.end())
			continue; // 에디터 액터는 포함하지 않는다.
		
		const TArray<USceneComponent*> Components = Actor->GetSceneComponents();
		for (USceneComponent* Component : Components)
		{
			if (UStaticMeshComponent* Smc = Cast<UStaticMeshComponent>(Component))
			{
				ComponentsAndBounds.push_back({ Smc, Smc->GetWorldAABB() });
				ComponentDirtySet.erase(Smc);
			}
		}
	}
	
	if (BVH) BVH->BulkInsert(ComponentsAndBounds);
}

void UWorldPartitionManager::Unregister(AActor* Actor)
{
	if (!Actor) return;

	const TArray<USceneComponent*> Components = Actor->GetSceneComponents();
	for (USceneComponent* Component : Components)
	{
		if (UStaticMeshComponent* Smc = Cast<UStaticMeshComponent>(Component))
		{
			if (BVH) BVH->Remove(Smc);

			ComponentDirtySet.erase(Smc);
		}
	}
}

void UWorldPartitionManager::MarkDirty(AActor* Actor)
{
	if (!Actor) return;
	const TArray<USceneComponent*> Components = Actor->GetSceneComponents();
	for (USceneComponent* Component : Components)
	{
		if (UStaticMeshComponent* Smc = Cast<UStaticMeshComponent>(Component))
		{
			MarkDirty(Smc);
			
		}
	}
}

void UWorldPartitionManager::MarkDirty(UStaticMeshComponent* Smc)
{
	if (!Smc) return;

	if (Cast<UGizmoArrowComponent>(Smc)) return;
	
	// second: 새로운 요소가 성공적으로 삽입되었으면 true, 이미 요소가 존재하여 삽입에 실패했으면 false
	// DirtyQueue 중복 삽입 방지 로직
	if (ComponentDirtySet.insert(Smc).second)
	{
		ComponentDirtyQueue.push(Smc);
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
