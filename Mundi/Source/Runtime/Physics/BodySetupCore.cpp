#include "pch.h"
#include "BodySetupCore.h"
#include "JsonSerializer.h"

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

    if (bInIsLoading)
    {
        // BoneName 로드
        FString BoneNameStr;
        FJsonSerializer::ReadString(InOutHandle, "BoneName", BoneNameStr, "None", false);
        BoneName = FName(BoneNameStr);

        // PhysicsType 로드 (enum을 int로)
        int32 PhysicsTypeInt = 0;
        FJsonSerializer::ReadInt32(InOutHandle, "PhysicsType", PhysicsTypeInt);
        PhysicsType = static_cast<EPhysicsType>(PhysicsTypeInt);

        // CollisionResponse 로드
        int32 CollisionResponseInt = 0;
        FJsonSerializer::ReadInt32(InOutHandle, "CollisionResponse", CollisionResponseInt);
        CollisionResponse = static_cast<EBodyCollisionResponse::Type>(CollisionResponseInt);
    }
    else // 저장
    {
        InOutHandle["BoneName"] = BoneName.ToString();
        InOutHandle["PhysicsType"] = static_cast<int>(PhysicsType);
        InOutHandle["CollisionResponse"] = static_cast<int>(CollisionResponse);
    }
}
