#pragma once
#include "FConstraintInstance.generated.h"
#include "extensions/PxJoint.h"

/**
 * EAngularConstraintMotion
 *
 * 각 축의 회전 모션 타입
 */
UENUM()
enum class EAngularConstraintMotion : uint8
{
    Free,       // 자유 회전
    Limited,    // 제한된 회전
    Locked      // 회전 잠금
};

/**
 * ELinearConstraintMotion
 *
 * 각 축의 선형 모션 타입
 */
UENUM()
enum class ELinearConstraintMotion : uint8
{
    Free,       // 자유 이동
    Limited,    // 제한된 이동
    Locked      // 이동 잠금
};

/**
 * FConstraintInstance
 *
 * 실제 물리 조인트의 런타임 인스턴스 데이터.
 * PhysX PxJoint와 매핑됨.
 *
 * 두 강체(RigidBody) 사이의 물리적 연결을 정의.
 * 랙돌의 관절, 문의 경첩 등에 사용.
 */
USTRUCT()
struct FConstraintInstance
{
    GENERATED_REFLECTION_BODY()

    // --- 연결된 본 정보 ---

    UPROPERTY(EditAnywhere, Category="Constraint")
    FName JointName = "None";

    // 첫 번째 본 (부모)
    UPROPERTY(EditAnywhere, Category="Constraint")
    FName ConstraintBone1 = "None";

    // 두 번째 본 (자식)
    UPROPERTY(EditAnywhere, Category="Constraint")
    FName ConstraintBone2 = "None";

    // --- 선형 제한 (Linear Limits) ---

    UPROPERTY(EditAnywhere, Category="Linear")
    ELinearConstraintMotion LinearXMotion = ELinearConstraintMotion::Locked;

    UPROPERTY(EditAnywhere, Category="Linear")
    ELinearConstraintMotion LinearYMotion = ELinearConstraintMotion::Locked;

    UPROPERTY(EditAnywhere, Category="Linear")
    ELinearConstraintMotion LinearZMotion = ELinearConstraintMotion::Locked;

    // 선형 제한 거리
    UPROPERTY(EditAnywhere, Category="Linear")
    float LinearLimit = 0.0f;

    // --- 각도 제한 (Angular Limits) ---

    // Twist 축 (보통 X축)
    UPROPERTY(EditAnywhere, Category="Angular")
    EAngularConstraintMotion AngularTwistMotion = EAngularConstraintMotion::Limited;

    // Swing1 축 (보통 Y축)
    UPROPERTY(EditAnywhere, Category="Angular")
    EAngularConstraintMotion AngularSwing1Motion = EAngularConstraintMotion::Limited;

    // Swing2 축 (보통 Z축)
    UPROPERTY(EditAnywhere, Category="Angular")
    EAngularConstraintMotion AngularSwing2Motion = EAngularConstraintMotion::Limited;

    // 각도 제한 값 (degrees)
    UPROPERTY(EditAnywhere, Category="Angular")
    float TwistLimitAngle = 45.0f;

    UPROPERTY(EditAnywhere, Category="Angular")
    float Swing1LimitAngle = 45.0f;

    UPROPERTY(EditAnywhere, Category="Angular")
    float Swing2LimitAngle = 45.0f;

    // --- PhysX 런타임 데이터 ---

    // PhysX Joint 핸들 (런타임에 설정)
    physx::PxJoint* PxJoint = nullptr;

    // --- 생성자 ---
    FConstraintInstance() = default;
};
