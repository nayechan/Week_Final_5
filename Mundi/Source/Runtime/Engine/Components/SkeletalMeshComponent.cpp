#include "pch.h"
#include "SkeletalMeshComponent.h"
#include "PlatformTime.h"
#include "AnimSequence.h"
#include "AnimDataModel.h"
#include "FbxLoader.h"
#include "ResourceManager.h"

USkeletalMeshComponent::USkeletalMeshComponent()
{
    // 테스트용 DancingRacer 메시 및 애니메이션 설정
    FString TestFbxPath = "Data/DancingRacer.fbx";

    // 1. 스켈레탈 메시 로드
    SetSkeletalMesh(TestFbxPath);

    // 2. 메시가 로드된 후 애니메이션 로드
    if (SkeletalMesh && SkeletalMesh->GetSkeleton())
    {
        UE_LOG("USkeletalMeshComponent: Loading test animation from '%s'", TestFbxPath.c_str());

        // 3. FbxLoader를 통해 애니메이션 로드
        UAnimSequence* TestAnimation = UFbxLoader::GetInstance().LoadFbxAnimation(
            TestFbxPath,
            SkeletalMesh->GetSkeleton()
        );

        // 4. 애니메이션 로드 성공 시 재생 시작
        if (TestAnimation)
        {
            UE_LOG("USkeletalMeshComponent: Starting animation playback (Loop: true)");
            PlayAnimation(TestAnimation, true);
        }
        else
        {
            UE_LOG("USkeletalMeshComponent: Failed to load animation");
        }
    }
    else
    {
        UE_LOG("USkeletalMeshComponent: Failed to load skeletal mesh");
    }
}


