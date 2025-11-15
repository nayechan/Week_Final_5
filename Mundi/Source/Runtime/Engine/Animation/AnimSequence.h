#pragma once
#include "AnimSequenceBase.h"
#include "AnimDataModel.h"
#include "UAnimSequence.generated.h"

/**
 * 애니메이션 시퀀스
 * 실제 애니메이션 키프레임 데이터를 관리하는 구체 클래스
 * FBX에서 로드된 애니메이션 데이터를 저장하고 재생
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
	 */
	void Load(const FString& InFilePath);

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

protected:
	/** 실제 애니메이션 키프레임 데이터를 저장하는 모델 */
	UAnimDataModel* AnimDataModel = nullptr;

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
