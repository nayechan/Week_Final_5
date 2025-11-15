#pragma once
#include "SkinnedMeshComponent.h"
#include "USkeletalMeshComponent.generated.h"

UCLASS(DisplayName="스켈레탈 메시 컴포넌트", Description="스켈레탈 메시를 렌더링하는 컴포넌트입니다")
class USkeletalMeshComponent : public USkinnedMeshComponent
{
public:
    GENERATED_REFLECTION_BODY()
    
    USkeletalMeshComponent();
    ~USkeletalMeshComponent() override = default;

    void TickComponent(float DeltaTime) override;
    void SetSkeletalMesh(const FString& PathFileName) override;

    /**
     * 애니메이션 재생 시작
     * @param Animation 재생할 애니메이션 시퀀스
     * @param bLoop 루핑 여부 (기본값: true)
     */
    void PlayAnimation(class UAnimSequence* Animation, bool bLoop = true);

    /**
     * 애니메이션 재생 중지
     */
    void StopAnimation();

    /**
     * 애니메이션 재생 시간 설정
     * @param Time 설정할 시간 (초 단위)
     */
    void SetAnimationTime(float Time);

    /**
     * 현재 애니메이션 재생 중인지 확인
     */
    bool IsPlayingAnimation() const { return bIsPlaying; }

    /**
     * 현재 재생 중인 애니메이션 가져오기
     */
    class UAnimSequence* GetCurrentAnimation() const { return CurrentAnimation; }

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

    /**
     * @brief 주어진 시간에 애니메이션을 평가하여 CurrentLocalSpacePose 업데이트
     * @param Time 평가할 시간 (초 단위)
     */
    void EvaluateAnimation(float Time);

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

    /**
     * @brief 현재 재생 중인 애니메이션 시퀀스
     */
    class UAnimSequence* CurrentAnimation = nullptr;

    /**
     * @brief 현재 애니메이션 재생 시간 (초 단위)
     */
    float CurrentAnimationTime = 0.0f;

    /**
     * @brief 애니메이션 재생 중 여부
     */
    bool bIsPlaying = false;

    /**
     * @brief 애니메이션 루핑 여부
     */
    bool bLooping = false;

// FOR TEST!!!
private:
    float TestTime = 0;
    bool bIsInitialized = false;
    FTransform TestBoneBasePose;
};
