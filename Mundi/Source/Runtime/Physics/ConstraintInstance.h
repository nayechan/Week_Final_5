#pragma once
#include "extensions/PxJoint.h"
#include "extensions/PxD6Joint.h"
#include "FConstraintInstance.generated.h"

// 전방 선언
class UPrimitiveComponent;

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

    // --- 충돌 설정 ---

    // Joint로 연결된 두 Body 간 충돌 비활성화 여부
    // true = 인접 본 간 충돌 무시 (래그돌 기본값)
    UPROPERTY(EditAnywhere, Category="Collision")
    bool bDisableCollision = true;

    // --- PhysX 런타임 데이터 ---

    // PhysX Joint 핸들 (런타임에 설정)
    physx::PxJoint* PxJoint = nullptr;

    // --- 생성자 ---
    FConstraintInstance() = default;

    // --- 런타임 멤버 ---

    UPrimitiveComponent* OwnerComponent = nullptr;

    // --- Joint 생성/해제 ---

    /**
     * PxD6Joint 생성 및 초기화 (동적 Frame 계산 - 권장)
     * Body의 현재 위치를 기반으로 Joint Frame을 자동 계산
     * Twist 축(X축)을 부모→자식 뼈 방향으로 정렬
     *
     * @param Body1 - 첫 번째 바디 (부모)
     * @param Body2 - 두 번째 바디 (자식)
     * @param InOwnerComponent - 소유자 컴포넌트 (옵션)
     */
    void InitConstraint(
        struct FBodyInstance* Body1,
        struct FBodyInstance* Body2,
        UPrimitiveComponent* InOwnerComponent = nullptr
    );

    /**
     * PxD6Joint 생성 및 초기화 (수동 Frame 지정)
     * @param Body1 - 첫 번째 바디 (부모)
     * @param Body2 - 두 번째 바디 (자식)
     * @param Frame1 - Body1 로컬 기준 조인트 프레임
     * @param Frame2 - Body2 로컬 기준 조인트 프레임
     */
    void InitConstraintWithFrames(
        struct FBodyInstance* Body1,
        struct FBodyInstance* Body2,
        const FTransform& Frame1,
        const FTransform& Frame2
    );

    /**
     * PxJoint 해제
     */
    void TermConstraint();

    /**
     * Joint 유효성 검사
     */
    bool IsValidConstraint() const { return PxJoint != nullptr; }

private:
    /**
     * 선형 제한 설정 적용
     */
    void ConfigureLinearLimits(physx::PxD6Joint* Joint);

    /**
     * 각도 제한 설정 적용
     */
    void ConfigureAngularLimits(physx::PxD6Joint* Joint);
};
