#pragma once
#include "SkinnedMeshComponent.h"
#include "USkeletalMeshComponent.generated.h"

class UAnimInstance;
class UAnimationAsset;
class UAnimSequence;
class UAnimStateMachineInstance;
class UAnimBlendSpaceInstance;

UCLASS(DisplayName="스켈레탈 메시 컴포넌트", Description="스켈레탈 메시를 렌더링하는 컴포넌트입니다")
class USkeletalMeshComponent : public USkinnedMeshComponent
{
public:
    GENERATED_REFLECTION_BODY()
    
    USkeletalMeshComponent();
    ~USkeletalMeshComponent() override = default;

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
    TArray<FTransform> RefPose;

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
};
