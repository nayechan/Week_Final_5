#include "pch.h"

#include "Vector.h"
#include "Frustum.h"
#include "AABoundingBoxComponent.h"
#include "CameraComponent.h"
#include <immintrin.h> // For SSE, AVX, FMA instructions


inline FVector4 MakePoint4(const FVector& P) { return FVector4(P.X, P.Y, P.Z, 1.0f); }
inline FVector4 MakeDir4(const FVector& D) { return FVector4(D.X, D.Y, D.Z, 0.0f); }

inline float     Dot3(const FVector4& A, const FVector4& B)
{
    // Use SSE4.1's dot product instruction. It's faster than manual shuffling.
    // 0x71 = (0111 0001b)
    //   - Low 4 bits (0111): Multiply and sum the first three components (X, Y, Z).
    //   - High 4 bits (0001): Store the result in the first component of the destination.
    __m128 dp = _mm_dp_ps(A.SimdData, B.SimdData, 0x71);
    float result;
    _mm_store_ss(&result, dp);
    return result;
}

inline FVector4  Cross3(const FVector4& A, const FVector4& B)
{
    // Use shuffles and FMA (Fused Multiply-Add) for an efficient cross product.
    // Formula:
    // C.x = A.y * B.z - A.z * B.y
    // C.y = A.z * B.x - A.x * B.z
    // C.z = A.x * B.y - A.y * B.x

    // Shuffle A and B to align components for multiplication
    __m128 a_yzx = _mm_shuffle_ps(A.SimdData, A.SimdData, _MM_SHUFFLE(3, 0, 2, 1)); // (Ay, Az, Ax, Aw)
    __m128 b_zxy = _mm_shuffle_ps(B.SimdData, B.SimdData, _MM_SHUFFLE(3, 1, 0, 2)); // (Bz, Bx, By, Bw)
    __m128 a_zxy = _mm_shuffle_ps(A.SimdData, A.SimdData, _MM_SHUFFLE(3, 1, 0, 2)); // (Az, Ax, Ay, Aw)
    __m128 b_yzx = _mm_shuffle_ps(B.SimdData, B.SimdData, _MM_SHUFFLE(3, 0, 2, 1)); // (By, Bz, Bx, Bw)

    // term1 = (Ay*Bz, Az*Bx, Ax*By, ...)
    __m128 term1 = _mm_mul_ps(a_yzx, b_zxy);

    // result = term1 - (a_zxy * b_yzx) using FMA
    // _mm_fnmadd_ps(a, b, c) computes c - (a * b)
    __m128 sub = _mm_fnmadd_ps(a_zxy, b_yzx, term1);

    FVector4 result(sub);
    result.W = 0.0f;
    return result;
}
inline float     Length3(const FVector4& V)
{
    float dot = Dot3(V, V);
    return std::sqrt(dot);
}
inline FVector4  Normalize3(const FVector4& V)
{
    float len = Length3(V);
    if (len > 0.0f)
    {
        __m128 v = _mm_load_ps(&V.X);
        __m128 len_v = _mm_set1_ps(len);
        __m128 result_v = _mm_div_ps(v, len_v);
        FVector4 result;
        _mm_store_ps(&result.X, result_v);
        result.W = 0.0f;
        return result;
    }
    return FVector4(0, 0, 0, 0);
}
// ------------------------------------------------------------
// 절두체(Frustum)/평면 유틸
//  - 평면 식:  dot(N, X) - D = 0
//  - 본 코드는 "절두체 안쪽이 >= 0" 이 되도록 법선을 맞춘다(가시성 테스트에 유리).
//  - 좌표계가 LH여도 외적(Cross)은 RH 정의를 사용한다.
//    => 법선의 "방향(안/밖)"은 외적의 피연산자 "순서"로 결정한다.
// ------------------------------------------------------------

namespace
{
    // 점(Point)과 법선(Normal)로 평면 생성
    //  - 입력 Normal은 정규화되지 않았을 수 있으므로, 반드시 정규화
    //  - Distance = dot(Normal, Point)  (평면 상의 임의의 점과 법선의 내적)
    Plane MakePlane(const FVector4& Point, const FVector4& Normal)
    {
        const FVector4 UnitNormal = Normalize3(Normal); // 길이 1로 보정
        return Plane
        {
            UnitNormal,
            Dot3(UnitNormal, Point)
        };
    }
    // (선택) 모든 평면을 "안쪽을 향하도록" 보정
    //  - 수치 불안정, 축/부호 실수, 미러링(det<0) 등에도 항상 안쪽 ≥ 0을 보장
    /*
    inline void EnsureFacingInwards(Plane& P, const FVector& InsidePoint)
    {
        if (FVector::Dot(P.Normal, InsidePoint) - P.Distance < 0.0f)
        {
            P.Normal *= -1.0f;
            P.Distance *= -1.0f;
        }
    }
    */
}

