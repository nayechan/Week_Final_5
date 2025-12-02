#pragma once
#include "SkinnedMeshComponent.h"
#include "PhysicsAsset.h"
#include "USkeletalMeshComponent.generated.h"

class UAnimInstance;
class UAnimationAsset;
class UAnimSequence;
class UAnimStateMachineInstance;
class UAnimBlendSpaceInstance;
struct FBodyInstance;
struct FConstraintInstance;
class FPhysScene;

namespace physx { class PxAggregate; }

UCLASS(DisplayName="스켈레탈 메시 컴포넌트", Description="스켈레탈 메시를 렌더링하는 컴포넌트입니다")
class USkeletalMeshComponent : public USkinnedMeshComponent
{
public:
    GENERATED_REFLECTION_BODY()
    
    USkeletalMeshComponent();
    ~USkeletalMeshComponent() override = default;

    void TickComponent(float DeltaTime) override;
    void SetSkeletalMesh(const FString& PathFileName) override;
    void DuplicateSubObjects() override;

    // PhysicsAsset 디버그 시각화
    void RenderDebugVolume(URenderer* Renderer) const override;

    // 물리 상태 관리 (UPrimitiveComponent 오버라이드)
    void OnCreatePhysicsState() override;
    void OnDestroyPhysicsState() override;

    // 프로퍼티 변경 시 물리 상태 재생성
    void OnPropertyChanged(const FProperty& Prop) override;

    // 트랜스폼 변경 시 물리 바디 동기화
    void OnUpdateTransform(EUpdateTransformFlags UpdateTransformFlags, ETeleportType Teleport = ETeleportType::None) override;
    // Animation Integration
public:
    void SetAnimInstance(class UAnimInstance* InInstance);
    UAnimInstance* GetAnimInstance() const { return AnimInstance; }

    // Convenience single-clip controls (optional)
    void PlayAnimation(class UAnimationAsset* Asset, bool bLooping = true, float InPlayRate = 1.f);
    void StopAnimation();
    void SetAnimationPosition(float InSeconds);
    float GetAnimationPosition();
    bool IsPlayingAnimation() const;

    //==== Minimal Lua-friendly helper to switch to a state machine anim instance ====
    UFUNCTION(LuaBind, DisplayName="UseStateMachine")
    void UseStateMachine();

    UFUNCTION(LuaBind, DisplayName="GetOrCreateStateMachine")
    UAnimStateMachineInstance* GetOrCreateStateMachine();

    //==== Minimal Lua-friendly helper to switch to a blend space 2D anim instance ====
    UFUNCTION(LuaBind, DisplayName="UseBlendSpace2D")
    void UseBlendSpace2D();

    UFUNCTION(LuaBind, DisplayName="GetOrCreateBlendSpace2D")
    UAnimBlendSpaceInstance* GetOrCreateBlendSpace2D();

// Editor Section
public:
    /**
     * @brief 특정 뼈의 부모 기준 로컬 트랜스폼을 설정
     * @param BoneIndex 수정할 뼈의 인덱스
     * @param NewLocalTransform 새로운 부모 기준 로컬 FTransform
     */
    void SetBoneLocalTransform(int32 BoneIndex, const FTransform& NewLocalTransform);

    void SetBoneWorldTransform(int32 BoneIndex, const FTransform& NewWorldTransform);
    
    /**
     * @brief 특정 뼈의 현재 로컬 트랜스폼을 반환
     */
    FTransform GetBoneLocalTransform(int32 BoneIndex) const;
    
    /**
     * @brief 기즈모를 렌더링하기 위해 특정 뼈의 월드 트랜스폼을 계산
     */
    FTransform GetBoneWorldTransform(int32 BoneIndex);

    /**
     * @brief 애니메이션 포즈 위에 추가적인(additive) 트랜스폼을 적용 (by 사용자 조작)
     * @param AdditiveTransforms BoneIndex -> Additive FTransform 맵
     */
    void ApplyAdditiveTransforms(const TMap<int32, FTransform>& AdditiveTransforms);

    /**
     * @brief CurrentLocalSpacePose를 RefPose로 리셋하고 포즈를 재계산
     * 애니메이션이 없는 뷰어에서 additive transform 적용 전에 호출
     */
    void ResetToRefPose();

    /**
     * @brief CurrentLocalSpacePose를 BaseAnimationPose로 리셋
     * 애니메이션이 있는 뷰어에서 additive transform 적용 전에 호출
     */
    void ResetToAnimationPose();

    TArray<FTransform> RefPose;
    TArray<FTransform> BaseAnimationPose;

    // Notify
    void TriggerAnimNotify(const FAnimNotifyEvent& NotifyEvent);

protected:
    /**
     * @brief CurrentLocalSpacePose의 변경사항을 ComponentSpace -> FinalMatrices 계산까지 모두 수행
     */
    void ForceRecomputePose();

