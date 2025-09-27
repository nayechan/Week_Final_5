#pragma once
#include <unordered_map>
#include "AABoundingBoxComponent.h"
#include "Renderer.h"
#include "Picking.h"
class FOctree
{
public:
    // 생성자/소멸자
    FOctree(const FBound& InBounds, int InDepth = 0, int InMaxDepth = 5, int InMaxObjects = 15);
    ~FOctree();

	// 초기화
	void Clear();

	// 삽입 / 제거 / 갱신
	void Insert(AActor* InActor, const FBound& ActorBounds);
	// 벌크 삽입 최적화 - 대량 액터 처리용
	void BulkInsert(const TArray<std::pair<AActor*, FBound>>& ActorsAndBounds);
	bool Contains(const FBound& Box) const;
	bool Remove(AActor* InActor, const FBound& ActorBounds);
    void Update(AActor* InActor, const FBound& OldBounds, const FBound& NewBounds);

    //void QueryRay(const Ray& InRay, std::vector<Actor*>& OutActors) const;
    //void QueryFrustum(const Frustum& InFrustum, std::vector<Actor*>& OutActors) const;
	//쿼리
	void QueryRay(const FRay& Ray, TArray<AActor*>& OutActors) const;
    // Ordered query: returns pairs (Actor, AABB tmin) for early-out pruning
    void QueryRayOrdered(const FRay& Ray, TArray<std::pair<AActor*, float>>& OutCandidates) const;
    // for Partition Manager Query
    void Remove(AActor* InActor);
    void Update(AActor* InActor);

    // Debug draw
    void DebugDraw(URenderer* InRenderer) const;

    // Debug/Stats
    int TotalNodeCount() const;
    int TotalActorCount() const;
    int MaxOccupiedDepth() const;
    void DebugDump() const;

    const FBound& GetBounds() const { return Bounds; }

private:
	// 내부 함수
	void Split();
	// 최적화된 옥탄트 계산 - Contains() 대신 사용
	int GetOctantIndex(const FBound& ActorBounds) const;
	bool CanFitInOctant(const FBound& ActorBounds, int OctantIndex) const;

private:
	int Depth;
	int MaxDepth;
	int MaxObjects;
	FBound Bounds;

	TArray<AActor*> Actors;
	FOctree* Children[8]; // 8분할 

    TMap<AActor*, FBound> ActorLastBounds;
    
    // 메모리 풀링을 위한 정적 스택
    static TArray<FOctree*> NodePool;
    static FOctree* AllocateNode(const FBound& InBounds, int InDepth, int InMaxDepth, int InMaxObjects);
    static void DeallocateNode(FOctree* Node);
};

