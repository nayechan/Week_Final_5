#include "pch.h"
#include "PhysXSimEventCallback.h"

#include "BodyInstance.h"
#include "PhysScene.h"

struct FBodyInstance;

FPhysXSimEventCallback::FPhysXSimEventCallback(FPhysScene* InOwnerScene)
    : OwnerScene(InOwnerScene)
{
}

FPhysXSimEventCallback::~FPhysXSimEventCallback()
{
}

void FPhysXSimEventCallback::onContact(const PxContactPairHeader& pairHeader, const PxContactPair* pairs, PxU32 nbPairs)
{
    // @note 이미 삭제된 액터가 포함된 경우 무시 (안전장치)
    if (pairHeader.flags & (PxContactPairHeaderFlag::eREMOVED_ACTOR_0 | PxContactPairHeaderFlag::eREMOVED_ACTOR_1))
    {
        return;
    }

    PxRigidActor* Actor0 = pairHeader.actors[0]->is<PxRigidActor>();
    PxRigidActor* Actor1 = pairHeader.actors[1]->is<PxRigidActor>();

    if (!Actor0 || !Actor1) return;

    FBodyInstance* BodyInst0 = static_cast<FBodyInstance*>(Actor0->userData);
    FBodyInstance* BodyInst1 = static_cast<FBodyInstance*>(Actor1->userData);

    if (!BodyInst0 || !BodyInst1) { return; }

    for (PxU32 i = 0; i < nbPairs; ++i)
    {
        const PxContactPair& curPair = pairs[i];

        // @note 쉐이프가 삭제되었으면 무시
        if (curPair.flags & (PxContactPairFlag::eREMOVED_SHAPE_0 | PxContactPairFlag::eREMOVED_SHAPE_1))
        {
            continue;
        }

        // @note 이벤트 필터링: '충돌 시작(Touch Found)' 또는 '강한 충돌(Threshold Force)'인 경우만 처리
        bool bIsHitEvent = (curPair.events & (PxPairFlag::eNOTIFY_TOUCH_FOUND | PxPairFlag::eNOTIFY_THRESHOLD_FORCE_FOUND));
        
        if (bIsHitEvent)
        {
            FCollisionNotifyInfo NotifyInfo;

            // @todo 이벤트를 받을지 여부 확인 (최적화)
            // NotifyInfo.bCallEvent0 = BodyInst0->bNotifyRigidBodyCollision;
            // NotifyInfo.bCallEvent1 = BodyInst1->bNotifyRigidBodyCollision;

            NotifyInfo.Type = ECollisionNotifyType::Hit;
            
            NotifyInfo.bCallEvent0 = true;
            NotifyInfo.bCallEvent1 = true;

            if (!NotifyInfo.bCallEvent0 && !NotifyInfo.bCallEvent1)
            {
                continue;
            }

            NotifyInfo.Info0.SetFrom(BodyInst0);
            NotifyInfo.Info1.SetFrom(BodyInst1);

            PxContactStreamIterator Iter(curPair.contactPatches, curPair.contactPoints, curPair.getInternalFaceIndices(), curPair.patchCount, curPair.contactCount);

            PxVec3 AccumPoint(0.0f);
            PxVec3 AccumNormal(0.0f);
            float TotalImpulse = 0.0f;
            int32 ContactCount = 0;
            PxU32 GlobalContactIndex = 0;

            while (Iter.hasNextPatch())
            {
                Iter.nextPatch();
                while (Iter.hasNextContact())
                {
                    Iter.nextContact();

                    AccumPoint += Iter.getContactPoint();
                    AccumNormal += Iter.getContactNormal();
                    
                    if (curPair.contactImpulses)
                    {
                        TotalImpulse += curPair.contactImpulses[GlobalContactIndex];
                    }

                    GlobalContactIndex++;
                    ContactCount++;
                }
            }

            if (ContactCount > 0)
            {
                NotifyInfo.ImpactPoint = P2UVector(AccumPoint * (1.0f / ContactCount));
                NotifyInfo.ImpactNormal = P2UVector(AccumNormal.getNormalized());
                NotifyInfo.TotalImpulse = TotalImpulse;
            }
            else
            {
                // 접점이 없는데 충돌 이벤트가 떴다면 (드문 경우), 액터 위치 등으로 대체
                NotifyInfo.ImpactPoint = P2UVector(Actor0->getGlobalPose().p);
                NotifyInfo.ImpactNormal = FVector(0.0f, 0.0f, 1.0f);
            }

            if (OwnerScene)
            {
                OwnerScene->AddPendingCollisionNotify(NotifyInfo);
            }
        }
    }
}

