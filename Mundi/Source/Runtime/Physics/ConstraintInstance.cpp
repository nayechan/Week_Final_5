#include "pch.h"
#include "ConstraintInstance.h"
#include "BodyInstance.h"
#include "PhysXConversion.h"
#include "PhysicsSystem.h"
#include <cmath>

using namespace physx;

// ===== 헬퍼: 방향 벡터에서 회전 쿼터니언 계산 (Twist 축 = 방향) =====
// PhysX D6 Joint의 Twist 축(X축)을 뼈의 길이 방향으로 정렬하는 회전 계산
static PxQuat ComputeJointFrameRotation(const PxVec3& Direction)
{
    PxVec3 DefaultAxis(1, 0, 0);  // PhysX D6 Joint의 기본 Twist 축
    PxVec3 Dir = Direction.getNormalized();

    float Dot = DefaultAxis.dot(Dir);

    // 이미 정렬됨 (같은 방향)
    if (Dot > 0.9999f)
    {
        return PxQuat(PxIdentity);
    }

    // 반대 방향
    if (Dot < -0.9999f)
    {
        return PxQuat(PxPi, PxVec3(0, 1, 0));
    }

    // 두 벡터 사이의 회전 축과 각도 계산
    PxVec3 Axis = DefaultAxis.cross(Dir).getNormalized();
    float Angle = std::acos(Dot);
    return PxQuat(Angle, Axis);
}

// ===== 1. 동적 Frame 계산 방식 (자동 계산기) =====
void FConstraintInstance::InitConstraint(
    FBodyInstance* Body1,
    FBodyInstance* Body2,
    UPrimitiveComponent* InOwnerComponent)
{
    // 유효성 검사 (기존 동일)
    if (!Body1 || !Body2) return;
    PxRigidDynamic* Parent = Body1->RigidActor ? Body1->RigidActor->is<PxRigidDynamic>() : nullptr;
    PxRigidDynamic* Child = Body2->RigidActor ? Body2->RigidActor->is<PxRigidDynamic>() : nullptr;
    if (!Parent || !Child) return;

    // --- 계산 로직 (기존 동일) ---
    PxTransform ParentGlobalPose = Parent->getGlobalPose();
    PxTransform ChildGlobalPose = Child->getGlobalPose();
    PxVec3 JointWorldPos = ChildGlobalPose.p;

    PxVec3 BoneDirection = (ChildGlobalPose.p - ParentGlobalPose.p);
    if (BoneDirection.magnitude() < 1e-4f) BoneDirection = PxVec3(1, 0, 0);

    PxQuat JointRotation = ComputeJointFrameRotation(BoneDirection);

    PxTransform LocalFrame1 = ParentGlobalPose.getInverse() * PxTransform(JointWorldPos, JointRotation);
    PxTransform LocalFrame2 = ChildGlobalPose.getInverse() * PxTransform(JointWorldPos, JointRotation);

    // --- [수정됨] 저장 로직 (복붙 실수 수정 완료) ---
    // PhysX Transform -> Engine Transform 변환 및 저장
    Frame1Loc = PhysXConvert::FromPx(LocalFrame1.p);
    Frame1Rot = PhysXConvert::FromPx(LocalFrame1.q).ToEulerZYXDeg(); // Quaternion -> Euler

    Frame2Loc = PhysXConvert::FromPx(LocalFrame2.p); // p (위치) 사용
    Frame2Rot = PhysXConvert::FromPx(LocalFrame2.q).ToEulerZYXDeg(); // q (회전) 사용

    // --- [핵심] 실제 생성은 아래 함수에게 위임 ---
    // 여기서 또 CreateJoint 하지 말고, 저장된 데이터로 만들어달라고 요청
    InitConstraintWithFrames(Body1, Body2, InOwnerComponent);
}

// ===== 2. 수동 Frame 지정 방식 (실제 생성기) =====
void FConstraintInstance::InitConstraintWithFrames(
    FBodyInstance* Body1,
    FBodyInstance* Body2,
    UPrimitiveComponent* InOwnerComponent)
{
    // 1. 유효성 검사 및 초기화
    if (!Body1 || !Body2 || !Body1->RigidActor || !Body2->RigidActor) return;
    TermConstraint();

    FPhysicsSystem* PhysSystem = GEngine.GetPhysicsSystem();
    if (!PhysSystem || !PhysSystem->GetPhysics()) return;

    OwnerComponent = InOwnerComponent; // Owner 등록

    // 2. 데이터 복원 (Engine -> PhysX)
    // 저장된 Loc/Rot를 다시 PxTransform으로 변환
    // Euler -> Quaternion 복원 시 순서(ZYX) 주의 (저장할 때랑 맞아야 함)
    PxTransform PxFrame1 = PxTransform(
        PhysXConvert::ToPx(Frame1Loc), 
        PhysXConvert::ToPx(FQuat::MakeFromEulerZYX(Frame1Rot)) // 엔진 내부 함수 사용 가정
    );

    PxTransform PxFrame2 = PxTransform(
        PhysXConvert::ToPx(Frame2Loc), 
        PhysXConvert::ToPx(FQuat::MakeFromEulerZYX(Frame2Rot))
    );

    // 3. 조인트 생성 (Local Frame 그대로 사용)
    PxD6Joint* D6Joint = PxD6JointCreate(
        *PhysSystem->GetPhysics(),
        Body1->RigidActor, PxFrame1,
        Body2->RigidActor, PxFrame2
    );

    if (!D6Joint)
    {
        UE_LOG("PxD6JointCreate failed");
        return;
    }

    // 4. 설정 적용
    ConfigureLinearLimits(D6Joint);
    ConfigureAngularLimits(D6Joint);
    D6Joint->setConstraintFlag(PxConstraintFlag::eCOLLISION_ENABLED, !bDisableCollision);
    
    // [수정됨] 누락되었던 userData 추가
    D6Joint->userData = this; 

    // 5. 최종 저장
    PxJoint = D6Joint;
}

