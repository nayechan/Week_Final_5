#include "pch.h"
#include "BodySetupCore.h"

// --- 생성자/소멸자 ---

UBodySetupCore::UBodySetupCore()
    : BoneName("None")
    , PhysicsType(EPhysicsType::Default)
    , CollisionResponse(EBodyCollisionResponse::BodyCollision_Enabled)
{
}

UBodySetupCore::~UBodySetupCore()
{
}

// --- 직렬화 ---

void UBodySetupCore::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
    Super::Serialize(bInIsLoading, InOutHandle);

    // TODO: BoneName, PhysicsType 등 직렬화
}