void USkeletalMeshComponent::TickComponent(float DeltaTime)
{
    Super::TickComponent(DeltaTime);

    if (!SkeletalMesh) { return; }

    // 애니메이션 재생 중인 경우
    if (bIsPlaying && CurrentAnimation)
    {
        // 1. 애니메이션 시간 업데이트
        float OldTime = CurrentAnimationTime;
        CurrentAnimationTime += DeltaTime;

        // 2. 시간 래핑 (루핑 또는 클램핑)
        float AnimLength = CurrentAnimation->GetPlayLength();
        if (CurrentAnimationTime >= AnimLength)
        {
            if (bLooping)
            {
                CurrentAnimationTime = fmodf(CurrentAnimationTime, AnimLength);
            }
            else
            {
                CurrentAnimationTime = AnimLength;
                bIsPlaying = false; // 애니메이션 종료
            }
        }

        // 디버그 로깅: 애니메이션 시간 업데이트 확인
        static float LastLogTime = 0.0f;
        if (CurrentAnimationTime - LastLogTime >= 1.0f) // 1초마다 로그
        {
            UE_LOG("USkeletalMeshComponent::TickComponent: Time %.3f -> %.3f (Delta: %.4f, Length: %.3f)",
                OldTime, CurrentAnimationTime, DeltaTime, AnimLength);
            LastLogTime = CurrentAnimationTime;
        }

        // 3. 애니메이션 평가 및 포즈 적용
        EvaluateAnimation(CurrentAnimationTime);

        // 4. 포즈 갱신
        ForceRecomputePose();
    }
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

void USkeletalMeshComponent::SetBoneWorldTransform(int32 BoneIndex, const FTransform& NewWorldTransform)
{
    if (BoneIndex < 0 || BoneIndex >= CurrentLocalSpacePose.Num())
        return;

    const int32 ParentIndex = SkeletalMesh->GetSkeleton()->Bones[BoneIndex].ParentIndex;

    const FTransform& ParentWorldTransform = GetBoneWorldTransform(ParentIndex);
    FTransform DesiredLocal = ParentWorldTransform.GetRelativeTransform(NewWorldTransform);

    SetBoneLocalTransform(BoneIndex, DesiredLocal);
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
    if (CurrentLocalSpacePose.Num() > BoneIndex && BoneIndex >= 0)
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
    // UpdateFinalSkinningMatrices()에서 UpdateSkinningMatrices()를 호출하여 본 행렬 계산 시간과 함께 전달
    UpdateFinalSkinningMatrices();
    // PerformSkinning은 CollectMeshBatches에서 전역 모드에 따라 수행됨
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

    // 본 행렬 계산 시간 측정 시작
    uint64 BoneMatrixCalcStart = FWindowsPlatformTime::Cycles64();

    for (int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex)
    {
        const FMatrix& InvBindPose = Skeleton.Bones[BoneIndex].InverseBindPose;
        const FMatrix ComponentPoseMatrix = CurrentComponentSpacePose[BoneIndex].ToMatrix();

        TempFinalSkinningMatrices[BoneIndex] = InvBindPose * ComponentPoseMatrix;
    }

    // 본 행렬 계산 시간 측정 종료
    uint64 BoneMatrixCalcEnd = FWindowsPlatformTime::Cycles64();
    double BoneMatrixCalcTimeMS = FWindowsPlatformTime::ToMilliseconds(BoneMatrixCalcEnd - BoneMatrixCalcStart);

    // 본 행렬 계산 시간을 부모 USkinnedMeshComponent로 전달
    // 부모에서 실제 스키닝 모드(CPU/GPU)에 따라 통계에 추가됨
    UpdateSkinningMatrices(TempFinalSkinningMatrices, BoneMatrixCalcTimeMS);
}

void USkeletalMeshComponent::PlayAnimation(UAnimSequence* Animation, bool bLoop)
{
    if (!Animation)
    {
        UE_LOG("USkeletalMeshComponent::PlayAnimation: Invalid animation");
        return;
    }

    CurrentAnimation = Animation;
    CurrentAnimationTime = 0.0f;
    bIsPlaying = true;
    bLooping = bLoop;

    UE_LOG("USkeletalMeshComponent::PlayAnimation: Started animation (Length: %.3f sec, Loop: %s)",
        Animation->GetPlayLength(), bLoop ? "true" : "false");
}

void USkeletalMeshComponent::StopAnimation()
{
    bIsPlaying = false;
    CurrentAnimationTime = 0.0f;
    UE_LOG("USkeletalMeshComponent::StopAnimation: Stopped animation");
}

void USkeletalMeshComponent::SetAnimationTime(float Time)
{
    if (CurrentAnimation)
    {
        float ClampedTime = Time;
        float AnimLength = CurrentAnimation->GetPlayLength();

        if (bLooping)
        {
            while (ClampedTime >= AnimLength)
            {
                ClampedTime -= AnimLength;
            }
            while (ClampedTime < 0.0f)
            {
                ClampedTime += AnimLength;
            }
        }
        else
        {
            ClampedTime = FMath::Clamp(ClampedTime, 0.0f, AnimLength);
        }

        CurrentAnimationTime = ClampedTime;
    }
}

void USkeletalMeshComponent::EvaluateAnimation(float Time)
{
    if (!CurrentAnimation || !SkeletalMesh)
        return;

    const UAnimDataModel* DataModel = CurrentAnimation->GetDataModel();
    if (!DataModel || !DataModel->IsValid())
    {
        UE_LOG("USkeletalMeshComponent::EvaluateAnimation: DataModel is invalid!");
        return;
    }

    const FSkeleton* Skeleton = SkeletalMesh->GetSkeleton();
    if (!Skeleton)
    {
        UE_LOG("USkeletalMeshComponent::EvaluateAnimation: Skeleton is null!");
        return;
    }

    const TArray<FBoneAnimationTrack>& AnimTracks = DataModel->GetBoneAnimationTracks();

    // 디버그: 처음 한 번만 트랙 정보 출력
    static bool bLoggedOnce = false;
    if (!bLoggedOnce)
    {
        UE_LOG("USkeletalMeshComponent::EvaluateAnimation: %d bones, %d animation tracks",
            Skeleton->Bones.Num(), AnimTracks.Num());
        bLoggedOnce = true;
    }

    int32 TracksApplied = 0;
    // 각 본에 대해 애니메이션 트랙 평가
    for (int32 BoneIndex = 0; BoneIndex < Skeleton->Bones.Num(); ++BoneIndex)
    {
        const FBone& Bone = Skeleton->Bones[BoneIndex];

        // 해당 본의 애니메이션 트랙 찾기
        const FRawAnimSequenceTrack* Track = DataModel->GetTrackByBoneIndex(BoneIndex);

        if (Track && !Track->IsEmpty())
        {
            TracksApplied++;

            // 프레임 인덱스 계산
            float FrameRate = DataModel->FrameRate;
            float FrameTime = Time * FrameRate;
            int32 FrameIndex0 = static_cast<int32>(floorf(FrameTime));
            int32 FrameIndex1 = FrameIndex0 + 1;
            float Alpha = FrameTime - static_cast<float>(FrameIndex0);

            // 키프레임 인덱스 클램핑
            int32 NumPosKeys = Track->GetNumPosKeys();
            int32 NumRotKeys = Track->GetNumRotKeys();
            int32 NumScaleKeys = Track->GetNumScaleKeys();

            // Position 보간
            FVector Position;
            if (NumPosKeys > 0)
            {
                int32 PosIdx0 = FMath::Clamp(FrameIndex0, 0, NumPosKeys - 1);
                int32 PosIdx1 = FMath::Clamp(FrameIndex1, 0, NumPosKeys - 1);
                Position = FMath::Lerp(Track->PositionKeys[PosIdx0], Track->PositionKeys[PosIdx1], Alpha);
            }
            else
            {
                Position = FVector(0, 0, 0);
            }

            // Rotation 보간 (Slerp)
            FQuat Rotation;
            if (NumRotKeys > 0)
            {
                int32 RotIdx0 = FMath::Clamp(FrameIndex0, 0, NumRotKeys - 1);
                int32 RotIdx1 = FMath::Clamp(FrameIndex1, 0, NumRotKeys - 1);
                Rotation = FQuat::Slerp(Track->RotationKeys[RotIdx0], Track->RotationKeys[RotIdx1], Alpha);
                Rotation.Normalize();
            }
            else
            {
                Rotation = FQuat::Identity();
            }

            // Scale 보간
            FVector Scale;
            if (NumScaleKeys > 0)
            {
                int32 ScaleIdx0 = FMath::Clamp(FrameIndex0, 0, NumScaleKeys - 1);
                int32 ScaleIdx1 = FMath::Clamp(FrameIndex1, 0, NumScaleKeys - 1);
                Scale = FMath::Lerp(Track->ScaleKeys[ScaleIdx0], Track->ScaleKeys[ScaleIdx1], Alpha);
            }
            else
            {
                Scale = FVector(1, 1, 1);
            }

            // 디버그: 루트 본 변환 로깅 (매 초마다)
            if (BoneIndex == 0)
            {
                static float LastRootLogTime = 0.0f;
                if (Time - LastRootLogTime >= 1.0f)
                {
                    UE_LOG("  Root Bone [Frame %.2f, Alpha %.3f]: Pos(%.2f, %.2f, %.2f) Rot(%.2f, %.2f, %.2f, %.2f)",
                        FrameTime, Alpha, Position.X, Position.Y, Position.Z,
                        Rotation.X, Rotation.Y, Rotation.Z, Rotation.W);
                    LastRootLogTime = Time;
                }
            }

            // CurrentLocalSpacePose 업데이트
            CurrentLocalSpacePose[BoneIndex] = FTransform(Position, Rotation, Scale);
        }
    }

    // 디버그: 적용된 트랙 수 확인 (1초마다)
    static float LastTrackLogTime = 0.0f;
    if (Time - LastTrackLogTime >= 1.0f)
    {
        UE_LOG("  Applied %d/%d animation tracks", TracksApplied, Skeleton->Bones.Num());
        LastTrackLogTime = Time;
    }
}