Frustum CreateFrustumFromCamera(const UCameraComponent& Camera, float OverrideAspect)
{
    // 카메라 파라미터
    const float NearClip = Camera.GetNearClip();
    const float FarClip = Camera.GetFarClip();
    const float Aspect = OverrideAspect > 0.f ? OverrideAspect : Camera.GetAspectRatio();
    const float FovRad = DegreeToRadian(Camera.GetFOV());

    // 카메라 기준 좌표축(월드 공간)
    // Forward=+X, Right=+Y, Up=+Z
    const FVector4 Origin = MakePoint4(Camera.GetWorldLocation());
    const FVector4 Forward = MakeDir4(Camera.GetForward());
    const FVector4 Right = MakeDir4(Camera.GetRight());
    const FVector4 Up = MakeDir4(Camera.GetUp());

    // Far 평면에서의 절반 높이/너비
    const float HalfVSide = FarClip * tanf(FovRad * 0.5f); // 세로(Vertical) 반폭
    const float HalfHSide = HalfVSide * Aspect;            // 가로(Horizontal) 반폭
    const FVector4 FrontMultFar = FVector4(Forward.X * FarClip, Forward.Y * FarClip, Forward.Z * FarClip, 0.0f);        // Far까지의 전방 벡터

    Frustum Result;

    // ------------------------------------------------------------
    // Near / Far
    //  - 평면 법선이 "프러스텀 내부"를 향하도록 잡는다.
    //    Near: +Forward,  Far: -Forward
    // ------------------------------------------------------------
    Result.NearFace = MakePlane(Origin + Forward * NearClip, Forward);
    Result.FarFace = MakePlane(Origin + FrontMultFar, Forward * -1.0f);


    // ------------------------------------------------------------
    // 측면 평면(Left/Right/Top/Bottom)
    //  - 외적은 RH 정의를 사용한다.
    //  - 피연산자 순서를 적절히 선택해 "내부"를 바라보는 법선을 만든다.
    // 
    //	- 현재 좌표축이 LH(Left-Handed) 기준이므로, 
    //  - Cross 연산 순서 또한 왼손으로 A 먼저, 그 후 B를 감았을 때 나오는 방향이 법선이 된다.
    // 
    //  - Right  : Cross( F*Far + R*HalfH,  U )  → (+X, -Y, 0)
    //  - Left   : Cross( U, F*Far - R*HalfH ) → (+X, +Y, 0)
    //  - Top    : Cross( R, F*Far + U*HalfV ) → (+X, 0, -Z)
    //  - Bottom : Cross( F*Far - U*HalfV,   R )  → (+X, 0, +Z)
    // ------------------------------------------------------------
    Result.RightFace = MakePlane(Origin, Cross3(FrontMultFar + Right * HalfHSide, Up));
    Result.LeftFace = MakePlane(Origin, Cross3(Up, FrontMultFar - Right * HalfHSide));
    Result.TopFace = MakePlane(Origin, Cross3(Right, FrontMultFar + Up * HalfVSide));
    Result.BottomFace = MakePlane(Origin, Cross3(FrontMultFar - Up * HalfVSide, Right));
    return Result;
}

// ------------------------------------------------------------
// AABB vs 프러스텀 판정
//  - 각 평면에 대해: 중심의 부호 + 박스의 "프로젝션 반경"으로 배제 테스트
//  - 규약: 안쪽 ≥ 0  ⇒  Distance + Radius >= 0 이면 그 평면을 통과(겹침)
//  - 하나라도 실패하면(음수) 절두체 밖 → 즉시 탈락
// ------------------------------------------------------------
bool Intersects(const Plane& P, const FVector4& Center, const FVector4& Extents) 
{
	// 평면과 박스사이의 거리 (양수면 평면의 법선 방향, 음수면 반대 방향)
    const float Distance = Dot3(P.Normal, Center) - P.Distance;
    // AABB를 평면 법선 방향으로 투영했을 때의 최대 반경
    // Radius = abs(Normal.X) * Extents.X + abs(Normal.Y) * Extents.Y + abs(Normal.Z) * Extents.Z
    __m128 abs_normal = _mm_andnot_ps(_mm_set1_ps(-0.0f), P.Normal.SimdData); // abs for all components
    __m128 dp = _mm_dp_ps(abs_normal, Extents.SimdData, 0x71);
    float radius;
    _mm_store_ss(&radius, dp);
	//  최대 반경은 항상 양수이므로, Distance + Radius < 0 이면 절두체의 바깥
    return Distance + radius >= 0.0f;
}

