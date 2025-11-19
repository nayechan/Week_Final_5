#pragma once
#include "Vector.h"

// Lightweight helpers for 2D triangular blend math
// Used by BlendSpace2D node to compute barycentric coordinates and handle clamping.

namespace AnimBlendMath
{
    // Compute barycentric coordinates (u,v,w) of point P with respect to triangle (A,B,C).
    // Returns false if the triangle is degenerate (area ~ 0). On success, u+v+w = 1 (within numerical error).
    bool ComputeBarycentric(const FVector2D& P,
                            const FVector2D& A,
                            const FVector2D& B,
                            const FVector2D& C,
                            float& OutU, float& OutV, float& OutW);

    // True if inside or on the triangle considering tolerance. Weights should sum to ~1.
    bool IsInsideTriangle(float U, float V, float W, float Epsilon = 1e-4f);

    // Clamp barycentric coordinates to the triangle: any negative weight is set to 0 and others renormalized.
    // If all weights become zero (degenerate input), selects the largest original weight as 1.
    void ClampBarycentricToTriangle(float& U, float& V, float& W);
}