void FConstraintInstance::TermConstraint()
{
    if (PxJoint)
    {
        PxJoint->release();
        PxJoint = nullptr;
    }
    OwnerComponent = nullptr;
}

void FConstraintInstance::ConfigureLinearLimits(PxD6Joint* Joint)
{
    if (!Joint) return;

    // 선형 모션 타입 변환
    auto ToPxMotion = [](ELinearConstraintMotion Motion) -> PxD6Motion::Enum
    {
        switch (Motion)
        {
            case ELinearConstraintMotion::Free:    return PxD6Motion::eFREE;
            case ELinearConstraintMotion::Limited: return PxD6Motion::eLIMITED;
            case ELinearConstraintMotion::Locked:
            default:                               return PxD6Motion::eLOCKED;
        }
    };

    // 각 축의 선형 모션 설정
    Joint->setMotion(PxD6Axis::eX, ToPxMotion(LinearXMotion));
    Joint->setMotion(PxD6Axis::eY, ToPxMotion(LinearYMotion));
    Joint->setMotion(PxD6Axis::eZ, ToPxMotion(LinearZMotion));

    // 선형 제한 설정 (Limited인 경우에만 의미 있음)
    if (LinearLimit > 0.0f)
    {
        bool bHasLimited = (LinearXMotion == ELinearConstraintMotion::Limited) ||
                          (LinearYMotion == ELinearConstraintMotion::Limited) ||
                          (LinearZMotion == ELinearConstraintMotion::Limited);

        if (bHasLimited)
        {
            FPhysicsSystem* PhysSystem = GEngine.GetPhysicsSystem();
            if (PhysSystem && PhysSystem->GetPhysics())
            {
                PxTolerancesScale Scale = PhysSystem->GetPhysics()->getTolerancesScale();
                PxJointLinearLimit LinearLimitPx(Scale, LinearLimit);
                Joint->setLinearLimit(LinearLimitPx);
            }
        }
    }
}

void FConstraintInstance::ConfigureAngularLimits(PxD6Joint* Joint)
{
    if (!Joint) return;

    // 각도 모션 타입 변환
    auto ToPxMotion = [](EAngularConstraintMotion Motion) -> PxD6Motion::Enum
    {
        switch (Motion)
        {
            case EAngularConstraintMotion::Free:    return PxD6Motion::eFREE;
            case EAngularConstraintMotion::Limited: return PxD6Motion::eLIMITED;
            case EAngularConstraintMotion::Locked:
            default:                                return PxD6Motion::eLOCKED;
        }
    };

    // 각 축의 각도 모션 설정
    Joint->setMotion(PxD6Axis::eTWIST,  ToPxMotion(AngularTwistMotion));
    Joint->setMotion(PxD6Axis::eSWING1, ToPxMotion(AngularSwing1Motion));
    Joint->setMotion(PxD6Axis::eSWING2, ToPxMotion(AngularSwing2Motion));

    // 각도 제한값을 라디안으로 변환
    float TwistRad  = PhysXConvert::DegreesToRadians(TwistLimitAngle);
    float Swing1Rad = PhysXConvert::DegreesToRadians(Swing1LimitAngle);
    float Swing2Rad = PhysXConvert::DegreesToRadians(Swing2LimitAngle);

    // contactDist: limit 경계에서 부드러운 접촉 처리를 위한 거리
    float ContactDist = 0.01f;

    // Twist 제한 설정
    if (AngularTwistMotion == EAngularConstraintMotion::Limited)
    {
        PxJointAngularLimitPair TwistLimit(-TwistRad, TwistRad, ContactDist);
        Joint->setTwistLimit(TwistLimit);
    }

    // Swing 제한 설정 (Cone limit)
    if (AngularSwing1Motion == EAngularConstraintMotion::Limited ||
        AngularSwing2Motion == EAngularConstraintMotion::Limited)
    {
        // Swing1, Swing2 중 하나라도 Limited면 Cone limit 설정
        float UsedSwing1 = (AngularSwing1Motion == EAngularConstraintMotion::Limited) ? Swing1Rad : PxPi;
        float UsedSwing2 = (AngularSwing2Motion == EAngularConstraintMotion::Limited) ? Swing2Rad : PxPi;

        PxJointLimitCone SwingLimit(UsedSwing1, UsedSwing2, ContactDist);
        Joint->setSwingLimit(SwingLimit);
    }
}
