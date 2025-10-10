#include"pch.h"
#include "AABoundingBoxComponent.h"
#include "SelectionManager.h"
#include "Line.h"   
#include "Actor.h"

UAABoundingBoxComponent::UAABoundingBoxComponent()
    : LocalMin(FVector{}), LocalMax(FVector{})
{
}

using std::max;
using std::sqrt;

void UAABoundingBoxComponent::SetFromVertices(const TArray<FVector>& Verts)
{
    if (Verts.empty()) return;

    LocalMin = LocalMax = Verts[0];
    for (const auto& v : Verts)
    {
        LocalMin = LocalMin.ComponentMin(v);
        LocalMax = LocalMax.ComponentMax(v);
    }
}

void UAABoundingBoxComponent::SetFromVertices(const TArray<FNormalVertex>& Verts)
{
    if (Verts.empty()) return;

    LocalMin = LocalMax = Verts[0].pos;
    for (const auto& v : Verts)
    {
        LocalMin = LocalMin.ComponentMin(v.pos);
        LocalMax = LocalMax.ComponentMax(v.pos);
    }
}

void UAABoundingBoxComponent::Render(URenderer* Renderer, const FMatrix& ViewMatrix, const FMatrix& ProjectionMatrix)
{
    UWorld* World = GetOwner() ? GetOwner()->GetWorld() : nullptr;
    const bool bShowBounds = World && World->GetRenderSettings().IsShowFlagEnabled(EEngineShowFlags::SF_BoundingBoxes);
    const bool bSelected = (World && World->GetSelectionManager()->GetSelectedActor() == GetOwner());

    if (bShowBounds && bSelected)
    {
        TArray<FVector> Start;
        TArray<FVector> End;
        TArray<FVector4> Color;

        // Arvo 방식으로 월드 AABB
        FAABB WorldBound = GetWorldBound();
        CreateLineData(WorldBound.Min, WorldBound.Max, Start, End, Color);
        Renderer->AddLines(Start, End, Color);
    }
}


FAABB UAABoundingBoxComponent::GetWorldBound() const
{
    // 1) 로컬 중심/익스텐트
    const FVector LocalCenter = (LocalMin + LocalMax) * 0.5f;
    const FVector LocalExtents = (LocalMax - LocalMin) * 0.5f;

    // 2) 월드 행렬
    if (GetOwner() == nullptr)
    {
        UE_LOG("owner is nullptr");
    }
    const FMatrix WorldMat = GetOwner()->GetWorldMatrix();

    // 3) SIMD 최적화된 월드 중심 계산 (행벡터: p' = p * M)
    const FVector WorldCenter = TransformVectorSIMD(LocalCenter, WorldMat);

    // 4) 월드 익스텐트 (Arvo)
    const FVector WorldExtents = ComputeWorldExtentsArvo(LocalExtents, WorldMat);

    // 5) AABB 재조립
    return FAABB(WorldCenter - WorldExtents, WorldCenter + WorldExtents);
}

FVector UAABoundingBoxComponent::TransformVectorSIMD(const FVector& v, const FMatrix& M) const
{
#if defined(_MSC_VER) && (defined(_M_X64) || defined(_M_IX86))
    // SSE 구현 (행벡터: p' = p * M)
    const __m128 vx = _mm_set1_ps(v.X);
    const __m128 vy = _mm_set1_ps(v.Y);
    const __m128 vz = _mm_set1_ps(v.Z);

    const __m128 m0 = _mm_loadu_ps(&M.M[0][0]); // [m00 m01 m02 m03]
    const __m128 m1 = _mm_loadu_ps(&M.M[1][0]); // [m10 m11 m12 m13]
    const __m128 m2 = _mm_loadu_ps(&M.M[2][0]); // [m20 m21 m22 m23]
    const __m128 m3 = _mm_loadu_ps(&M.M[3][0]); // [m30 m31 m32 m33]

    __m128 r = _mm_mul_ps(vx, m0);
    r = _mm_add_ps(r, _mm_mul_ps(vy, m1));
    r = _mm_add_ps(r, _mm_mul_ps(vz, m2));
    r = _mm_add_ps(r, m3);

    alignas(16) float out[4];
    _mm_store_ps(out, r);
    return FVector(out[0], out[1], out[2]);
#else
    // Fallback: 스칼라 버전
    return FVector(
        v.X * M.M[0][0] + v.Y * M.M[1][0] + v.Z * M.M[2][0] + M.M[3][0],
        v.X * M.M[0][1] + v.Y * M.M[1][1] + v.Z * M.M[2][1] + M.M[3][1],
        v.X * M.M[0][2] + v.Y * M.M[1][2] + v.Z * M.M[2][2] + M.M[3][2]
    );
#endif
}

FAABB UAABoundingBoxComponent::GetWorldBoundFromCube() 
{
    auto corners = GetLocalCorners();

    FMatrix WorldMat = GetOwner()->GetWorldMatrix();
    FVector4 MinW = corners[0] * WorldMat;
    FVector4 MaxW = MinW;

    for (auto& c : corners)
    {
        FVector4 wc = c * WorldMat;
        MinW = MinW.ComponentMin(wc);
        MaxW = MaxW.ComponentMax(wc);
    }

    Bound = FAABB({ MinW.X, MinW.Y, MinW.Z }, { MaxW.X, MaxW.Y, MaxW.Z });
    return Bound;
}



