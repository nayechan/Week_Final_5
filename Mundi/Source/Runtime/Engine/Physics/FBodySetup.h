#pragma once

#include "Source/Runtime/Core/Math/Vector.h"
#include "PhysicsTypes.h"
#include "FBodySetup.generated.h"

/**
 * FBodySetup
 *
 * 스켈레톤 본에 연결된 물리 바디 설정
 * USkeletalBodySetup에서 필수 속성만 추출
 */
USTRUCT(DisplayName="바디 설정", Description="스켈레톤 본에 연결된 물리 바디 설정")
struct FBodySetup
{
	GENERATED_REFLECTION_BODY()

public:
	// ────────────────────────────────────────────────
	// 본 연결 정보
	// ────────────────────────────────────────────────
	UPROPERTY(EditAnywhere, Category="Bone", Tooltip="연결된 본 이름")
	FName BoneName;

	UPROPERTY(EditAnywhere, Category="Bone", Tooltip="연결된 본 인덱스")
	int32 BoneIndex = -1;

	// ────────────────────────────────────────────────
	// Shape 설정
	// ────────────────────────────────────────────────
	UPROPERTY(EditAnywhere, Category="Shape", Tooltip="충돌 형태 타입")
	EPhysicsShapeType ShapeType = EPhysicsShapeType::Capsule;

	UPROPERTY(EditAnywhere, Category="Shape", Tooltip="본 기준 로컬 트랜스폼")
	FTransform LocalTransform;

	// Shape 파라미터 (타입에 따라 사용되는 값이 다름)
	// Sphere: Radius만 사용
	// Capsule: Radius, HalfHeight 사용
	// Box: Extent 사용
	UPROPERTY(EditAnywhere, Category="Shape", Tooltip="반지름 (Sphere, Capsule)")
	float Radius = 10.0f;

	UPROPERTY(EditAnywhere, Category="Shape", Tooltip="캡슐 반높이 (Capsule)")
	float HalfHeight = 20.0f;

	UPROPERTY(EditAnywhere, Category="Shape", Tooltip="박스 크기 (Box)")
	FVector Extent = FVector(10.0f, 10.0f, 10.0f);

	// ────────────────────────────────────────────────
	// 물리 속성
	// ────────────────────────────────────────────────
	UPROPERTY(EditAnywhere, Category="Physics", Tooltip="질량 (kg)")
	float Mass = 1.0f;

	UPROPERTY(EditAnywhere, Category="Physics", Tooltip="선형 감쇠")
	float LinearDamping = 0.01f;

	UPROPERTY(EditAnywhere, Category="Physics", Tooltip="각 감쇠")
	float AngularDamping = 0.0f;

	UPROPERTY(EditAnywhere, Category="Physics", Tooltip="중력 적용 여부")
	bool bEnableGravity = true;

	// ────────────────────────────────────────────────
	// 에디터 상태 (직렬화 제외)
	// ────────────────────────────────────────────────
	bool bSelected = false;

	// ────────────────────────────────────────────────
	// 생성자
	// ────────────────────────────────────────────────
	FBodySetup() = default;

	FBodySetup(const FName& InBoneName, int32 InBoneIndex)
		: BoneName(InBoneName)
		, BoneIndex(InBoneIndex)
	{}
};
