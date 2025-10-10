#pragma once

#include <vector>

struct Frustum;
struct FRay; // forward declaration for ray type

class FBVHierachy
{
public:
    // 생성자/소멸자
    FBVHierachy(const FAABB& InBounds, int InDepth = 0, int InMaxDepth = 12, int InMaxObjects = 8);
    ~FBVHierachy();

    // 초기화
    void Clear();

    // 삽입 / 제거 / 갱신
    void Insert(AActor* InActor, const FAABB& ActorBounds);
    void BulkInsert(const TArray<std::pair<AActor*, FAABB>>& ActorsAndBounds);
    bool Remove(AActor* InActor, const FAABB& ActorBounds);
    void Update(AActor* InActor, const FAABB& OldBounds, const FAABB& NewBounds);

    bool Contains(const FAABB& Box) const;

    // Partition Manager Interface
    void Remove(AActor* InActor);
    void Update(AActor* InActor);

    void FlushRebuild();

    void QueryRayClosest(const FRay& Ray, AActor*& OutActor, OUT float& OutBestT) const;
    void QueryFrustum(const Frustum& InFrustum);

    void DebugDraw(URenderer* Renderer) const;

    // Debug/Stats
    int TotalNodeCount() const;
    int TotalActorCount() const;
    int MaxOccupiedDepth() const;
    void DebugDump() const;
    const FAABB& GetBounds() const { return Bounds; }

    // 프러스텀 기준으로 오클루더(내부노드 AABB) / 오클루디(리프의 액터들) 수집
// VP는 행벡터 기준(네 컨벤션): p' = p * VP
private:
    static FAABB UnionBounds(const FAABB& A, const FAABB& B);

    // === LBVH data ===
    struct FLBVHNode
    {
        FAABB Bounds;
        int32 Left = -1;
        int32 Right = -1;
        int32 First = -1;
        int32 Count = 0;
        bool IsLeaf() const { return Count > 0; }
    };
    void BuildLBVHFromMap();

private:
    int BuildRange(int s, int e);

    int Depth;
    int MaxDepth;
    int MaxObjects;
    FAABB Bounds;

    // 리프 페이로드(호환용): 유지하지만 트리 구조는 사용 안 함
    TArray<AActor*> Actors;

    // 액터의 마지막 바운드 캐시
    TMap<AActor*, FAABB> ActorLastBounds;
    TArray<AActor*> ActorArray;

    // LBVH nodes
    TArray<FLBVHNode> Nodes;

    bool bPendingRebuild = false;
};
