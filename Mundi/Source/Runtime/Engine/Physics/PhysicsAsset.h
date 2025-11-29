#pragma once
#include "ResourceBase.h"
#include "PhysicsTypes.h"
#include "FBodySetup.h"
#include "FConstraintSetup.h"
#include "UPhysicsAsset.generated.h"

class USkeletalMesh;

/**
 * UPhysicsAsset
 *
 * 스켈레탈 메시의 물리 시뮬레이션 데이터를 저장하는 에셋
 * UE의 UPhysicsAsset을 단순화하여 구현
 *
 * 주요 기능:
 * - 본별 충돌 바디 설정 (FBodySetup)
 * - 바디 간 제약 조건 설정 (FConstraintSetup)
 * - 직렬화/역직렬화 지원
 */
UCLASS(DisplayName="피직스 에셋", Description="스켈레탈 메시의 물리 시뮬레이션 데이터입니다")
class UPhysicsAsset : public UResourceBase
{
public:
	GENERATED_REFLECTION_BODY()

	UPhysicsAsset() = default;
	virtual ~UPhysicsAsset() = default;

	// ────────────────────────────────────────────────────────────────
	// 속성
	// ────────────────────────────────────────────────────────────────

	/** 연결된 스켈레탈 메시 경로 */
	UPROPERTY(EditAnywhere, Category="PhysicsAsset")
	FString SkeletalMeshPath;

	/** 스켈레탈 본에 연결된 바디 설정 목록 */
	UPROPERTY(EditAnywhere, Category="PhysicsAsset")
	TArray<FBodySetup> BodySetups;

	/** 바디 간 제약 조건 목록 */
	UPROPERTY(EditAnywhere, Category="PhysicsAsset")
	TArray<FConstraintSetup> ConstraintSetups;

	// ────────────────────────────────────────────────────────────────
	// Body 관리 메서드
	// ────────────────────────────────────────────────────────────────

	/**
	 * 새로운 바디를 추가합니다.
	 * @param BoneName 연결할 본 이름
	 * @param BoneIndex 본 인덱스
	 * @return 추가된 바디의 인덱스
	 */
	int32 AddBody(const FName& BoneName, int32 BoneIndex);

	/**
	 * 바디를 제거합니다.
	 * @param BodyIndex 제거할 바디 인덱스
	 * @return 성공 여부
	 */
	bool RemoveBody(int32 BodyIndex);

	/**
	 * 본 이름으로 바디 인덱스를 찾습니다.
	 * @param BoneName 검색할 본 이름
	 * @return 바디 인덱스 (-1이면 없음)
	 */
	int32 FindBodyIndex(const FName& BoneName) const;

	/**
	 * 본 인덱스로 바디를 찾습니다.
	 * @param BoneIndex 검색할 본 인덱스
	 * @return 바디 인덱스 (-1이면 없음)
	 */
	int32 FindBodyIndexByBone(int32 BoneIndex) const;

	// ────────────────────────────────────────────────────────────────
	// Constraint 관리 메서드
	// ────────────────────────────────────────────────────────────────

	/**
	 * 새로운 제약 조건을 추가합니다.
	 * @param JointName 제약 조건 이름
	 * @param ParentBodyIndex 부모 바디 인덱스
	 * @param ChildBodyIndex 자식 바디 인덱스
	 * @return 추가된 제약 조건의 인덱스
	 */
	int32 AddConstraint(const FName& JointName, int32 ParentBodyIndex, int32 ChildBodyIndex);

	/**
	 * 제약 조건을 제거합니다.
	 * @param ConstraintIndex 제거할 제약 조건 인덱스
	 * @return 성공 여부
	 */
	bool RemoveConstraint(int32 ConstraintIndex);

	/**
	 * 두 바디 사이의 제약 조건을 찾습니다.
	 * @param ParentBodyIndex 부모 바디 인덱스
	 * @param ChildBodyIndex 자식 바디 인덱스
	 * @return 제약 조건 인덱스 (-1이면 없음)
	 */
	int32 FindConstraintIndex(int32 ParentBodyIndex, int32 ChildBodyIndex) const;

	// ────────────────────────────────────────────────────────────────
	// 유틸리티
	// ────────────────────────────────────────────────────────────────

	/**
	 * 모든 바디의 선택 상태를 해제합니다.
	 */
	void ClearBodySelection();

	/**
	 * 모든 제약 조건의 선택 상태를 해제합니다.
	 */
	void ClearConstraintSelection();

	/**
	 * 에셋 유효성을 검사합니다.
	 * @return 유효하면 true
	 */
	bool IsValid() const;

	// ────────────────────────────────────────────────────────────────
	// 직렬화
	// ────────────────────────────────────────────────────────────────

	/** 리소스 로드 (ResourceManager에서 호출) */
	void Load(const FString& InFilePath, ID3D11Device* InDevice);

	/** 직렬화 */
	virtual void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;

	/** 복제 */
	virtual void DuplicateSubObjects() override;

private:
	/** 바디 인덱스 캐시 (BoneName -> BodyIndex 매핑) */
	TMap<FName, int32> BodySetupIndexMap;

	/** 캐시 갱신 */
	void RebuildIndexMap();
};