    /**
     * @brief CurrentLocalSpacePose를 기반으로 CurrentComponentSpacePose 채우기
     */
    void UpdateComponentSpaceTransforms();

    /**
     * @brief CurrentComponentSpacePose를 기반으로 TempFinalSkinningMatrices 채우기
     */
    void UpdateFinalSkinningMatrices();

protected:
    /**
     * @brief 각 뼈의 부모 기준 로컬 트랜스폼
     */
    TArray<FTransform> CurrentLocalSpacePose;

    /**
     * @brief LocalSpacePose로부터 계산된 컴포넌트 기준 트랜스폼
     */
    TArray<FTransform> CurrentComponentSpacePose;

    /**
     * @brief 부모에게 보낼 최종 스키닝 행렬 (임시 계산용)
     */
    TArray<FMatrix> TempFinalSkinningMatrices;

// FOR TEST!!!
private:
    float TestTime = 0;
    bool bIsInitialized = false;
    FTransform TestBoneBasePose;

    // Animation state
    UAnimInstance* AnimInstance = nullptr;
    bool bUseAnimation = true;

// ============================================================================
// Ragdoll Physics Section
// ============================================================================
public:
    /**
     * @brief 물리 에셋 설정 (랙돌 데이터)
     * @param InPhysicsAsset 적용할 물리 에셋
     */
    void SetPhysicsAsset(UPhysicsAsset* InPhysicsAsset);

    /**
     * @brief 현재 연결된 물리 에셋 반환
     */
    UPhysicsAsset* GetPhysicsAsset() const { return PhysicsAsset; }

    /**
     * @brief 랙돌 시뮬레이션 활성화/비활성화
     * @param bEnable true: 랙돌 활성화 (물리 시뮬레이션), false: 애니메이션 모드
     */
    virtual void SetSimulatePhysics(bool bEnable) override;

    /**
     * @brief 랙돌 시뮬레이션 중인지 확인
     */
    bool IsSimulatingPhysics() const { return bSimulatePhysics; }

    /**
     * @brief 랙돌 초기화 (PhysicsAsset 기반으로 바디/제약조건 생성)
     * @param InPhysScene 물리 씬
     */
    void InitRagdoll(FPhysScene* InPhysScene);

    /**
     * @brief 랙돌 해제 (모든 바디/제약조건 제거)
     */
    void TermRagdoll();

    /**
     * @brief 특정 본의 바디 인스턴스 반환
     * @param BoneIndex 본 인덱스
     * @return 바디 인스턴스 (없으면 nullptr)
     */
    FBodyInstance* GetBodyInstance(int32 BoneIndex) const;

    /**
     * @brief 랙돌에 충격 적용
     * @param Impulse 충격 벡터
     * @param BoneName 적용할 본 이름 (빈 문자열이면 모든 바디에 적용)
     */
    void AddImpulse(const FVector& Impulse, const FName& BoneName = FName());

    /**
     * @brief 랙돌에 힘 적용
     * @param Force 힘 벡터
     * @param BoneName 적용할 본 이름 (빈 문자열이면 모든 바디에 적용)
     */
    void AddForce(const FVector& Force, const FName& BoneName = FName());

    // ============================================================================
    // 테스트용 하드코딩 랙돌 (PhysicsAsset 없이 테스트)
    // ============================================================================

    /**
     * @brief 테스트용 하드코딩 랙돌 생성
     * @param InPhysScene 물리 씬
     *
     * PhysicsAsset 없이 코드에서 직접 바디/조인트를 생성하여 테스트
     * 스켈레탈 메시의 본 구조를 기반으로 자동 생성
     */
    void InitTestRagdoll(FPhysScene* InPhysScene);

protected:
    /**
     * @brief 물리 시뮬레이션 결과를 본 트랜스폼에 동기화
     */
    void SyncBonesFromPhysics();

    /**
     * @brief 현재 본 트랜스폼을 물리 바디에 동기화 (랙돌 시작 시)
     */
    void SyncPhysicsFromBones();

    /**
     * @brief 초기 포즈에서 겹치는 바디 쌍을 검출하여 충돌 무시 설정
     */
    void SetupInitialOverlapFilters();

public:
    
    /** 연결된 물리 에셋 */
    UPROPERTY(EditAnywhere, Category="[피직스 에셋]")
    UPhysicsAsset* PhysicsAsset = nullptr;

    /** 본별 바디 인스턴스 (BoneIndex -> FBodyInstance) */
    TArray<FBodyInstance*> Bodies;

    /** 제약 조건 인스턴스 */
    TArray<FConstraintInstance*> Constraints;

    /** 랙돌이 초기화되었는지 여부 */
    bool bRagdollInitialized = false;

    /** 물리 씬 참조 */
    FPhysScene* PhysScene = nullptr;

    /** PhysX Aggregate (랙돌 내 자체 충돌 비활성화용) */
    physx::PxAggregate* Aggregate = nullptr;
};
