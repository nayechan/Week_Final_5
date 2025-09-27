#include "pch.h"
#include "Occlusion.h"
#include "AABoundingBoxComponent.h"
#include "Frustum.h"

void FOcclusionRing::Initialize(ID3D11Device* Dev, uint32 MaxQ)
{
    MaxPerFrame = MaxQ;
    CurrentFrame = 0;
    for (int i = 0; i < kDelayFrames; ++i) { FrameBatches[i].Empty(); }
    IssuedThisFrame.Empty();
    FreePool.Empty();
}

void FOcclusionRing::ReleaseAll()
{
    auto ReleaseList = [](TArray<FOcclusionQueryEntry*>& L)
        {
            for (auto* E : L)
            {
                if (E)
                {
                    if (E->Predicate) { E->Predicate->Release(); E->Predicate = nullptr; }
                    delete E;
                }
            }
            L.Empty();
        };

    for (int i = 0; i < kDelayFrames; ++i) ReleaseList(FrameBatches[i]);
    ReleaseList(IssuedThisFrame);
    ReleaseList(FreePool);
}

void FOcclusionRing::BeginFrame()
{
    // 이번 프레임에 기록할 슬롯을 재활용하기 위해, 기존 엔트리들을 풀로 되돌림
    auto& Slot = FrameBatches[CurrentFrame];
    for (auto* Q : Slot) { if (Q) { Q->Issued = false; FreePool.Add(Q); } }
    Slot.Empty();

    IssuedThisFrame.Empty();
}

void FOcclusionRing::EndFrame()
{
    // 이번 프레임 발급분을 현재 슬롯에 저장
    FrameBatches[CurrentFrame] = IssuedThisFrame;

    // 다음 프레임로 인덱스 회전
    CurrentFrame = (CurrentFrame + 1) % kDelayFrames;
}

FOcclusionQueryEntry* FOcclusionRing::AllocQuery(ID3D11Device* Dev)
{
    if (!FreePool.IsEmpty())
    {
        auto* Q = FreePool.Last();
        FreePool.RemoveAt(FreePool.Num() - 1);
        return Q;
    }

    D3D11_QUERY_DESC qd = {};
    qd.Query = D3D11_QUERY_OCCLUSION_PREDICATE;

    ID3D11Predicate* Pred = nullptr;
    if (FAILED(Dev->CreatePredicate(&qd, &Pred))) return nullptr;

    auto* E = new FOcclusionQueryEntry();
    E->Predicate = Pred;
    return E;
}

void FOcclusionRing::EnqueueIssued(FOcclusionQueryEntry* Q)
{
    if (!Q) return;
    Q->Issued = true;
    IssuedThisFrame.Add(Q);
}

ID3D11Predicate* FOcclusionRing::GetPredicateForId(FOcclusionId Id) const
{
    // 현재 프레임은 아직 GPU가 끝나지 않았을 수 있으니,
    // 직전 프레임 슬롯을 사용 (kDelayFrames=2 기준)
    const int Delayed = (CurrentFrame + kDelayFrames - 1) % kDelayFrames;

    const auto& Batch = FrameBatches[Delayed];
    for (auto* Q : Batch)
    {
        if (Q && Q->Id == Id) return Q->Predicate;
    }
    return nullptr;
}