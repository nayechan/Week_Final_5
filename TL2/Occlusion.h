#pragma once


// ─────────────────────────────────────────────────────────────
// 공용 ID (노드/액터 식별)
// ─────────────────────────────────────────────────────────────
using FOcclusionId = uint32;

// ─────────────────────────────────────────────────────────────
// 쿼리 엔트리 (Predicate 1:1)
// ─────────────────────────────────────────────────────────────
struct FOcclusionQueryEntry
{
    ID3D11Predicate* Predicate = nullptr; // D3D11_QUERY_OCCLUSION_PREDICATE
    FOcclusionId     Id = 0;              // 노드/액터 식별자
    bool             Issued = false;
};

// ─────────────────────────────────────────────────────────────
// 2-프레임 지연 링 버퍼 (deque 미사용, TArray 기반)
// ─────────────────────────────────────────────────────────────
class FOcclusionRing
{
public:
    FOcclusionRing() = default;
    ~FOcclusionRing() { ReleaseAll(); }

    void Initialize(ID3D11Device* Dev, uint32 MaxQueriesPerFrame = 10000);
    void ReleaseAll();

    // 프레임 경계
    void BeginFrame();   // 쓰기 슬롯 재활용 준비
    void EndFrame();     // 이번 프레임 발급 저장 + 인덱스 회전

    // 쿼리 객체 풀
    FOcclusionQueryEntry* AllocQuery(ID3D11Device* Dev);
    void EnqueueIssued(FOcclusionQueryEntry* Q);

    // kDelayFrames 지연된 프레디케이트 취득(없으면 nullptr)
    ID3D11Predicate* GetPredicateForId(FOcclusionId Id) const;

private:
    static constexpr int kDelayFrames = 2; // 2프레임 지연
    uint32 MaxPerFrame = 0;

    // 프레임 링: 고정 2 슬롯
    TArray< FOcclusionQueryEntry* > FrameBatches[kDelayFrames];
    int CurrentFrame = 0;                 // 이번 프레임에 쓸 슬롯

    // 이번 프레임 발급 리스트 (FrameBatches[CurrentFrame]에 기록됨)
    TArray< FOcclusionQueryEntry* > IssuedThisFrame;

    // 재사용 풀
    TArray< FOcclusionQueryEntry* > FreePool;
};