bool IsAABBVisible(const Frustum& Frustum, const FBound& Bound)
{
    // AABB 중심/반길이
    const FVector Center3 = (Bound.Min + Bound.Max) * 0.5f;
    const FVector Extents3 = (Bound.Max - Bound.Min) * 0.5f; // 항상 양수
    const FVector4 Center = MakePoint4(Center3);
    const FVector4 Extents = MakeDir4(Extents3); // 항상 양수
    // 6면 모두 통과해야 절두체의 안쪽이므로 "보이는 것"
    return Intersects(Frustum.LeftFace, Center, Extents) && 
           Intersects(Frustum.RightFace, Center, Extents)  &&
           Intersects(Frustum.TopFace, Center, Extents) &&
           Intersects(Frustum.BottomFace, Center, Extents) &&
           Intersects(Frustum.NearFace, Center, Extents) &&
           Intersects(Frustum.FarFace, Center, Extents);
}



// 추후에 절두체를 VP 행렬에서 바로 추출하는 방법도 필요하다면 아래를 참고.
// ---------- VP(=View*Proj)에서 평면 추출 ----------
// row-vector 규약(p' = p * M)에서 클립 경계는
// Left:   R3 + R0
// Right:  R3 - R0
// Bottom: R3 + R1
// Top:    R3 - R1
// Near:   R3 + R2
// Far:    R3 - R2
// 결합 결과 P=(a,b,c,d)에 대해:  a*x + b*y + c*z + d >= 0  (클립 내부)
// 우리의 평면식 dot(N,X) - D >= 0 과 맞추려면  N=(a,b,c), D=-d  을 사용.
// 정규화 스케일도 Distance에 나눠 반영해야 함.
/*
static inline Plane MakePlaneFromRowCombo(const FVector4& R3, const FVector4& Ri, int Sign) // +1 or -1
{
    // P = R3 + Sign * Ri
    const FVector4 P(R3.X + Sign * Ri.X,
        R3.Y + Sign * Ri.Y,
        R3.Z + Sign * Ri.Z,
        R3.W + Sign * Ri.W);

    // 법선/거리 추출 및 정규화
    Plane Out;
    const FVector4 N(P.X, P.Y, P.Z, 0.0f);
    const float    Len = Length3(N);

    if (Len > 0.0f)
    {
        Out.Normal = FVector4(N.X / Len, N.Y / Len, N.Z / Len, 0.0f);
        // dot(N,X) - D >= 0  와  a*x+b*y+c*z+d >= 0  을 맞추기 위해 D = -d/Len
        Out.Distance = -P.W / Len;
    }
    else
    {
        Out.Normal = FVector4(0, 1, 0, 0);
        Out.Distance = 0.0f;
    }
    return Out;
}

// 카메라에서 VP를 구할 수 있다면 직접 사용.
// 주의: row-vector 규약이면 반드시  VP = View * Proj  (이 순서)
static Frustum CreateFrustumFromVP(const FMatrix& View, const FMatrix& Proj)
{
    const FMatrix VP = View * Proj;

    // 행 접근 함수가 있다고 가정 (없다면 직접 구현)
    const FVector4 R0 = VP.GetRow(0);
    const FVector4 R1 = VP.GetRow(1);
    const FVector4 R2 = VP.GetRow(2);
    const FVector4 R3 = VP.GetRow(3);

    Frustum F;
    F.LeftFace = MakePlaneFromRowCombo(R3, R0, +1);
    F.RightFace = MakePlaneFromRowCombo(R3, R0, -1);
    F.BottomFace = MakePlaneFromRowCombo(R3, R1, +1);
    F.TopFace = MakePlaneFromRowCombo(R3, R1, -1);
    F.NearFace = MakePlaneFromRowCombo(R3, R2, +1);
    F.FarFace = MakePlaneFromRowCombo(R3, R2, -1);
    return F;
}

Frustum CreateFrustumFromCamera(const UCameraComponent& Camera, float)  // OverrideAspect
{
    // 카메라에서 뷰/프로젝션 행렬을 가져오는 API는 엔진에 맞춰 바꿔줘.
    // row-vector 규약: p' = p * View * Proj
    const FMatrix View = Camera.GetViewMatrix();         // 너의 엔진 API
    const FMatrix Proj = Camera.GetProjectionMatrix();   // 너의 엔진 API
    return CreateFrustumFromVP(View, Proj);
}

// ---------- AABB vs 평면 판정(기존 유지, std::abs 권장) ----------
bool Intersects(const Plane& P, const FVector4& Center, const FVector4& Extents)
{
    const float Distance = Dot3(P.Normal, Center) - P.Distance;

    // AABB의 법선 방향 투영 반경
    const float Radius =
        std::abs(P.Normal.X) * Extents.X +
        std::abs(P.Normal.Y) * Extents.Y +
        std::abs(P.Normal.Z) * Extents.Z;

    return Distance + Radius >= 0.0f;
}

bool IsAABBVisible(const Frustum& F, const FBound& B)
{
    const FVector  Center3 = (B.Min + B.Max) * 0.5f;
    const FVector  Extents3 = (B.Max - B.Min) * 0.5f; // 양수

    const FVector4 Center = FVector4(Center3.X, Center3.Y, Center3.Z, 1.0f); // 점
    const FVector4 Extents = FVector4(Extents3.X, Extents3.Y, Extents3.Z, 0.0f); // 방향

    return Intersects(F.LeftFace, Center, Extents) &&
        Intersects(F.RightFace, Center, Extents) &&
        Intersects(F.TopFace, Center, Extents) &&
        Intersects(F.BottomFace, Center, Extents) &&
        Intersects(F.NearFace, Center, Extents) &&
        Intersects(F.FarFace, Center, Extents);
}

*/

