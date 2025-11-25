#pragma once

#include "ParticleModuleEventReceiverBase.h"
#include "UParticleModuleEventReceiverSpawn.generated.h"

/**
 * UParticleModuleEventReceiverSpawn
 *
 * 이벤트 수신 시 새로운 파티클을 스폰하는 모듈입니다.
 */
UCLASS(DisplayName="이벤트 수신 스폰", Description="이벤트 수신 시 새로운 파티클을 스폰합니다")
class UParticleModuleEventReceiverSpawn : public UParticleModuleEventReceiverBase
{
public:
	GENERATED_REFLECTION_BODY()

public:
	// 이벤트당 스폰할 파티클 수
	UPROPERTY(EditAnywhere, Category="Spawn", meta=(ClampMin="1", ToolTip="이벤트당 스폰할 파티클 수"))
	int32 SpawnCount = 1;

	// 이벤트 위치에서 스폰할지 여부 (false면 이미터 원점에서 스폰)
	UPROPERTY(EditAnywhere, Category="Spawn", meta=(ToolTip="이벤트 발생 위치에서 스폰할지 여부"))
	bool bUseEventLocation = true;

	// 이벤트 속도를 상속할지 여부
	UPROPERTY(EditAnywhere, Category="Spawn", meta=(ToolTip="이벤트 속도를 상속할지 여부"))
	bool bInheritVelocity = false;

	// 속도 상속 비율 (0~1)
	UPROPERTY(EditAnywhere, Category="Spawn", meta=(ClampMin="0.0", ClampMax="1.0", ToolTip="속도 상속 비율"))
	float VelocityScale = 1.0f;

	UParticleModuleEventReceiverSpawn()
	{
		EventType = EParticleEventType::Death;  // 기본적으로 Death 이벤트 수신
	}

	// 이벤트 처리: 새 파티클 스폰
	virtual void HandleEvent(FParticleEmitterInstance* Owner, const FParticleEventData& Event) override;
	virtual void HandleCollisionEvent(FParticleEmitterInstance* Owner, const FParticleEventCollideData& Event) override;

	// 직렬화
	virtual void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;

private:
	// 실제 스폰 로직
	void SpawnParticlesAtLocation(FParticleEmitterInstance* Owner, const FVector& Location, const FVector& Velocity);
};
