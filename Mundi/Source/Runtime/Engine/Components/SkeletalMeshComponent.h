#pragma once
#include "SkinnedMeshComponent.h"
#include "USkeletalMeshComponent.generated.h"

class UAnimInstance;
class UAnimationAsset;
class UAnimSequence;
class UAnimStateMachineInstance;
class UAnimBlendSpaceInstance;

// Ragdoll 관련 전방 선언
struct FBodyInstance;
struct FConstraintInstance;
class UPhysicsAsset;
class FPhysicsScene;

/**
 * ERagdollState
 * Ragdoll 시뮬레이션 상태
 */
UENUM()
enum class EPhysicsBlendState : uint8
{
    Disabled,       // 애니메이션만 사용
    BlendingIn,     // 애니메이션 → Ragdoll 전환 중
    Active,         // Ragdoll 시뮬레이션 중
    BlendingOut     // Ragdoll → 애니메이션 전환 중
};

UCLASS(DisplayName="스켈레탈 메시 컴포넌트", Description="스켈레탈 메시를 렌더링하는 컴포넌트입니다")
class USkeletalMeshComponent : public USkinnedMeshComponent
{
public:
    GENERATED_REFLECTION_BODY()
    
    USkeletalMeshComponent();
    ~USkeletalMeshComponent() override = default;

    void BeginPlay() override;
    void EndPlay() override;
    void TickComponent(float DeltaTime) override;
    void PostPhysicsTick(float DeltaTime) override;
    void SetSkeletalMesh(const FString& PathFileName) override;
    void DuplicateSubObjects() override;

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

private:
    // Animation state
    UAnimInstance* AnimInstance = nullptr;
    bool bUseAnimation = true;

// ===========================
// Physics Asset & Simulation Section
// ===========================
public:
    void SyncByPhysics(const FTransform& NewTransform) override;
    
    /** 물리 엔진용 바디와 제약조건을 생성하고 초기화 */
    void InitPhysics();
    /** 물리 리소스를 해제 */
    void TermPhysics();
    
    // PhysicsAsset을 편집하기 위한 프로퍼티
    UPROPERTY(EditAnywhere, Category="Physics")
    UPhysicsAsset* PhysicsAsset = nullptr;

    // 프로퍼티 변경 시 메시에 반영
    void OnPropertyChanged(const FProperty& Property) override;

    /**
     * Ragdoll 시뮬레이션 시작
     * @param bSimulate - true면 PhysicsAsset Bodies Simulate, false면 KINEMATIC
     * @param bBlend - true면 블렌딩 전환, false면 즉시 전환
     */
    void SetSimulatePhysics(bool bSimulate, bool bBlend = false);

    // --- 상태 조회 ---
    EPhysicsBlendState GetRagdollState() const { return PhysicsBlendState; }
    bool IsPhysicsLogicActive() const { return PhysicsBlendState != EPhysicsBlendState::Disabled; }

    // --- 힘/임펄스 적용 ---
    /** 특정 본의 물리 바디에 임펄스 적용 */
    void AddImpulse(const FVector& Impulse, FName BoneName = FName(), bool bVelChange = false);
    /** 특정 본의 물리 바디에 힘 적용 */
    void AddForce(const FVector& Force, FName BoneName = FName(), bool bAccelChange = false);

protected:
    /** Ragdoll 상태 업데이트 (TickComponent에서 호출) */
    void UpdatePhysicsBlend(float DeltaTime);

    /** [Anim -> Physics] 애니메이션 포즈를 Kinematic Body로 전송 */
    void UpdateKinematicBonesToAnim();

    /**
     * [Physics -> Anim] 물리 시뮬레이션 결과를 뼈 트랜스폼에 반영
     */
    void BlendPhysicsBones();

    /**
     * 애니메이션/Ragdoll 포즈 블렌딩
     */
    void BlendPhysicsPoses(float Alpha);

    /**
     * PhysicsAsset의 Constraint들을 생성
     */
    void CreatePhysicsConstraints();

private:
    // --- Physics 상태 ---
    EPhysicsBlendState PhysicsBlendState = EPhysicsBlendState::Disabled;
    float PhysicsBlendWeight = 0.0f;
    float PhysicsBlendDuration = 0.3f;

    // --- Ragdoll Bodies/Constraints ---
    TArray<FBodyInstance*> Bodies;
    TArray<FConstraintInstance*> Constraints;
    TMap<FName, int32> BodyIndexMap;
    TArray<FBodyInstance*> BoneIndexToBodyCache; // 크기는 전체 뼈 개수와 동일, Body가 없으면 nullptr

    // --- 블렌딩용 포즈 저장 ---
    TArray<FTransform> PhysicsBlendPoseSaved;  // Ragdoll 전환 전 애니메이션 포즈
    TArray<FTransform> PhysicsResultPose;      // 피직스에서 가져온 포즈
};