// AVX-optimized culling for 8 AABBs
uint8_t AreAABBsVisible_8_AVX(const Frustum& Frustum, const FBound Bounds[8])
{
    // This function performs frustum culling for 8 AABBs simultaneously using AVX2.
    // It works by testing all 8 boxes against each of the 6 frustum planes.

    // 1. Transpose AoS (Array of Structures) to SoA (Structure of Arrays)
    // We need to rearrange the FBound data (Min.X, Min.Y, Min.Z, Max.X, ...)
    // into separate __m256 registers for each component (all 8 Min.X values together, etc.).

    __m256 centers_x, centers_y, centers_z;
    __m256 extents_x, extents_y, extents_z;

    // To transpose, we load data and shuffle it around.
    // Load 8 FVector from Bounds (Min and Max for 4 boxes)
    __m256 row0 = _mm256_loadu_ps(&Bounds[0].Min.X);
    __m256 row1 = _mm256_loadu_ps(&Bounds[1].Min.X);
    __m256 row2 = _mm256_loadu_ps(&Bounds[2].Min.X);
    __m256 row3 = _mm256_loadu_ps(&Bounds[3].Min.X);
    __m256 row4 = _mm256_loadu_ps(&Bounds[4].Min.X);
    __m256 row5 = _mm256_loadu_ps(&Bounds[5].Min.X);
    __m256 row6 = _mm256_loadu_ps(&Bounds[6].Min.X);
    __m256 row7 = _mm256_loadu_ps(&Bounds[7].Min.X);

    // Transpose 8x8 matrix of floats
    __m256 t0, t1, t2, t3, t4, t5, t6, t7;
    t0 = _mm256_unpacklo_ps(row0, row1);
    t1 = _mm256_unpackhi_ps(row0, row1);
    t2 = _mm256_unpacklo_ps(row2, row3);
    t3 = _mm256_unpackhi_ps(row2, row3);
    t4 = _mm256_unpacklo_ps(row4, row5);
    t5 = _mm256_unpackhi_ps(row4, row5);
    t6 = _mm256_unpacklo_ps(row6, row7);
    t7 = _mm256_unpackhi_ps(row6, row7);

    __m256 tt0, tt1, tt2, tt3, tt4, tt5, tt6, tt7;
    tt0 = _mm256_shuffle_ps(t0, t2, _MM_SHUFFLE(1, 0, 1, 0));
    tt1 = _mm256_shuffle_ps(t0, t2, _MM_SHUFFLE(3, 2, 3, 2));
    tt2 = _mm256_shuffle_ps(t1, t3, _MM_SHUFFLE(1, 0, 1, 0));
    tt3 = _mm256_shuffle_ps(t1, t3, _MM_SHUFFLE(3, 2, 3, 2));
    tt4 = _mm256_shuffle_ps(t4, t6, _MM_SHUFFLE(1, 0, 1, 0));
    tt5 = _mm256_shuffle_ps(t4, t6, _MM_SHUFFLE(3, 2, 3, 2));
    tt6 = _mm256_shuffle_ps(t5, t7, _MM_SHUFFLE(1, 0, 1, 0));
    tt7 = _mm256_shuffle_ps(t5, t7, _MM_SHUFFLE(3, 2, 3, 2));

    __m256 min_x = _mm256_permute2f128_ps(tt0, tt4, 0x20);
    __m256 min_y = _mm256_permute2f128_ps(tt1, tt5, 0x20);
    __m256 min_z = _mm256_permute2f128_ps(tt2, tt6, 0x20);
    __m256 max_x = _mm256_permute2f128_ps(tt0, tt4, 0x31);
    __m256 max_y = _mm256_permute2f128_ps(tt1, tt5, 0x31);
    __m256 max_z = _mm256_permute2f128_ps(tt2, tt6, 0x31);

    // Calculate centers and extents
    const __m256 half = _mm256_set1_ps(0.5f);
    centers_x = _mm256_mul_ps(_mm256_add_ps(max_x, min_x), half);
    centers_y = _mm256_mul_ps(_mm256_add_ps(max_y, min_y), half);
    centers_z = _mm256_mul_ps(_mm256_add_ps(max_z, min_z), half);
    extents_x = _mm256_mul_ps(_mm256_sub_ps(max_x, min_x), half);
    extents_y = _mm256_mul_ps(_mm256_sub_ps(max_y, min_y), half);
    extents_z = _mm256_mul_ps(_mm256_sub_ps(max_z, min_z), half);

    // 2. Perform Culling
    const Plane* planes = &Frustum.TopFace;
    uint32_t all_visible_mask = 0xFF; // Start with all 8 boxes being visible

    const __m256 sign_mask = _mm256_set1_ps(-0.0f); // Mask to get absolute value

    for (int i = 0; i < 6; ++i)
    {
        const Plane& p = planes[i];

        // Broadcast plane data to 256-bit registers
        __m256 plane_nx = _mm256_set1_ps(p.Normal.X);
        __m256 plane_ny = _mm256_set1_ps(p.Normal.Y);
        __m256 plane_nz = _mm256_set1_ps(p.Normal.Z);
        __m256 plane_d = _mm256_set1_ps(p.Distance);

        // Calculate signed distance from plane for all 8 box centers
        // dist = dot(center, normal) - plane_dist
        __m256 dist = _mm256_sub_ps(
            _mm256_add_ps(
                _mm256_add_ps(_mm256_mul_ps(centers_x, plane_nx), _mm256_mul_ps(centers_y, plane_ny)),
                _mm256_mul_ps(centers_z, plane_nz)
            ),
            plane_d
        );

        // Calculate projected radius of boxes onto the plane normal
        // radius = dot(extents, abs(normal))
        __m256 radius = _mm256_add_ps(
            _mm256_add_ps(
                _mm256_mul_ps(extents_x, _mm256_andnot_ps(sign_mask, plane_nx)),
                _mm256_mul_ps(extents_y, _mm256_andnot_ps(sign_mask, plane_ny))
            ),
            _mm256_mul_ps(extents_z, _mm256_andnot_ps(sign_mask, plane_nz))
        );

        // A box is outside if dist < -radius (or dist + radius < 0)
        __m256 comparison = _mm256_cmp_ps(_mm256_add_ps(dist, radius), _mm256_setzero_ps(), _CMP_GE_OQ);
        
        // Convert comparison result to an 8-bit mask
        int plane_mask = _mm256_movemask_ps(comparison);

        // Update the overall visibility mask. If a box is outside this plane,
        // its corresponding bit will be 0, and it will be culled for the rest of the process.
        all_visible_mask &= plane_mask;

        // Early exit: if all boxes are culled, no need to check other planes
        if (all_visible_mask == 0)
        {
            return 0;
        }
    }

    return static_cast<uint8_t>(all_visible_mask);
}