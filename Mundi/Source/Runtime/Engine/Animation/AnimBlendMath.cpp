#include "pch.h"
#include "AnimBlendMath.h"

namespace
{
    inline float Dot2(const FVector2D& A, const FVector2D& B)
    {
        return A.X * B.X + A.Y * B.Y;
    }
}

namespace AnimBlendMath
{
    bool ComputeBarycentric(const FVector2D& P,
                            const FVector2D& A,
                            const FVector2D& B,
                            const FVector2D& C,
                            float& OutU, float& OutV, float& OutW)
    {
        const FVector2D v0 = B - A;
        const FVector2D v1 = C - A;
        const FVector2D v2 = P - A;

        const float d00 = Dot2(v0, v0);
        const float d01 = Dot2(v0, v1);
        const float d11 = Dot2(v1, v1);
        const float d20 = Dot2(v2, v0);
        const float d21 = Dot2(v2, v1);

        const float denom = d00 * d11 - d01 * d01;
        if (std::fabs(denom) <= 1e-12f)
        {
            OutU = OutV = OutW = 0.f;
            return false; // degenerate triangle
        }

        const float v = (d11 * d20 - d01 * d21) / denom;
        const float w = (d00 * d21 - d01 * d20) / denom;
        const float u = 1.0f - v - w;

        OutU = u; OutV = v; OutW = w;
        return true;
    }

    bool IsInsideTriangle(float U, float V, float W, float Epsilon)
    {
        const float Sum = U + V + W;
        const bool bSumOK = std::fabs(Sum - 1.0f) <= (10.0f * Epsilon);
        return (U >= -Epsilon) && (V >= -Epsilon) && (W >= -Epsilon) && bSumOK;
    }

    void ClampBarycentricToTriangle(float& U, float& V, float& W)
    {
        const float U0 = U, V0 = V, W0 = W;
        if (U < 0.f) U = 0.f;
        if (V < 0.f) V = 0.f;
        if (W < 0.f) W = 0.f;

        float Sum = U + V + W;
        if (Sum <= 1e-8f)
        {
            // All clamped to zero; pick the largest original component as 1
            if (U0 >= V0 && U0 >= W0) { U = 1.f; V = 0.f; W = 0.f; }
            else if (V0 >= U0 && V0 >= W0) { U = 0.f; V = 1.f; W = 0.f; }
            else { U = 0.f; V = 0.f; W = 1.f; }
            return;
        }

        const float Inv = 1.0f / Sum;
        U *= Inv; V *= Inv; W *= Inv;
    }
}

