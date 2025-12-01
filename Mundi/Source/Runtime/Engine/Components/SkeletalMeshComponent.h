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
enum class ERagdollState : uint8
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
    void TickComponent(float DeltaTime) override;
    void SetSkeletalMesh(const FString& PathFileName) override;

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

// ===========================
// Physics Asset Section
// ===========================
public:
    // 메시의 PhysicsAsset을 편집하기 위한 프로퍼티
    // 변경 시 메시의 PhysicsAsset도 함께 변경됨
    UPROPERTY(EditAnywhere, Category="Physics")
    UPhysicsAsset* PhysicsAsset = nullptr;

    // PhysicsAsset 동기화 (메시 변경 시 호출)
    void SyncPhysicsAssetFromMesh();

    // 프로퍼티 변경 시 메시에 반영
    virtual void OnPropertyChanged(const FProperty& Property) override;

// ===========================
// Ragdoll Physics Section
// ===========================
public:
    // --- Ragdoll 생명주기 ---

    /**
     * Ragdoll Bodies 및 Constraints 초기화
     * PhysicsAsset이 설정되어 있어야 함
     */
    void InitRagdoll();

    /**
     * Ragdoll Bodies 및 Constraints 해제
     */
    void TermRagdoll();

    // --- Ragdoll 제어 ---

    /**
     * Ragdoll 시뮬레이션 시작
     * @param bBlend - true면 블렌딩 전환, false면 즉시 전환
     */
    void StartRagdoll(bool bBlend = true);

    /**
     * Ragdoll 시뮬레이션 정지 (Body는 Kinematic으로 유지, 재사용)
     * @param bBlend - true면 블렌딩 전환, false면 즉시 전환
     */
    void StopRagdoll(bool bBlend = true);

    // --- Ragdoll 상태 조회 ---

    ERagdollState GetRagdollState() const { return RagdollState; }
    bool IsRagdollActive() const { return RagdollState == ERagdollState::Active; }
    bool IsRagdollSimulating() const
    {
        return RagdollState == ERagdollState::Active ||
               RagdollState == ERagdollState::BlendingIn;
    }

    // --- Ragdoll 힘/임펄스 적용 ---

    /**
     * 특정 본의 물리 바디에 임펄스 적용
     */
    void AddImpulseToBody(const FName& BoneName, const FVector& Impulse);

    /**
     * 특정 본의 물리 바디에 힘 적용
     */
    void AddForceToBody(const FName& BoneName, const FVector& Force);

    // --- Ragdoll 테스트/디버그 ---

    /**
     * PhysicsAsset이 없으면 자동으로 생성
     * 모든 본에 대해 캡슐 Shape와 부모-자식 Constraint를 생성
     */
    void CreateDefaultPhysicsAsset();

    /**
     * Ragdoll 테스트 모드 활성화
     * 특정 키(기본: R) 입력 시 Ragdoll 토글
     */
    void EnableRagdollTestMode(bool bEnable) { bRagdollTestMode = bEnable; }
    bool IsRagdollTestModeEnabled() const { return bRagdollTestMode; }

private:
    bool bRagdollTestMode = true;

protected:
    // --- Ragdoll 내부 메서드 ---

    /**
     * Ragdoll 상태 업데이트 (TickComponent에서 호출)
     */
    void UpdateRagdollState(float DeltaTime);

    /**
     * 애니메이션 포즈를 물리 Body에 텔레포트
     */
    void SyncRagdollToAnimation();

    /**
     * 물리 Body 위치를 본 트랜스폼으로 동기화
     */
    void SyncAnimationToRagdoll();

    /**
     * 애니메이션/Ragdoll 포즈 블렌딩
     */
    void BlendPoses(float Alpha);

    /**
     * PhysicsAsset의 Constraint들을 생성
     */
    void CreateRagdollConstraints();

private:
    // --- Ragdoll 상태 ---
    ERagdollState RagdollState = ERagdollState::Disabled;
    float RagdollBlendAlpha = 0.0f;
    float RagdollBlendDuration = 0.3f;

    // --- Ragdoll Bodies/Constraints ---
    TArray<FBodyInstance*> RagdollBodies;
    TArray<FConstraintInstance*> RagdollConstraints;
    TMap<FName, int32> BoneNameToBodyIndex;

    // --- 블렌딩용 포즈 저장 ---
    TArray<FTransform> PreRagdollPose;  // Ragdoll 전환 전 애니메이션 포즈
    TArray<FTransform> RagdollPose;     // 피직스에서 가져온 포즈
};
