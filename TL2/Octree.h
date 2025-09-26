#pragma once
#include "AABoundingBoxComponent.h"
#include "Renderer.h"

class FOctree
{
public:
    // 생성자/소멸자
    FOctree(const FBound& InBounds, int InDepth = 0, int InMaxDepth = 5, int InMaxObjects = 5);
    ~FOctree();

	// 초기화
	void Clear();

	// 삽입 / 제거 / 갱신
	void Insert(AActor* InActor, const FBound& ActorBounds);
	bool Contains(const FBound& Box) const;
	bool Remove(AActor* InActor, const FBound& ActorBounds);
    void Update(AActor* InActor, const FBound& OldBounds, const FBound& NewBounds);

    //void QueryRay(const Ray& InRay, std::vector<Actor*>& OutActors) const;
    //void QueryFrustum(const Frustum& InFrustum, std::vector<Actor*>& OutActors) const;

    // Debug draw
    void DebugDraw(URenderer* Renderer) const;

    // Debug/Stats
    int TotalNodeCount() const;
    int TotalActorCount() const;
    int MaxOccupiedDepth() const;
    void DebugDump() const;
    const FBound& GetBounds() const { return Bounds; }

private:
	// 내부 함수
	void Split();

private:
	int Depth;
	int MaxDepth;
	int MaxObjects;
	FBound Bounds;

	TArray<AActor*> Actors;
	FOctree* Children[8]; // 8분할 
};

