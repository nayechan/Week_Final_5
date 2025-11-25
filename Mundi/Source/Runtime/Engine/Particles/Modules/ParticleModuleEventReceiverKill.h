#pragma once

#include "ParticleModuleEventReceiverBase.h"
#include "UParticleModuleEventReceiverKill.generated.h"

/**
 * UParticleModuleEventReceiverKill
 *
 * 이벤트 수신 시 파티클을 제거하는 모듈입니다.
 */
UCLASS(DisplayName="이벤트 수신 킬", Description="이벤트 수신 시 파티클을 제거합니다")
class UParticleModuleEventReceiverKill : public UParticleModuleEventReceiverBase
{
public:
	GENERATED_REFLECTION_BODY()

public:
	// 제거할 파티클 수 (0 = 전부)
	UPROPERTY(EditAnywhere, Category="Kill", meta=(ClampMin="0", ToolTip="제거할 파티클 수 (0 = 전부)"))
	int32 KillCount = 0;

	// 이벤트 위치에서 가장 가까운 파티클부터 제거할지 여부
	UPROPERTY(EditAnywhere, Category="Kill", meta=(ToolTip="이벤트 위치에서 가장 가까운 파티클부터 제거"))
	bool bKillNearestFirst = false;

	UParticleModuleEventReceiverKill()
	{
		EventType = EParticleEventType::Collision;  // 기본적으로 Collision 이벤트 수신
	}

	// 이벤트 처리: 파티클 제거
	virtual void HandleEvent(FParticleEmitterInstance* Owner, const FParticleEventData& Event) override;

	// 직렬화
	virtual void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;

private:
	// 제거할 파티클 선택 로직
	void KillParticles(FParticleEmitterInstance* Owner, const FVector& EventLocation);
};
