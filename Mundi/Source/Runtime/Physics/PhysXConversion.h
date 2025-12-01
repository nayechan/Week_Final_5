#pragma once
#include <PxPhysicsAPI.h>
#include "Vector.h"

/**
 * 좌표계 변환 유틸리티
 *
 * 프로젝트 좌표계: Z-up, X-front (왼손 좌표계)
 * PhysX 좌표계:    Y-up, Z-front (오른손 좌표계)
 *
 * 축 매핑:
 *   프로젝트 X → PhysX Z
 *   프로젝트 Y → PhysX X
 *   프로젝트 Z → PhysX Y
 */
namespace PhysXConvert
{
    // --- 위치(Vector) 변환 --- 

    // 프로젝트 → PhysX
    inline physx::PxVec3 ToPx(const FVector& V)
    {
        // X→Z, Y→X, Z→Y
        return physx::PxVec3(-V.Y, V.Z, V.X);
    }

    // PhysX → 프로젝트
    inline FVector FromPx(const physx::PxVec3& V)
    {
        // Z→X, X→Y, Y→Z
        return FVector(V.z, -V.x, V.y);
    }

    // --- 회전(Quaternion) 변환 --- 

    // 프로젝트 → PhysX
    inline physx::PxQuat ToPx(const FQuat& Q)
    {
        // 쿼터니언 축도 동일하게 변환
        // (X, Y, Z, W) → (Y, Z, X, W)
        return physx::PxQuat(-Q.Y, Q.Z, Q.X, -Q.W);
    }

    // PhysX → 프로젝트
    inline FQuat FromPx(const physx::PxQuat& Q)
    {
        // (X, Y, Z, W) → (Z, X, Y, W)
        return FQuat(Q.z, -Q.x, Q.y, -Q.w);
    }

    // --- Transform 변환 --- 

    // 프로젝트 → PhysX
    inline physx::PxTransform ToPx(const FTransform& T)
    {
        return physx::PxTransform(ToPx(T.Translation), ToPx(T.Rotation));
    }

    // PhysX → 프로젝트
    inline FTransform FromPx(const physx::PxTransform& T)
    {
        return FTransform(FromPx(T.p), FromPx(T.q), FVector::One());
    }

    // --- 스케일 변환 (축 순서 변경) --- 

    inline physx::PxVec3 ScaleToPx(const FVector& Scale)
    {
        return physx::PxVec3(Scale.Y, Scale.Z, Scale.X);
    }

    inline FVector ScaleFromPx(const physx::PxVec3& Scale)
    {
        return FVector(Scale.z, Scale.x, Scale.y);
    }

    // --- Capsule 축 변환 --- 

    /**
     * Capsule 축 회전
     * 프로젝트: Z축이 캡슐 축
     * PhysX:    X축이 캡슐 축
     *
     * Z축 → X축 변환을 위해 Y축 기준 -90도 회전 필요
     * (좌표계 변환 후 적용)
     */
    inline physx::PxQuat GetCapsuleAxisRotation()
    {
        // PhysX 좌표계에서 Y축 기준 90도 회전
        // 이렇게 하면 Z축(프로젝트의 캡슐 축)이 X축(PhysX의 캡슐 축)으로 정렬됨
        return physx::PxQuat(physx::PxHalfPi, physx::PxVec3(0.0f, 1.0f, 0.0f));
    }

    /**
     * Capsule Transform 변환 (축 회전 포함)
     * 캡슐 Shape 생성 시 사용
     */
    inline physx::PxTransform ToPxCapsule(const FTransform& T)
    {
        physx::PxTransform Result = ToPx(T);
        Result.q = Result.q * GetCapsuleAxisRotation();
        return Result;
    }

    // --- Box Half Extent 변환 --- 

    /**
     * Box Half Extent 변환
     * 프로젝트의 (X, Y, Z) half extent를 PhysX 축에 맞게 변환
     */
    inline physx::PxVec3 BoxHalfExtentToPx(float X, float Y, float Z)
    {
        // X→Z, Y→X, Z→Y
        return physx::PxVec3(Y, Z, X);
    }

    inline physx::PxVec3 BoxHalfExtentToPx(const FVector& HalfExtent)
    {
        return BoxHalfExtentToPx(HalfExtent.X, HalfExtent.Y, HalfExtent.Z);
    }

    // --- 방향 벡터 변환 (정규화된 방향) --- 

    inline physx::PxVec3 DirectionToPx(const FVector& Dir)
    {
        return ToPx(Dir);
    }

    inline FVector DirectionFromPx(const physx::PxVec3& Dir)
    {
        return FromPx(Dir);
    }

    // --- 각속도/토크 변환 --- 

    inline physx::PxVec3 AngularToPx(const FVector& Angular)
    {
        return ToPx(Angular);
    }

    inline FVector AngularFromPx(const physx::PxVec3& Angular)
    {
        return FromPx(Angular);
    }

    // --- 유틸리티: 단위 변환 --- 

    // 도(Degrees) → 라디안
    inline float DegreesToRadians(float Degrees)
    {
        return Degrees * (PI / 180.0f);
    }

    // 라디안 → 도(Degrees)
    inline float RadiansToDegrees(float Radians)
    {
        return Radians * (180.0f / PI);
    }
}

// --- 편의용 매크로 (선택적 사용) --- 

#define P2M(pxValue) PhysXConvert::FromPx(pxValue)  // PhysX to Mundi
#define M2P(mValue)  PhysXConvert::ToPx(mValue)     // Mundi to PhysX
