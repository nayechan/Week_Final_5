#include "pch.h"
#include "AnimSequence.h"

UAnimSequence::UAnimSequence()
	: AnimDataModel(nullptr)
{
}

void UAnimSequence::Load(const FString& InFilePath)
{
	// TODO: FBX 로딩은 Phase 3에서 구현
	SetFilePath(InFilePath);
}

void UAnimSequence::SetAnimDataModel(UAnimDataModel* InDataModel)
{
	AnimDataModel = InDataModel;
	if (AnimDataModel)
	{
		SetSequenceLength(AnimDataModel->SequenceLength);
	}
}

bool UAnimSequence::IsValid() const
{
	return AnimDataModel != nullptr && AnimDataModel->IsValid();
}

void UAnimSequence::GetBonePose(float Time, TArray<FTransform>& OutBonePose) const
{
	if (!IsValid())
	{
		return;
	}

	// 시간을 [0, SequenceLength] 범위로 클램프
	Time = FMath::Clamp(Time, 0.0f, SequenceLength);

	// 각 본 트랙에 대해 포즈 계산
	const TArray<FBoneAnimationTrack>& Tracks = AnimDataModel->GetBoneAnimationTracks();
	for (const FBoneAnimationTrack& Track : Tracks)
	{
		if (Track.BoneIndex < 0 || Track.BoneIndex >= OutBonePose.Num())
		{
			continue; // 유효하지 않은 본 인덱스
		}

		const FRawAnimSequenceTrack& RawTrack = Track.InternalTrack;

		// Position 보간
		FVector Position = InterpolatePosition(RawTrack.PositionKeys, Time, AnimDataModel->FrameRate);

		// Rotation 보간 (Slerp)
		FQuat Rotation = InterpolateRotation(RawTrack.RotationKeys, Time, AnimDataModel->FrameRate);

		// Scale 보간
		FVector Scale = InterpolateScale(RawTrack.ScaleKeys, Time, AnimDataModel->FrameRate);

		// 본 트랜스폼 설정
		OutBonePose[Track.BoneIndex] = FTransform(Position, Rotation, Scale);
	}
}

void UAnimSequence::FindKeyframeIndices(float Time, int32 NumKeys, float FrameRate, int32& OutIndex0, int32& OutIndex1, float& OutAlpha) const
{
	if (NumKeys == 0)
	{
		OutIndex0 = 0;
		OutIndex1 = 0;
		OutAlpha = 0.0f;
		return;
	}

	if (NumKeys == 1)
	{
		OutIndex0 = 0;
		OutIndex1 = 0;
		OutAlpha = 0.0f;
		return;
	}

	// 시간을 프레임 인덱스로 변환
	float FrameTime = Time * FrameRate;
	int32 FrameIndex = static_cast<int32>(FrameTime);

	// 인덱스 클램프
	FrameIndex = FMath::Clamp(FrameIndex, 0, NumKeys - 1);

	OutIndex0 = FrameIndex;
	OutIndex1 = FMath::Min(FrameIndex + 1, NumKeys - 1);

	// 보간 알파 계산 (0.0 ~ 1.0)
	OutAlpha = FrameTime - static_cast<float>(FrameIndex);
	OutAlpha = FMath::Clamp(OutAlpha, 0.0f, 1.0f);
}

FVector UAnimSequence::InterpolatePosition(const TArray<FVector>& PositionKeys, float Time, float FrameRate) const
{
	if (PositionKeys.Num() == 0)
	{
		return FVector(0.0f, 0.0f, 0.0f);
	}

	if (PositionKeys.Num() == 1)
	{
		return PositionKeys[0];
	}

	int32 Index0, Index1;
	float Alpha;
	FindKeyframeIndices(Time, PositionKeys.Num(), FrameRate, Index0, Index1, Alpha);

	// 선형 보간 (Lerp)
	return FMath::Lerp(PositionKeys[Index0], PositionKeys[Index1], Alpha);
}

FQuat UAnimSequence::InterpolateRotation(const TArray<FQuat>& RotationKeys, float Time, float FrameRate) const
{
	if (RotationKeys.Num() == 0)
	{
		return FQuat::Identity();
	}

	if (RotationKeys.Num() == 1)
	{
		return RotationKeys[0];
	}

	int32 Index0, Index1;
	float Alpha;
	FindKeyframeIndices(Time, RotationKeys.Num(), FrameRate, Index0, Index1, Alpha);

	// 구면 선형 보간 (Slerp)
	return FQuat::Slerp(RotationKeys[Index0], RotationKeys[Index1], Alpha);
}

FVector UAnimSequence::InterpolateScale(const TArray<FVector>& ScaleKeys, float Time, float FrameRate) const
{
	if (ScaleKeys.Num() == 0)
	{
		return FVector(1.0f, 1.0f, 1.0f);
	}

	if (ScaleKeys.Num() == 1)
	{
		return ScaleKeys[0];
	}

	int32 Index0, Index1;
	float Alpha;
	FindKeyframeIndices(Time, ScaleKeys.Num(), FrameRate, Index0, Index1, Alpha);

	// 선형 보간 (Lerp)
	return FMath::Lerp(ScaleKeys[Index0], ScaleKeys[Index1], Alpha);
}