//FBound UAABoundingBoxComponent::() const
//{
//    //중심 이동
//    const FVector WorldCenter = GetOwner()->GetActorLocation();
//    const FMatrix WorldMat = GetOwner()->GetWorldMatrix();
//
//    //월드 축 기준 스케일 값 계산
//    FVector WorldScaleExtents;
//    WorldScaleExtents.X = sqrtf(
//        WorldMat.M[0][0] * WorldMat.M[0][0] +
//        WorldMat.M[1][0] * WorldMat.M[1][0] + 
//        WorldMat.M[2][0] * WorldMat.M[2][0]
//    );
//    WorldScaleExtents.Y = sqrtf(
//        WorldMat.M[0][1] * WorldMat.M[0][1] +
//        WorldMat.M[1][1] * WorldMat.M[1][1] +
//        WorldMat.M[2][1] * WorldMat.M[2][1]
//    );
//    WorldScaleExtents.Z = sqrtf(
//        WorldMat.M[0][2] * WorldMat.M[0][2] +
//        WorldMat.M[1][2] * WorldMat.M[1][2] +
//        WorldMat.M[2][2] * WorldMat.M[2][2]
//    );
//
//    //최종 AABB
//    FVector Min = WorldCenter - WorldScaleExtents;
//    FVector Max = WorldCenter + WorldScaleExtents;
//    return FBound(Min, Max);
//}

TArray<FVector4> UAABoundingBoxComponent::GetLocalCorners() const
{
    return 
    {
        {LocalMin.X, LocalMin.Y, LocalMin.Z, 1},
        {LocalMax.X, LocalMin.Y, LocalMin.Z, 1},
        {LocalMin.X, LocalMax.Y, LocalMin.Z, 1},
        {LocalMax.X, LocalMax.Y, LocalMin.Z, 1},
        {LocalMin.X, LocalMin.Y, LocalMax.Z, 1},
        {LocalMax.X, LocalMin.Y, LocalMax.Z, 1},
        {LocalMin.X, LocalMax.Y, LocalMax.Z, 1},

        {LocalMax.X, LocalMax.Y, LocalMax.Z, 1}
    };
}

void UAABoundingBoxComponent::DuplicateSubObjects()
{
    Super::DuplicateSubObjects();
    return;
}

void UAABoundingBoxComponent::CreateLineData(
    const FVector& Min, const FVector& Max,
    OUT TArray<FVector>& Start,
    OUT TArray<FVector>& End,
    OUT TArray<FVector4>& Color)
{
    // 8개 꼭짓점 정의
    const FVector v0(Min.X, Min.Y, Min.Z);
    const FVector v1(Max.X, Min.Y, Min.Z);
    const FVector v2(Max.X, Max.Y, Min.Z);
    const FVector v3(Min.X, Max.Y, Min.Z);
    const FVector v4(Min.X, Min.Y, Max.Z);
    const FVector v5(Max.X, Min.Y, Max.Z);
    const FVector v6(Max.X, Max.Y, Max.Z);
    const FVector v7(Min.X, Max.Y, Max.Z);

    // 선 색상 정의
    const FVector4 LineColor(1.0f, 1.0f, 0.0f, 1.0f); // 노란색

    // --- 아래쪽 면 ---
    Start.Add(v0); End.Add(v1); Color.Add(LineColor);
    Start.Add(v1); End.Add(v2); Color.Add(LineColor);
    Start.Add(v2); End.Add(v3); Color.Add(LineColor);
    Start.Add(v3); End.Add(v0); Color.Add(LineColor);

    // --- 위쪽 면 ---
    Start.Add(v4); End.Add(v5); Color.Add(LineColor);
    Start.Add(v5); End.Add(v6); Color.Add(LineColor);
    Start.Add(v6); End.Add(v7); Color.Add(LineColor);
    Start.Add(v7); End.Add(v4); Color.Add(LineColor);

    // --- 옆면 기둥 ---
    Start.Add(v0); End.Add(v4); Color.Add(LineColor);
    Start.Add(v1); End.Add(v5); Color.Add(LineColor);
    Start.Add(v2); End.Add(v6); Color.Add(LineColor);
    Start.Add(v3); End.Add(v7); Color.Add(LineColor);
}

FVector UAABoundingBoxComponent::ComputeWorldExtentsArvo(const FVector& E, const FMatrix& M) const
{
    // 행벡터 기준: World.M[r][c], 선형부 L = 상위 3x3
    // WorldExtents = E * |L|  (오른쪽 곱)
    const float Ex = E.X, Ey = E.Y, Ez = E.Z;

    // 각 월드축(X,Y,Z) 방향 익스텐트
    const float Wx = std::abs(M.M[0][0]) * Ex + std::abs(M.M[1][0]) * Ey + std::abs(M.M[2][0]) * Ez;
    const float Wy = std::abs(M.M[0][1]) * Ex + std::abs(M.M[1][1]) * Ey + std::abs(M.M[2][1]) * Ez;
    const float Wz = std::abs(M.M[0][2]) * Ex + std::abs(M.M[1][2]) * Ey + std::abs(M.M[2][2]) * Ez;

    return FVector(Wx, Wy, Wz);
}