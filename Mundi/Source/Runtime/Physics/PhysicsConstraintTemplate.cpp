#include "pch.h"
#include "PhysicsConstraintTemplate.h"
#include "JsonSerializer.h"

// --- 생성자/소멸자 ---

UPhysicsConstraintTemplate::UPhysicsConstraintTemplate()
{
}

UPhysicsConstraintTemplate::~UPhysicsConstraintTemplate()
{
}

// --- 직렬화 ---

void UPhysicsConstraintTemplate::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
    Super::Serialize(bInIsLoading, InOutHandle);

    FConstraintInstance& CI = DefaultInstance;

    if (bInIsLoading)
    {
        // 본 이름 로드
        FString JointNameStr, Bone1Str, Bone2Str;
        FJsonSerializer::ReadString(InOutHandle, "JointName", JointNameStr, "None", false);
        FJsonSerializer::ReadString(InOutHandle, "Bone1", Bone1Str, "None", false);
        FJsonSerializer::ReadString(InOutHandle, "Bone2", Bone2Str, "None", false);
        CI.JointName = FName(JointNameStr);
        CI.ConstraintBone1 = FName(Bone1Str);
        CI.ConstraintBone2 = FName(Bone2Str);

        // Linear Limits 로드
        int32 LinearXInt = 2, LinearYInt = 2, LinearZInt = 2;
        FJsonSerializer::ReadInt32(InOutHandle, "LinearXMotion", LinearXInt);
        FJsonSerializer::ReadInt32(InOutHandle, "LinearYMotion", LinearYInt);
        FJsonSerializer::ReadInt32(InOutHandle, "LinearZMotion", LinearZInt);
        CI.LinearXMotion = static_cast<ELinearConstraintMotion>(LinearXInt);
        CI.LinearYMotion = static_cast<ELinearConstraintMotion>(LinearYInt);
        CI.LinearZMotion = static_cast<ELinearConstraintMotion>(LinearZInt);
        FJsonSerializer::ReadFloat(InOutHandle, "LinearLimit", CI.LinearLimit);

        // Angular Limits 로드
        int32 TwistInt = 1, Swing1Int = 1, Swing2Int = 1;
        FJsonSerializer::ReadInt32(InOutHandle, "TwistMotion", TwistInt);
        FJsonSerializer::ReadInt32(InOutHandle, "Swing1Motion", Swing1Int);
        FJsonSerializer::ReadInt32(InOutHandle, "Swing2Motion", Swing2Int);
        CI.AngularTwistMotion = static_cast<EAngularConstraintMotion>(TwistInt);
        CI.AngularSwing1Motion = static_cast<EAngularConstraintMotion>(Swing1Int);
        CI.AngularSwing2Motion = static_cast<EAngularConstraintMotion>(Swing2Int);
        FJsonSerializer::ReadFloat(InOutHandle, "TwistLimit", CI.TwistLimitAngle);
        FJsonSerializer::ReadFloat(InOutHandle, "Swing1Limit", CI.Swing1LimitAngle);
        FJsonSerializer::ReadFloat(InOutHandle, "Swing2Limit", CI.Swing2LimitAngle);

        // Collision 로드
        FJsonSerializer::ReadBool(InOutHandle, "DisableCollision", CI.bDisableCollision);
    }
    else // 저장
    {
        InOutHandle["JointName"] = CI.JointName.ToString();
        InOutHandle["Bone1"] = CI.ConstraintBone1.ToString();
        InOutHandle["Bone2"] = CI.ConstraintBone2.ToString();

        // Linear Limits
        InOutHandle["LinearXMotion"] = static_cast<int>(CI.LinearXMotion);
        InOutHandle["LinearYMotion"] = static_cast<int>(CI.LinearYMotion);
        InOutHandle["LinearZMotion"] = static_cast<int>(CI.LinearZMotion);
        InOutHandle["LinearLimit"] = CI.LinearLimit;

        // Angular Limits
        InOutHandle["TwistMotion"] = static_cast<int>(CI.AngularTwistMotion);
        InOutHandle["Swing1Motion"] = static_cast<int>(CI.AngularSwing1Motion);
        InOutHandle["Swing2Motion"] = static_cast<int>(CI.AngularSwing2Motion);
        InOutHandle["TwistLimit"] = CI.TwistLimitAngle;
        InOutHandle["Swing1Limit"] = CI.Swing1LimitAngle;
        InOutHandle["Swing2Limit"] = CI.Swing2LimitAngle;

        InOutHandle["DisableCollision"] = CI.bDisableCollision;
    }
}
