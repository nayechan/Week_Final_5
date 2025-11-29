#pragma once
#include "Source/Runtime/Core/Math/Vector.h"

/**
 * EPhysicsShapeType
 *
 * Physics Body의 충돌 형태 타입
 * UE의 EPhysicsShapeType을 단순화
 */
enum class EPhysicsShapeType : uint8
{
	Sphere,
	Capsule,
	Box,
};

/**
 * EConstraintType
 *
 * 물리 제약 조건 타입
 * UE의 11개 타입에서 핵심 2개로 단순화
 */
enum class EConstraintType : uint8
{
	BallAndSocket,  // 구형 조인트 (3축 회전)
	Hinge,          // 힌지 조인트 (1축 회전)
};

/**
 * IPhysicsAssetPreview
 *
 * PhysX 팀 연동을 위한 인터페이스
 * 에디터에서 시뮬레이션 미리보기 기능 제공
 * (현재는 더미 구현, 추후 PhysX 팀이 구현)
 */
class IPhysicsAssetPreview
{
public:
	virtual ~IPhysicsAssetPreview() = default;

	// 시뮬레이션 시작/중지
	virtual void StartSimulation() = 0;
	virtual void StopSimulation() = 0;
	virtual bool IsSimulating() const = 0;

	// 바디 위치 업데이트 (시뮬레이션 결과 적용)
	virtual void UpdateBodyTransforms(const TArray<FTransform>& Transforms) = 0;
};

// ────────────────────────────────────────────────────────────────
// 헬퍼 함수
// ────────────────────────────────────────────────────────────────

/** Shape 타입 이름 반환 */
inline const char* GetShapeTypeName(EPhysicsShapeType Type)
{
	switch (Type)
	{
	case EPhysicsShapeType::Sphere: return "Sphere";
	case EPhysicsShapeType::Capsule: return "Capsule";
	case EPhysicsShapeType::Box: return "Box";
	default: return "Unknown";
	}
}

/** Constraint 타입 이름 반환 */
inline const char* GetConstraintTypeName(EConstraintType Type)
{
	switch (Type)
	{
	case EConstraintType::BallAndSocket: return "Ball & Socket";
	case EConstraintType::Hinge: return "Hinge";
	default: return "Unknown";
	}
}
