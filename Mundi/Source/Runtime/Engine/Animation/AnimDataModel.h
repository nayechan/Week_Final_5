#pragma once
#include "Object.h"
#include "AnimTypes.h"
#include "UAnimDataModel.generated.h"

/**
 * 애니메이션 데이터 모델
 * 실제 애니메이션 키프레임 데이터를 저장하는 클래스
 * UAnimSequence가 이 클래스를 통해 애니메이션 데이터에 접근함
 */
UCLASS(DisplayName="애니메이션 데이터 모델", Description="애니메이션 키프레임 데이터 저장소")
class UAnimDataModel : public UObject
{
	GENERATED_REFLECTION_BODY()

public:
	UAnimDataModel() = default;
	virtual ~UAnimDataModel() = default;

	/** 본별 애니메이션 트랙 배열 */
	TArray<FBoneAnimationTrack> BoneAnimationTracks;

	/** 애니메이션 전체 재생 길이 (초 단위) */
	float SequenceLength = 0.0f;

	/** 프레임레이트 (초당 프레임 수) */
	float FrameRate = 30.0f;

	/** 전체 프레임 개수 */
	int32 NumberOfFrames = 0;

	/** 전체 키프레임 개수 */
	int32 NumberOfKeys = 0;

	/** FBX AnimCurve에서 추출한 실제 키프레임 데이터 */
	FAnimationCurveData CurveData;

	/**
	 * 본 인덱스로 트랙 가져오기
	 * @param BoneIndex 스켈레톤의 본 인덱스
	 * @return 해당 본의 애니메이션 트랙 (없으면 nullptr)
	 */
	const FRawAnimSequenceTrack* GetTrackByBoneIndex(int32 BoneIndex) const
	{
		for (const FBoneAnimationTrack& Track : BoneAnimationTracks)
		{
			if (Track.BoneIndex == BoneIndex)
			{
				return &Track.InternalTrack;
			}
		}
		return nullptr;
	}

	/**
	 * 본 이름으로 트랙 가져오기
	 * @param BoneName 본 이름
	 * @return 해당 본의 애니메이션 트랙 (없으면 nullptr)
	 */
	const FRawAnimSequenceTrack* GetTrackByBoneName(const FString& BoneName) const
	{
		for (const FBoneAnimationTrack& Track : BoneAnimationTracks)
		{
			if (Track.BoneName == BoneName)
			{
				return &Track.InternalTrack;
			}
		}
		return nullptr;
	}

	/**
	 * 본별 애니메이션 트랙 배열 가져오기
	 * @return 본별 애니메이션 트랙 배열
	 */
	const TArray<FBoneAnimationTrack>& GetBoneAnimationTracks() const
	{
		return BoneAnimationTracks;
	}

	/**
	 * 애니메이션 데이터가 유효한지 확인
	 * @return 데이터가 유효하면 true
	 */
	bool IsValid() const
	{
		return BoneAnimationTracks.Num() > 0 && SequenceLength > 0.0f;
	}

	/**
	 * 애니메이션 데이터 초기화
	 */
	void Reset()
	{
		BoneAnimationTracks.clear();
		CurveData.Reset();
		SequenceLength = 0.0f;
		FrameRate = 30.0f;
		NumberOfFrames = 0;
		NumberOfKeys = 0;
	}

	/**
	 * 아카이브로 직렬화
	 */
	friend FArchive& operator<<(FArchive& Ar, UAnimDataModel& Model)
	{
		// 본 애니메이션 트랙 직렬화
		int32 NumTracks = Model.BoneAnimationTracks.Num();
		Ar << NumTracks;
		if (Ar.IsLoading())
		{
			Model.BoneAnimationTracks.SetNum(NumTracks);
		}
		for (int32 i = 0; i < NumTracks; ++i)
		{
			Ar << Model.BoneAnimationTracks[i];
		}

		// 애니메이션 메타 데이터 직렬화
		Ar << Model.SequenceLength;
		Ar << Model.FrameRate;
		Ar << Model.NumberOfFrames;
		Ar << Model.NumberOfKeys;

		// 커브 데이터 직렬화
		Ar << Model.CurveData;

		return Ar;
	}
};
