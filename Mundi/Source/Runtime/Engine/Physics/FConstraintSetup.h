#pragma once

#include "PhysicsTypes.h"
#include "FConstraintSetup.generated.h"

/**
 * FConstraintSetup
 *
 * 두 바디 간의 물리 제약 조건 설정
 * UPhysicsConstraintTemplate에서 필수 속성만 추출
 */
USTRUCT(DisplayName="제약 조건 설정", Description="두 바디 간의 물리 제약 조건 설정")
struct FConstraintSetup
{
	GENERATED_REFLECTION_BODY()

public:
	// ────────────────────────────────────────────────
	// 기본 정보
	// ────────────────────────────────────────────────
	UPROPERTY(EditAnywhere, Category="Joint", Tooltip="제약 조건 이름")
	FName JointName;

	// ────────────────────────────────────────────────
	// 연결된 바디
	// ────────────────────────────────────────────────
	UPROPERTY(EditAnywhere, Category="Joint", Tooltip="부모 바디 인덱스")
	int32 ParentBodyIndex = -1;

	UPROPERTY(EditAnywhere, Category="Joint", Tooltip="자식 바디 인덱스")
	int32 ChildBodyIndex = -1;

	// ────────────────────────────────────────────────
	// 제약 조건 타입
	// ────────────────────────────────────────────────
	UPROPERTY(EditAnywhere, Category="Joint", Tooltip="제약 조건 타입")
	EConstraintType ConstraintType = EConstraintType::BallAndSocket;

	// ────────────────────────────────────────────────
	// 각도 제한 (degrees)
	// BallAndSocket: Swing1, Swing2, TwistMin/Max 모두 사용
	// Hinge: Swing1만 사용 (회전축 제한)
	// ────────────────────────────────────────────────
	UPROPERTY(EditAnywhere, Category="Limits", Tooltip="Y축 회전 제한 (degrees)")
	float Swing1Limit = 45.0f;

	UPROPERTY(EditAnywhere, Category="Limits", Tooltip="Z축 회전 제한 (degrees)")
	float Swing2Limit = 45.0f;

	UPROPERTY(EditAnywhere, Category="Limits", Tooltip="X축 회전 최소 (degrees)")
	float TwistLimitMin = -45.0f;

	UPROPERTY(EditAnywhere, Category="Limits", Tooltip="X축 회전 최대 (degrees)")
	float TwistLimitMax = 45.0f;

	// ────────────────────────────────────────────────
	// 제약 조건 강도 (PhysX 연동 시 활용)
	// ────────────────────────────────────────────────
	UPROPERTY(EditAnywhere, Category="Drive", Tooltip="스프링 강성")
	float Stiffness = 0.0f;

	UPROPERTY(EditAnywhere, Category="Drive", Tooltip="감쇠 계수")
	float Damping = 0.0f;

	// ────────────────────────────────────────────────
	// 에디터 상태 (직렬화 제외)
	// ────────────────────────────────────────────────
	bool bSelected = false;

	// ────────────────────────────────────────────────
	// 생성자
	// ────────────────────────────────────────────────
	FConstraintSetup() = default;

	FConstraintSetup(const FName& InJointName, int32 InParentIndex, int32 InChildIndex)
		: JointName(InJointName)
		, ParentBodyIndex(InParentIndex)
		, ChildBodyIndex(InChildIndex)
	{}
};
