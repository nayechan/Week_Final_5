#pragma once

#include <vector>

class FBVHierachy
{
public:
    // 생성자/소멸자
    FBVHierachy(const FBound& InBounds, int InDepth = 0, int InMaxDepth = 12, int InMaxObjects = 8);
    ~FBVHierachy();

    // 초기화
    void Clear();

    // 삽입 / 제거 / 갱신
    void Insert(AActor* InActor, const FBound& ActorBounds);
    // 벌크 삽입 최적화 - 대량 액터 처리용
    void BulkInsert(const TArray<std::pair<AActor*, FBound>>& ActorsAndBounds);
    bool Contains(const FBound& Box) const;
    bool Remove(AActor* InActor, const FBound& ActorBounds);
    void Update(AActor* InActor, const FBound& OldBounds, const FBound& NewBounds);

    // for Partition Manager Query
    void Remove(AActor* InActor);
    void Update(AActor* InActor);

    void Query(FRay InRay, OUT TArray<AActor*>& Actors);
    void Query(FBound InBound, OUT TArray<AActor*>& Actors);

    // Debug draw
    void DebugDraw(URenderer* Renderer) const;

    // Debug/Stats
    int TotalNodeCount() const;
    int TotalActorCount() const;
    int MaxOccupiedDepth() const;
    void DebugDump() const;
    const FBound& GetBounds() const { return Bounds; }
	FBVHierachy* GetLeft() { return Left; }
    FBVHierachy* GetRight() { return Right; }
    const TArray<AActor*>& GetActors() const { return Actors; }


private:
    // 내부 함수 (BVH)
    void Split();
    void Refit();
    static FBound UnionBounds(const FBound& A, const FBound& B);
    int ChooseSplitAxis() const; // 0:X, 1:Y, 2:Z

    // 대량 빌더(가장 긴 축 중앙값 분할)
    static FBVHierachy* Build(const TArray<std::pair<AActor*, FBound>>& Items, int InMaxDepth = 8, int InMaxObjects = 16);
    // 빌더 헬퍼: 아이템과 재귀 빌드
    struct FBuildItem { AActor* Actor; FBound Box; FVector Center; };
    static FBVHierachy* BuildRecursive(TArray<FBuildItem>& Items, int Depth, int InMaxDepth, int InMaxObjects);

private:
    int Depth;
    int MaxDepth;
    int MaxObjects;
    FBound Bounds;

    // 리프는 Actors 보유, 내부 노드는 Left/Right 보유
    TArray<AActor*> Actors;
    FBVHierachy* Left;
    FBVHierachy* Right;

    // 액터의 마지막 바운드 캐시 (루트 호출 기준으로 갱신)
    TMap<AActor*, FBound> ActorLastBounds;

};
