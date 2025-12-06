#pragma once
#include "AnimSequenceBase.h"
#include "AnimDataModel.h"
#include "Source/Runtime/Engine/Viewer/ViewerState.h"
#include "UAnimSequence.generated.h"

/**
 * 애니메이션 시퀀스
 * 키프레임 데이터(AnimDataModel)와 사용자 편집 데이터(NotifyTracks)를 관리
 * 키프레임: FBX → .anim.bin 캐시
 * NotifyTracks: .animsequence 파일에 저장
 */
UCLASS(DisplayName="애니메이션 시퀀스", Description="키프레임 기반 스켈레탈 애니메이션")
class UAnimSequence : public UAnimSequenceBase
{
	GENERATED_REFLECTION_BODY()

public:
	UAnimSequence();
	virtual ~UAnimSequence() = default;

	/**
	 * 파일에서 애니메이션 로드
	 * @param InFilePath 애니메이션 파일 경로
	 * @param InDevice DirectX 디바이스 (사용하지 않지만 ResourceManager 호환성 위해 필요)
	 */
	void Load(const FString& InFilePath, ID3D11Device* InDevice = nullptr);

	/**
	 * 특정 시간의 본 포즈 가져오기
	 * @param Time 평가할 시간 (초 단위)
	 * @param OutBonePose 출력 본 트랜스폼 배열 (스켈레톤의 본 개수만큼)
	 */
	void GetBonePose(float Time, TArray<FTransform>& OutBonePose) const;

	/**
	 * 애니메이션 데이터 모델 가져오기
	 * @return 애니메이션 데이터 모델 포인터
	 */
	UAnimDataModel* GetDataModel() const { return AnimDataModel; }

	/**
	 * 애니메이션 데이터 모델 설정
	 * @param InDataModel 새로운 데이터 모델
	 */
	void SetAnimDataModel(UAnimDataModel* InDataModel);

	/**
	 * 애니메이션이 유효한지 확인
	 * @return 유효하면 true
	 */
	virtual bool IsValid() const override;

	// UAnimSequenceBase override
	virtual void ExtractBonePose(const FSkeleton& Skeleton, float Time, bool bLooping, bool bInterpolate, TArray<FTransform>& OutLocalPose) const override;

	/**
	 * JSON 직렬화 (AnimDataModel 포함 전체 데이터)
	 * @param bInIsLoading true면 로드, false면 저장
	 * @param InOutHandle JSON 데이터
	 */
	virtual void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;

	/**
	 * 애니메이션 시퀀스를 .animsequence 파일로 저장
	 * @param InFilePath 저장할 파일 경로 (확장자 포함)
	 * @return 성공하면 true
	 */
	bool Save(const FString& InFilePath);

	/**
	 * .animsequence 파일에서 NotifyTracks 및 메타데이터 로드
	 * 키프레임 데이터는 SourceFilePath의 FBX에서 로드해야 함
	 * @param InFilePath .animsequence 파일 경로
	 * @return 성공하면 true
	 */
	bool LoadAnimSequenceFile(const FString& InFilePath);

	/**
	 * 캐시 파일 경로 설정 (.anim.bin)
	 */
	void SetCachePath(const FString& InPath) { CachePath = InPath; }
	const FString& GetCachePath() const { return CachePath; }

	/**
	 * NotifyTracks 접근자
	 */
	TArray<FNotifyTrack>& GetNotifyTracks() { return NotifyTracks; }
	const TArray<FNotifyTrack>& GetNotifyTracks() const { return NotifyTracks; }
	void SetNotifyTracks(const TArray<FNotifyTrack>& InTracks) { NotifyTracks = InTracks; }

protected:
	/** 실제 애니메이션 키프레임 데이터를 저장하는 모델 */
	UAnimDataModel* AnimDataModel = nullptr;

	/** 사용자 편집 노티파이 트랙 (.animsequence에 저장) */
	TArray<FNotifyTrack> NotifyTracks;

	/** .anim.bin 캐시 파일 경로 (FBX 독립 로딩용) */
	FString CachePath;

private:
	/**
	 * Position 키프레임 보간
	 * @param PositionKeys 위치 키 배열
	 * @param Time 평가 시간
	 * @return 보간된 위치
	 */
	FVector InterpolatePosition(const TArray<FVector>& PositionKeys, float Time, float FrameRate) const;

	/**
	 * Rotation 키프레임 보간 (Slerp)
	 * @param RotationKeys 회전 키 배열
	 * @param Time 평가 시간
	 * @return 보간된 회전 (Quaternion)
	 */
	FQuat InterpolateRotation(const TArray<FQuat>& RotationKeys, float Time, float FrameRate) const;

	/**
	 * Scale 키프레임 보간
	 * @param ScaleKeys 스케일 키 배열
	 * @param Time 평가 시간
	 * @return 보간된 스케일
	 */
	FVector InterpolateScale(const TArray<FVector>& ScaleKeys, float Time, float FrameRate) const;

	/**
	 * 시간에서 키프레임 인덱스 찾기
	 * @param Time 평가 시간
	 * @param NumKeys 키프레임 개수
	 * @param FrameRate 프레임레이트
	 * @param OutIndex0 이전 키프레임 인덱스 (출력)
	 * @param OutIndex1 다음 키프레임 인덱스 (출력)
	 * @param OutAlpha 보간 알파 값 (출력)
	 */
	void FindKeyframeIndices(float Time, int32 NumKeys, float FrameRate, int32& OutIndex0, int32& OutIndex1, float& OutAlpha) const;
};
