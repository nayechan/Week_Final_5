#pragma once
#include "Vector.h"

class UPrimitiveComponent;
class AStaticMeshActor;

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

	//재시작시 필요 
	void ClearSceneOctree();
	void Query(FRay InRay);
	void Query(FBound InBound);

	/** 옥트리 게터 */
	FOctree* GetSceneOctree() const { return SceneOctree; }

private:
	// 싱글톤 
	UWorldPartitionManager(const UWorldPartitionManager&) = delete;
	UWorldPartitionManager& operator=(const UWorldPartitionManager&) = delete;

	TQueue<AActor*> DirtyQueue;
	TSet<AActor*> DirtySet;
	FOctree* SceneOctree = nullptr;
};