void FPhysXSimEventCallback::onTrigger(PxTriggerPair* pairs, PxU32 count)
{
    for (PxU32 i = 0; i < count; i++)
    {
        const PxTriggerPair& curPair = pairs[i];

        if (curPair.flags & (PxTriggerPairFlag::eREMOVED_SHAPE_TRIGGER | PxTriggerPairFlag::eREMOVED_SHAPE_OTHER))
            continue;

        PxRigidActor* TriggerActor = curPair.triggerActor;
        PxRigidActor* OtherActor = curPair.otherActor;

        if (!TriggerActor || !OtherActor) continue;

        FBodyInstance* TriggerBody = static_cast<FBodyInstance*>(TriggerActor->userData);
        FBodyInstance* OtherBody = static_cast<FBodyInstance*>(OtherActor->userData);

        if (TriggerBody && OtherBody && OwnerScene)
        {
            FCollisionNotifyInfo NotifyInfo;
            
            if (curPair.status == PxPairFlag::eNOTIFY_TOUCH_FOUND)
            {
                NotifyInfo.Type = ECollisionNotifyType::BeginOverlap;
            }
            else if (curPair.status == PxPairFlag::eNOTIFY_TOUCH_LOST)
            {
                NotifyInfo.Type = ECollisionNotifyType::EndOverlap;
            }
            else
            {
                continue;
            }

            NotifyInfo.bCallEvent0 = true; 
            NotifyInfo.bCallEvent1 = true;

            NotifyInfo.Info0.SetFrom(TriggerBody);
            NotifyInfo.Info1.SetFrom(OtherBody);

            OwnerScene->AddPendingCollisionNotify(NotifyInfo);
        }
    }
}

void FPhysXSimEventCallback::onWake(PxActor** actors, PxU32 count)
{
    if (!OwnerScene) return;

    for (PxU32 i = 0; i < count; i++)
    {
        PxRigidActor* Actor = actors[i]->is<PxRigidActor>();
        if (!Actor) continue;

        FBodyInstance* BodyInst = static_cast<FBodyInstance*>(Actor->userData);
        if (BodyInst)
        {
            FCollisionNotifyInfo NotifyInfo;
            NotifyInfo.Type = ECollisionNotifyType::Wake;
            NotifyInfo.Info0.SetFrom(BodyInst); 
            NotifyInfo.bCallEvent0 = true; 

            OwnerScene->AddPendingCollisionNotify(NotifyInfo);
        }
    }
}

void FPhysXSimEventCallback::onSleep(PxActor** actors, PxU32 count)
{
    if (!OwnerScene) return;

    for (PxU32 i = 0; i < count; i++)
    {
        PxRigidActor* Actor = actors[i]->is<PxRigidActor>();
        if (!Actor) continue;

        FBodyInstance* BodyInst = static_cast<FBodyInstance*>(Actor->userData);
        if (BodyInst)
        {
            FCollisionNotifyInfo NotifyInfo;
            NotifyInfo.Type = ECollisionNotifyType::Sleep;
            NotifyInfo.Info0.SetFrom(BodyInst);
            NotifyInfo.bCallEvent0 = true;

            OwnerScene->AddPendingCollisionNotify(NotifyInfo);
        }
    }
}
