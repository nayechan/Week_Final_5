#pragma once
#include "ResourceBase.h"
#include "UAnimationAsset.generated.h"

// 전방 선언
struct FSkeleton;

/**
 * 애니메이션 에셋 베이스 클래스
 * 모든 애니메이션 관련 에셋의 기본 클래스
 * UAnimSequence, UAnimMontage 등이 이 클래스를 상속받음
 */
UCLASS(DisplayName="애니메이션 에셋", Description="애니메이션 에셋의 기본 클래스")
class UAnimationAsset : public UResourceBase
{
	GENERATED_REFLECTION_BODY()

public:
	UAnimationAsset() = default;
	virtual ~UAnimationAsset() = default;

	/**
	 * 애니메이션의 총 재생 길이를 반환 (초 단위)
	 * @return 재생 길이 (초)
	 */
	virtual float GetPlayLength() const { return 0.0f; }

	/**
	 * 이 애니메이션과 연결된 스켈레톤을 반환
	 * @return 스켈레톤 포인터 (없으면 nullptr)
	 */
	virtual const FSkeleton* GetSkeleton() const { return nullptr; }

	/**
	 * 애니메이션 에셋이 유효한지 확인
	 * @return 유효하면 true
	 */
	virtual bool IsValid() const { return GetPlayLength() > 0.0f; }
};
