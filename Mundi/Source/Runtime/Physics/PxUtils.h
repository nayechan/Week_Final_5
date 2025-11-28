#pragma once
#include <PxPhysicsAPI.h>

struct FHitResult;
struct FOverlapResult;
struct FSweepResult;

namespace PhysXUtils
{
    // Raycast 결과 변환
    FHitResult ToHitResult(const physx::PxRaycastHit& pHit);

    // Overlap 결과 변환
    FOverlapResult ToOverlapResult(const physx::PxOverlapHit& pHit);

    // Sweep 결과 변환
    FSweepResult ToSweepResult(const physx::PxSweepHit& pHit);
}
