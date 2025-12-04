#pragma once

class UPhysicsAsset;
class USkeletalMesh;
class USkeletalMeshComponent;

/**
 * Physics Asset 자동 생성 옵션
 */
struct FPhysicsAssetCreateOptions
{
	// Shape 타입 (0: Sphere, 1: Box, 2: Capsule)
	int32 ShapeType = 2;

	// 바디 크기 비율 (0.5 ~ 1.0)
	float BodySizeScale = 1.0f;

	// true면 모든 본에 바디 생성, false면 작은 본 병합
	bool bBodyForAll = true;

	// 최소 본 크기 (미터 단위, 이 크기 미만 본은 부모에 병합)
	float MinBoneSize = 0.25f;

	// 컨스트레인트 자동 생성 여부
	bool bCreateConstraints = true;

	// 각 컨스트레인트 모드 (0: Free, 1: Limited, 2: Locked)
	int32 AngularMode = 1;

	// Swing 제한 각도 (도)
	float SwingLimit = 45.0f;

	// Twist 제한 각도 (도)
	float TwistLimit = 45.0f;
};

/**
 * Physics Asset 유틸리티 함수 모음
 * 바디/컨스트레인트 자동 생성 관련 기능을 제공
 */
namespace FPhysicsAssetUtils
{
	/**
	 * PCA 기반 바디 자동 생성
	 * @param PhysAsset 대상 Physics Asset (기존 바디/컨스트레인트 모두 삭제됨)
	 * @param Mesh 버텍스/스켈레톤 정보를 얻을 SkeletalMesh
	 * @param MeshComp 본 월드 트랜스폼을 얻을 컴포넌트 (폴백용)
	 * @param Options 생성 옵션
	 * @return 생성된 바디 개수
	 */
	int32 CreateAllBodies(
		UPhysicsAsset* PhysAsset,
		USkeletalMesh* Mesh,
		USkeletalMeshComponent* MeshComp,
		const FPhysicsAssetCreateOptions& Options
	);

	/**
	 * 부모-자식 관계 기반 컨스트레인트 자동 생성
	 * @param PhysAsset 대상 Physics Asset
	 * @param Mesh 스켈레톤 정보를 얻을 SkeletalMesh
	 * @param Options 생성 옵션
	 * @return 생성된 컨스트레인트 개수
	 */
	int32 CreateConstraintsForBodies(
		UPhysicsAsset* PhysAsset,
		USkeletalMesh* Mesh,
		const FPhysicsAssetCreateOptions& Options
	);

	/**
	 * 모든 바디와 컨스트레인트 삭제
	 * @param PhysAsset 대상 Physics Asset
	 */
	void RemoveAllBodiesAndConstraints(UPhysicsAsset* PhysAsset);

	/**
	 * 모든 바디만 삭제 (컨스트레인트는 유지)
	 * @param PhysAsset 대상 Physics Asset
	 */
	void RemoveAllBodies(UPhysicsAsset* PhysAsset);

	/**
	 * 모든 컨스트레인트만 삭제 (바디는 유지)
	 * @param PhysAsset 대상 Physics Asset
	 */
	void RemoveAllConstraints(UPhysicsAsset* PhysAsset);

	
	// PhysX D6 Joint의 Twist 축(X축)을 뼈의 길이 방향으로 정렬하는 회전 계산
	physx::PxQuat ComputeJointFrameRotation(const physx::PxVec3& Direction);
	
	// ===== Frame 계산 함수 (PhysX 기반) =====
	void CalculateConstraintFramesFromPhysX(FConstraintInstance& Instance, const physx::PxTransform& ParentGlobalPose, const physx::PxTransform& ChildGlobalPose);
}
