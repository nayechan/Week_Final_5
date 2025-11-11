#include "pch.h"
#include "SkeletalMeshComponent.h"

USkeletalMeshComponent::USkeletalMeshComponent()
{
    // 테스트용 기본 메시 설정 (경로는 실제 스켈레탈 메시 캐시 파일로 변경)
   // SetSkeletalMesh(GDataDir + "/Test.fbx"); 
}


void USkeletalMeshComponent::TickComponent(float DeltaTime)
{
    Super::TickComponent(DeltaTime);
    //// FOR TEST ////
    if (!SkeletalMesh) { return; } // 부모의 SkeletalMesh 확인

    // 1. 테스트할 뼈 인덱스 (모델에 따라 1, 5, 10 등 바꿔보세요)
    constexpr int32 TEST_BONE_INDEX = 10;
    
    // 3. 테스트 시간 누적
    static float TestTime = 0;
    static auto TestBoneBasePose = CurrentLocalSpacePose[TEST_BONE_INDEX];
    TestTime += DeltaTime;

    // 4. sin 함수를 이용해 -1 ~ +1 사이를 왕복하는 회전값 생성
    // (예: Y축(Yaw)을 기준으로 1초에 1라디안(약 57도)씩 왕복)
    float Angle = sinf(TestTime * 2.f);
    FQuat TestRotation = FQuat::FromAxisAngle(FVector(0.f, 0.f, 1.f), Angle);
    TestRotation.Normalize();

    // 5. [중요] 원본 T-Pose에 테스트 회전을 누적
    FTransform NewLocalPose = TestBoneBasePose;
    NewLocalPose.Rotation = TestRotation * TestBoneBasePose.Rotation;
        
    // 6. [핵심] 기즈모가 하듯이, 뼈의 로컬 트랜스폼을 강제 설정
    // (이 함수는 내부적으로 ForceRecomputePose()를 호출함)
    SetBoneLocalTransform(TEST_BONE_INDEX, NewLocalPose);
    //// FOR TEST ////
}

void USkeletalMeshComponent::SetSkeletalMesh(const FString& PathFileName)
{
    Super::SetSkeletalMesh(PathFileName);

    if (SkeletalMesh && SkeletalMesh->GetSkeletalMeshData())
    {
        const FSkeleton& Skeleton = SkeletalMesh->GetSkeletalMeshData()->Skeleton;
        const int32 NumBones = Skeleton.Bones.Num();

        CurrentLocalSpacePose.SetNum(NumBones);
        CurrentComponentSpacePose.SetNum(NumBones);
        TempFinalSkinningMatrices.SetNum(NumBones);

        for (int32 i = 0; i < NumBones; ++i)
        {
            const FBone& ThisBone = Skeleton.Bones[i];
            const int32 ParentIndex = ThisBone.ParentIndex;
            FMatrix LocalBindMatrix;

            if (ParentIndex == -1) // 루트 본
            {
                LocalBindMatrix = ThisBone.BindPose;
            }
            else // 자식 본
            {
                const FMatrix& ParentInverseBindPose = Skeleton.Bones[ParentIndex].InverseBindPose;
                LocalBindMatrix = ThisBone.BindPose * ParentInverseBindPose;
            }
            // 계산된 로컬 행렬을 로컬 트랜스폼으로 변환
            CurrentLocalSpacePose[i] = FTransform(LocalBindMatrix); 
        }
        
        ForceRecomputePose(); 
    }
    else
    {
        // 메시 로드 실패 시 버퍼 비우기
        CurrentLocalSpacePose.Empty();
        CurrentComponentSpacePose.Empty();
        TempFinalSkinningMatrices.Empty();
    }
}

void USkeletalMeshComponent::SetBoneLocalTransform(int32 BoneIndex, const FTransform& NewLocalTransform)
{
    if (CurrentLocalSpacePose.Num() > BoneIndex)
    {
        CurrentLocalSpacePose[BoneIndex] = NewLocalTransform;
        ForceRecomputePose();
    }
}

FTransform USkeletalMeshComponent::GetBoneLocalTransform(int32 BoneIndex) const
{
    if (CurrentLocalSpacePose.Num() > BoneIndex)
    {
        return CurrentLocalSpacePose[BoneIndex];
    }
    return FTransform();
}

FTransform USkeletalMeshComponent::GetBoneWorldTransform(int32 BoneIndex)
{
    if (CurrentLocalSpacePose.Num() > BoneIndex)
    {
        // 뼈의 컴포넌트 공간 트랜스폼 * 컴포넌트의 월드 트랜스폼
        return GetWorldTransform().GetWorldTransform(CurrentComponentSpacePose[BoneIndex]);
    }
    return GetWorldTransform(); // 실패 시 컴포넌트 위치 반환
}

void USkeletalMeshComponent::ForceRecomputePose()
{
    if (!SkeletalMesh) { return; } 

    // LocalSpace -> ComponentSpace 계산
    UpdateComponentSpaceTransforms();
    // ComponentSpace -> Final Skinning Matrices 계산
    UpdateFinalSkinningMatrices();
    UpdateSkinningMatrices(TempFinalSkinningMatrices);
    PerformSkinning();
}

void USkeletalMeshComponent::UpdateComponentSpaceTransforms()
{
    const FSkeleton& Skeleton = SkeletalMesh->GetSkeletalMeshData()->Skeleton;
    const int32 NumBones = Skeleton.Bones.Num();

    for (int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex)
    {
        const FTransform& LocalTransform = CurrentLocalSpacePose[BoneIndex];
        const int32 ParentIndex = Skeleton.Bones[BoneIndex].ParentIndex;

        if (ParentIndex == -1) // 루트 본
        {
            CurrentComponentSpacePose[BoneIndex] = LocalTransform;
        }
        else // 자식 본
        {
            const FTransform& ParentComponentTransform = CurrentComponentSpacePose[ParentIndex];
            CurrentComponentSpacePose[BoneIndex] = ParentComponentTransform.GetWorldTransform(LocalTransform);
        }
    }
}

void USkeletalMeshComponent::UpdateFinalSkinningMatrices()
{
    const FSkeleton& Skeleton = SkeletalMesh->GetSkeletalMeshData()->Skeleton;
    const int32 NumBones = Skeleton.Bones.Num();

    for (int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex)
    {
        const FMatrix& InvBindPose = Skeleton.Bones[BoneIndex].InverseBindPose;
        const FMatrix ComponentPoseMatrix = CurrentComponentSpacePose[BoneIndex].ToMatrix();
        
        TempFinalSkinningMatrices[BoneIndex] = InvBindPose * ComponentPoseMatrix;
    }
}
