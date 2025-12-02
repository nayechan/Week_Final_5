#include "pch.h"
#include "PhysXPublic.h"

#include "BodyInstance.h"
#include "PrimitiveComponent.h"

void FRigidBodyCollisionInfo::SetFrom(FBodyInstance* BodyInst)
{
    if (!BodyInst) { return; }

    UPrimitiveComponent* OwnerComp = BodyInst->OwnerComponent;
    if (OwnerComp)
    {
        Component = OwnerComp;
        Actor = OwnerComp->GetOwner();
    }

    BodyIndex = BodyInst->BodySetup->BoneIndex; 
    BoneName = BodyInst->BodySetup->BoneName;
}

bool FRigidBodyCollisionInfo::IsValid() const
{
    assert(false && "TWeakObjectPtr을 무조건 구현해야 함");
    return false;
}

bool FCollisionNotifyInfo::IsValidForNotify() const
{
    return Info0.IsValid() && Info1.IsValid();
}
