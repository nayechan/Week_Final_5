#pragma once
#include "ResourceBase.h"
#include "SkeletalBodySetup.h"
#include "PhysicsConstraintTemplate.h"
#include "UPhysicsAsset.generated.h"

// 전방 선언
class USkeletalMesh;

/**
 * FRigidBodyIndexPair
 *
 * 두 바디의 인덱스 쌍. CollisionDisableTable의 키로 사용.
 */
struct FRigidBodyIndexPair
{
    int32 BodyIndex1;
    int32 BodyIndex2;

    FRigidBodyIndexPair(int32 InIndex1, int32 InIndex2)
        : BodyIndex1(FMath::Min(InIndex1, InIndex2))
        , BodyIndex2(FMath::Max(InIndex1, InIndex2))
    {
    }

    bool operator==(const FRigidBodyIndexPair& Other) const
    {
        return BodyIndex1 == Other.BodyIndex1 && BodyIndex2 == Other.BodyIndex2;
    }
};

// FRigidBodyIndexPair용 해시 함수
namespace std
{
    template<>
    struct hash<FRigidBodyIndexPair>
    {
        size_t operator()(const FRigidBodyIndexPair& Pair) const
        {
            return hash<int32>()(Pair.BodyIndex1) ^ (hash<int32>()(Pair.BodyIndex2) << 16);
        }
    };
}

/**
 * UPhysicsAsset
 *
 * SkeletalMesh의 물리 시뮬레이션을 위한 BodySetup 컨테이너.
 * 각 본(Bone)에 대응하는 충돌 Shape와 물리 속성을 정의.
 *
 * 주요 용도:
 * - 랙돌(Ragdoll) 물리
 * - 물리 기반 애니메이션
 * - 캐릭터 충돌 처리
 */
UCLASS(DisplayName="Physics Asset", Description="스켈레탈 메시용 물리 에셋")
class UPhysicsAsset : public UResourceBase
{
    GENERATED_REFLECTION_BODY()

public:
    // --- 리소스 로드/저장 ---
    void Load(const FString& InFilePath, ID3D11Device* InDevice = nullptr);
    void Save(const FString& InFilePath);

    // --- BodySetup 컬렉션 ---

    // 모든 SkeletalBodySetup 배열 (각 본에 대응)
    UPROPERTY(EditAnywhere, Category="Physics")
    TArray<USkeletalBodySetup*> SkeletalBodySetups;

    // --- Constraint 컬렉션 ---

    // 모든 PhysicsConstraintTemplate 배열 (본 사이의 조인트)
    UPROPERTY(EditAnywhere, Category="Constraints")
    TArray<UPhysicsConstraintTemplate*> ConstraintSetup;

    // --- 기본 물리 설정 ---

    // 기본 물리 재질 (개별 BodySetup에서 오버라이드 가능)
    UPROPERTY(EditAnywhere, Category="Physics")
    UPhysicalMaterial* DefaultPhysMaterial = nullptr;

    // 전체 시뮬레이션 활성화 여부
    //UPROPERTY(EditAnywhere, Category="Physics")
    //bool bEnablePhysicsSimulation = true;

    // --- 캐시/최적화 ---

    // BodySetup 이름으로 인덱스 빠른 검색용 맵
    TMap<FName, int32> BodySetupIndexMap;

    // 두 바디 간 충돌 비활성화 테이블
    std::unordered_map<FRigidBodyIndexPair, bool> CollisionDisableTable;

    // --- 생성자/소멸자 ---
    UPhysicsAsset();
    virtual ~UPhysicsAsset();

    // --- BodySetup 관리 ---

    // 본 이름으로 BodySetup 검색
    USkeletalBodySetup* FindBodySetup(const FName& BoneName) const;

    // 본 인덱스로 BodySetup 검색
    USkeletalBodySetup* GetBodySetup(int32 BodyIndex) const;

    // BodySetup 추가
    int32 AddBodySetup(USkeletalBodySetup* InBodySetup);

    // 본 이름으로 BodySetup 인덱스 검색
    int32 FindBodySetupIndex(const FName& BoneName) const;

    // BodySetup 개수
    int32 GetBodySetupCount() const { return SkeletalBodySetups.Num(); }

    // BodySetup 제거
    void RemoveBodySetup(int32 Index);
    void RemoveBodySetup(const FName& BoneName);

    // 모든 BodySetup 제거
    void ClearAllBodySetups();

    // --- Constraint 관리 ---

    // Constraint 추가
    int32 AddConstraint(UPhysicsConstraintTemplate* InConstraint);

    // Constraint 검색
    UPhysicsConstraintTemplate* FindConstraint(const FName& JointName) const;
    int32 FindConstraintIndex(const FName& JointName) const;
    int32 FindConstraintIndex(const FName& Bone1Name, const FName& Bone2Name) const;

    // Constraint 개수
    int32 GetConstraintCount() const { return ConstraintSetup.Num(); }

    // Constraint 제거
    void RemoveConstraint(int32 Index);

    // 모든 Constraint 제거
    void ClearAllConstraints();

    // --- 충돌 관리 ---

    // 두 바디 간 충돌 비활성화
    void DisableCollision(int32 BodyIndexA, int32 BodyIndexB);

    // 두 바디 간 충돌 활성화
    void EnableCollision(int32 BodyIndexA, int32 BodyIndexB);

    // 두 바디 간 충돌 활성화 여부 확인
    bool IsCollisionEnabled(int32 BodyIndexA, int32 BodyIndexB) const;

    // --- 캐시 관리 ---

    // BodySetupIndexMap 갱신
    void UpdateBodySetupIndexMap();

    // --- 유틸리티 ---

    // 유효성 검사
    bool IsValid() const { return SkeletalBodySetups.Num() > 0; }

    // 전체 Shape 개수 계산
    int32 GetTotalShapeCount() const;

    // --- 직렬화 ---
    virtual void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;
};
