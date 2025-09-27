#pragma once
#include "Vector.h"

class UPrimitiveComponent;
class AStaticMeshActor;

class FOctree;
class FBVHierachy;

struct FRay;
struct FBound;

class UWorldPartitionManager : public UObject
{
public:
	DECLARE_CLASS(UWorldPartitionManager, UObject)
	UWorldPartitionManager();
	~UWorldPartitionManager();
	/** 싱글톤 접근 */
	static UWorldPartitionManager* GetInstance()
	{
		static UWorldPartitionManager* Instance;
		if (!Instance)
		{
			Instance = NewObject<UWorldPartitionManager>();
		}
		return Instance;
	}

	void Clear();
	// Actor-based API (preferred)
	void Register(AActor* Actor);
	// 벌크 등록 - 대량 액터 처리용
	void BulkRegister(const TArray<AActor*>& Actors);
	void Unregister(AActor* Actor);
	void MarkDirty(AActor* Actor);

    void Update(float DeltaTime, uint32 budgetItems = 256);

    void RayQuery(FRay InRay, OUT TArray<AActor*>& Actors);
    void RayQueryOrdered(FRay InRay, OUT TArray<std::pair<AActor*, float>>& Candidates);
    void Query(FBound InBound, OUT TArray<AActor*>& Actors);

	/** 옥트리 게터 */
	FOctree* GetSceneOctree() const { return SceneOctree; }
	/** BVH 게터 */
	FBVHierachy* GetBVH() const { return BVH; }

private:
	// 싱글톤 
	UWorldPartitionManager(const UWorldPartitionManager&) = delete;
	UWorldPartitionManager& operator=(const UWorldPartitionManager&) = delete;


	//재시작시 필요 
	void ClearSceneOctree();
	void ClearBVHierachy();

	TQueue<AActor*> DirtyQueue;
	TSet<AActor*> DirtySet;
	FOctree* SceneOctree = nullptr;
	FBVHierachy* BVH = nullptr;
};
