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

// ===== 동적 Frame 계산 방식 (권장) =====
void FConstraintInstance::InitConstraint(
    FBodyInstance* Body1,
    FBodyInstance* Body2,
    UPrimitiveComponent* InOwnerComponent)
{
    // 기존 Joint 해제
    TermConstraint();

    // 유효성 검사
    if (!Body1 || !Body2)
    {
        UE_LOG("[FConstraintInstance] InitConstraint failed: Body is null");
        return;
    }

    PxRigidDynamic* Parent = Body1->RigidActor ? Body1->RigidActor->is<PxRigidDynamic>() : nullptr;
    PxRigidDynamic* Child = Body2->RigidActor ? Body2->RigidActor->is<PxRigidDynamic>() : nullptr;

    if (!Parent || !Child)
    {
        UE_LOG("[FConstraintInstance] InitConstraint failed: RigidDynamic is null");
        return;
    }

    // PhysX Physics 획득
    FPhysicsSystem* PhysSystem = GEngine.GetPhysicsSystem();
    if (!PhysSystem || !PhysSystem->GetPhysics())
    {
        UE_LOG("[FConstraintInstance] InitConstraint failed: PhysicsSystem is null");
        return;
    }

    OwnerComponent = InOwnerComponent;

    // ===== Joint Frame 계산 (뼈 방향 기반) =====
    PxTransform ParentGlobalPose = Parent->getGlobalPose();
    PxTransform ChildGlobalPose = Child->getGlobalPose();

    // Joint 위치: 자식 Body의 위치 (관절점)
    PxVec3 JointWorldPos = ChildGlobalPose.p;

    // 본 방향: 부모→자식 (Twist 축으로 사용)
    PxVec3 BoneDirection = (ChildGlobalPose.p - ParentGlobalPose.p);
    if (BoneDirection.magnitude() < KINDA_SMALL_NUMBER)
    {
        BoneDirection = PxVec3(1, 0, 0);  // 거리가 너무 가까우면 기본값 사용
    }

    // Joint Frame 회전: 본 방향을 Twist 축(X)으로 정렬
    PxQuat JointRotation = ComputeJointFrameRotation(BoneDirection);

    // 부모/자식 로컬 좌표계에서의 Joint Frame
    PxTransform LocalFrame1 = ParentGlobalPose.getInverse() * PxTransform(JointWorldPos, JointRotation);
    PxTransform LocalFrame2 = ChildGlobalPose.getInverse() * PxTransform(JointWorldPos, JointRotation);

    // D6 Joint 생성
    PxD6Joint* D6Joint = PxD6JointCreate(
        *PhysSystem->GetPhysics(),
        Parent, LocalFrame1,
        Child, LocalFrame2
    );

    if (!D6Joint)
    {
        UE_LOG("[FConstraintInstance] InitConstraint failed: PxD6JointCreate returned null");
        return;
    }

    // 제한 설정 적용
    ConfigureLinearLimits(D6Joint);
    ConfigureAngularLimits(D6Joint);

    // 충돌 설정: bDisableCollision = true면 연결된 두 Body 간 충돌 비활성화
    D6Joint->setConstraintFlag(PxConstraintFlag::eCOLLISION_ENABLED, !bDisableCollision);

    // userData에 이 인스턴스 포인터 저장
    D6Joint->userData = this;

    // Joint 저장
    PxJoint = D6Joint;
}

// ===== 수동 Frame 지정 방식 (기존 호환) =====
void FConstraintInstance::InitConstraintWithFrames(
    FBodyInstance* Body1,
    FBodyInstance* Body2,
    const FTransform& Frame1,
    const FTransform& Frame2)
{
    // 유효성 검사
    if (!Body1 || !Body2)
    {
        UE_LOG("[FConstraintInstance] InitConstraintWithFrames failed: Body is null");
        return;
    }

    if (!Body1->RigidActor || !Body2->RigidActor)
    {
        UE_LOG("[FConstraintInstance] InitConstraintWithFrames failed: RigidActor is null");
        return;
    }

    // 기존 Joint 해제
    TermConstraint();

    // PhysX Physics 획득
    FPhysicsSystem* PhysSystem = GEngine.GetPhysicsSystem();
    if (!PhysSystem)
    {
        UE_LOG("[FConstraintInstance] InitConstraintWithFrames failed: PhysicsSystem is null");
        return;
    }

    PxPhysics* Physics = PhysSystem->GetPhysics();
    if (!Physics)
    {
        UE_LOG("[FConstraintInstance] InitConstraintWithFrames failed: PxPhysics is null");
        return;
    }

    // 프레임 변환 (프로젝트 좌표계 → PhysX 좌표계)
    PxTransform PxFrame1 = PhysXConvert::ToPx(Frame1);
    PxTransform PxFrame2 = PhysXConvert::ToPx(Frame2);

    // D6Joint 생성
    PxD6Joint* D6Joint = PxD6JointCreate(
        *Physics,
        Body1->RigidActor, PxFrame1,
        Body2->RigidActor, PxFrame2
    );

    if (!D6Joint)
    {
        UE_LOG("[FConstraintInstance] InitConstraintWithFrames failed: PxD6JointCreate returned null");
        return;
    }

    // 제한 설정 적용
    ConfigureLinearLimits(D6Joint);
    ConfigureAngularLimits(D6Joint);

    // 충돌 설정
    D6Joint->setConstraintFlag(PxConstraintFlag::eCOLLISION_ENABLED, !bDisableCollision);

    // Joint 저장
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
