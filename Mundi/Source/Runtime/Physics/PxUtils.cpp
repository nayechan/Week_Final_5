#include "pch.h"
#include "PxUtils.h"
#include "BodyInstance.h"
#include "HitResult.h"
#include "PrimitiveComponent.h"

FVector2D::FVector2D(const physx::PxVec2& P) : X(P.x), Y(P.y) {}
FVector2D::operator physx::PxVec2() const { return {X, Y}; }

FVector::FVector(const physx::PxVec3& P) : X(P.x), Y(P.y), Z(P.z) {}
FVector::operator physx::PxVec3() const { return {X, Y, Z}; }

FQuat::FQuat(const physx::PxQuat& p) : X(p.x), Y(p.y), Z(p.z), W(p.w) {}
FQuat::operator physx::PxQuat() const { return {X, Y, Z, W}; }

FTransform::FTransform(const physx::PxTransform& pT)
{
    Translation = pT.p;
    Rotation = pT.q;
    // PhysX는 스케일이 없으므로 기본값 1
    Scale3D = FVector(1.0f, 1.0f, 1.0f);
}

FTransform::operator physx::PxTransform() const
{
    return physx::PxTransform(Translation, Rotation);
}

namespace PhysXUtils
{
    using namespace physx;

    // -----------------------------------------------------------------------
    // 헬퍼 함수 (Raycast, Sweep, Overlap 공통 로직)
    // -----------------------------------------------------------------------
    static void FillHitResultFromActor(FHitResult& OutResult, PxRigidActor* Actor, PxShape* Shape, PxU32 FaceIndex)
    {
        if (!Actor || !Actor->userData) return;

        // userData를 FBodyInstance로 캐스팅
        FBodyInstance* BodyInst = static_cast<FBodyInstance*>(Actor->userData);
        if (BodyInst)
        {
            OutResult.Component = BodyInst->OwnerComponent;
            if (OutResult.Component)
            {
                OutResult.Actor = OutResult.Component->GetOwner();
            }

            OutResult.BoneName = BodyInst->BoneName;
            OutResult.Item = static_cast<int32>(FaceIndex);
            if (Shape)
            {
                // 삼각형 메쉬인 경우 FaceIndex로 재질을 찾고, 아니면 Shape의 재질 사용
                PxMaterial* PxMat = nullptr;
                if (FaceIndex != 0xFFFFFFFF) 
                {
                    PxMat = Shape->getMaterialFromInternalFaceIndex(FaceIndex);
                }
                else 
                {
                    PxMaterial* Materials[1];
                    if (Shape->getMaterials(Materials, 1) > 0)
                    {
                        PxMat = Materials[0];
                    }
                }

                // PhysX 재질의 userData에 연결해둔 UPhysicalMaterial 가져오기
                if (PxMat && PxMat->userData)
                {
                    OutResult.PhysMaterial = static_cast<UPhysicalMaterial*>(PxMat->userData);
                }
            }
        }
    }

    FHitResult ToHitResult(const PxRaycastHit& pHit)
    {
        FHitResult Result;
        Result.bBlockingHit = true;
        
        Result.Distance = pHit.distance;
        Result.ImpactPoint = pHit.position;
        Result.ImpactNormal = pHit.normal;
        Result.Location = pHit.position; 

        FillHitResultFromActor(Result, pHit.actor, pHit.shape, pHit.faceIndex);

        return Result;
    }

    FOverlapResult ToOverlapResult(const PxOverlapHit& pHit)
    {
        FOverlapResult Result;
        
        if (pHit.actor && pHit.actor->userData)
        {
            FBodyInstance* BodyInst = static_cast<FBodyInstance*>(pHit.actor->userData);
            if (BodyInst)
            {
                Result.Component = BodyInst->OwnerComponent;
                if (Result.Component)
                {
                    Result.Actor = Result.Component->GetOwner();
                }
            }
        }
        return Result;
    }

    FSweepResult ToSweepResult(const PxSweepHit& pHit)
    {
        FSweepResult Result;
        Result.bBlockingHit = true;

        Result.Distance = pHit.distance;
        Result.ImpactPoint = pHit.position;
        Result.ImpactNormal = pHit.normal;
        // !!!주의!!! Sweep에서 Location(내 중심점)은 여기서 알 수 없음
        // 호출하는 함수(Trace)에서 [Start + Dir * Distance] 로 계산해서 덮어씌워야 함
        Result.Location = pHit.position; 

        FillHitResultFromActor(Result, pHit.actor, pHit.shape, pHit.faceIndex);

        return Result;
    }
}