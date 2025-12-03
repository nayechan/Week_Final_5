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
    return Actor.IsValid() && Component.IsValid();
}

bool FCollisionNotifyInfo::IsValidForNotify() const
{
    if (Type == ECollisionNotifyType::Wake || Type == ECollisionNotifyType::Sleep)
    {
        return Info0.IsValid();
    }
    return Info0.IsValid() && Info1.IsValid();
}
