#pragma once
#include "PhysXSupport.h"

class FPhysXSimEventCallback : public PxSimulationEventCallback
{
public:
    FPhysXSimEventCallback(FPhysScene* InOwnerScene);
    virtual ~FPhysXSimEventCallback();

    // ========================================================
    // PxSimulationEventCallback Interface
    // ========================================================

    virtual void onContact(const PxContactPairHeader& pairHeader, const PxContactPair* pairs, PxU32 nbPairs) override;

    virtual void onTrigger(PxTriggerPair* pairs, PxU32 count) override;

    virtual void onConstraintBreak(PxConstraintInfo* constraints, PxU32 count) override {}
    virtual void onWake(PxActor** actors, PxU32 count) override;
    virtual void onSleep(PxActor** actors, PxU32 count) override;
    virtual void onAdvance(const PxRigidBody* const* bodyBuffer, const PxTransform* poseBuffer, const PxU32 count) override {}
    // ========================================================

private:
    FPhysScene* OwnerScene;
};
