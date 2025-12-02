#include "pch.h"
#include "PhysXSimEventCallback.h"
#include "BodyInstance.h"
#include "HitResult.h"
#include "PrimitiveComponent.h"

void FPhysXSimEventCallback::onContact(const PxContactPairHeader& PairHeader, const PxContactPair* Pairs, PxU32 NbPairs)
{
    UPrimitiveComponent* CompA = GetCompFromPxActor(PairHeader.actors[0]);
    UPrimitiveComponent* CompB = GetCompFromPxActor(PairHeader.actors[1]);

    if (!CompA || !CompB || CompA->IsPendingDestroy() || CompB->IsPendingDestroy()) { return; }

    AActor* ActorA = CompA->GetOwner();
    AActor* ActorB = CompB->GetOwner();

    if (!ActorA || !ActorB) { return; }

    PxContactPairPoint ContactPoints[16];

    for (PxU32 i = 0; i < NbPairs; ++i)
    {
        const PxContactPair& CP = Pairs[i];

        if (CP.events & PxPairFlag::eNOTIFY_TOUCH_FOUND)
        {
            const PxShape* ShapeA = CP.shapes[0];
            const PxShape* ShapeB = CP.shapes[1];

            FName BoneNameA = GetBoneNameFromShape(ShapeA);
            FName BoneNameB = GetBoneNameFromShape(ShapeB);
            
            PxU32 ContactCount = CP.extractContacts(ContactPoints, 16);

            if (ContactCount > 0)
            {
                const PxContactPairPoint& Point = ContactPoints[0];
                FVector WorldNormal = PhysXConvert::FromPx(Point.normal);
                FVector WorldPos = PhysXConvert::FromPx(Point.position);

                // --- [HitResult A 생성 (A 입장에서의 충돌)] ---
                FHitResult HitA;
                HitA.bBlockingHit = true;
                HitA.Actor = ActorB;
                HitA.Component = CompB;
                HitA.ImpactPoint = WorldPos;
                HitA.ImpactNormal = WorldNormal; 
                HitA.Location = ActorA->GetActorLocation();
                HitA.Distance = 0.0f; // 물리 충돌은 거리 0
                HitA.Item = Point.internalFaceIndex1;
                HitA.MyBoneName = BoneNameA; 
                HitA.BoneName = BoneNameB;
                
                ActorA->OnComponentHit.Broadcast(CompA, CompB, HitA);

                // --- [HitResult B 생성 (B 입장에서의 충돌)] ---
                FHitResult HitB;
                HitB.bBlockingHit = true;
                HitB.Actor = ActorA;
                HitB.Component = CompA;
                HitB.ImpactPoint = WorldPos;
                HitB.ImpactNormal = -WorldNormal; 
                HitB.Location = ActorB->GetActorLocation();
                HitB.Distance = 0.0f; // 물리 충돌은 거리 0
                HitB.Item = Point.internalFaceIndex1;
                HitA.MyBoneName = BoneNameB;
                HitA.BoneName = BoneNameA;

                ActorB->OnComponentHit.Broadcast(CompB, CompA, HitB);
            }
        }
    }
}

void FPhysXSimEventCallback::onTrigger(PxTriggerPair* Pairs, PxU32 Count)
{
    for (PxU32 i = 0; i < Count; ++i)
    {
        const PxTriggerPair& TP = Pairs[i];
        
        UPrimitiveComponent* TriggerComp = GetCompFromPxActor(TP.triggerActor);
        UPrimitiveComponent* OtherComp = GetCompFromPxActor(TP.otherActor);
        if (!TriggerComp || !OtherComp) { continue; }

        AActor* TriggerActor = TriggerComp->GetOwner();
        AActor* OtherActor = OtherComp->GetOwner();
        if (!TriggerActor || !OtherActor) { continue; }

        // OnComponentBeginOverlap
        if (TP.status == PxPairFlag::eNOTIFY_TOUCH_FOUND)
        {
            TriggerActor->OnComponentBeginOverlap.Broadcast(TriggerComp, OtherComp);
            OtherActor->OnComponentBeginOverlap.Broadcast(OtherComp, TriggerComp);
        }
        // OnComponentEndOverlap
        else if (TP.status == PxPairFlag::eNOTIFY_TOUCH_LOST)
        {
            TriggerActor->OnComponentEndOverlap.Broadcast(TriggerComp, OtherComp);
            OtherActor->OnComponentEndOverlap.Broadcast(OtherComp, TriggerComp);
        }
    }
}

UPrimitiveComponent* FPhysXSimEventCallback::GetCompFromPxActor(PxRigidActor* InActor)
{
    if (!InActor || !InActor->userData) { return nullptr; }

    auto* BodyInst = static_cast<FBodyInstance*>(InActor->userData);
    if (!BodyInst) { return nullptr; }

    return BodyInst->OwnerComponent;
}

FName FPhysXSimEventCallback::GetBoneNameFromShape(const physx::PxShape* Shape)
{
    if (!Shape || !Shape->userData) return FName();
    FName* NamePtr = static_cast<FName*>(Shape->userData);
    return *NamePtr;
}
