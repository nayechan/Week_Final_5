#pragma once

#include "ParticleModule.h"
#include "ParticleEventTypes.h"
#include "UParticleModuleEventReceiverBase.generated.h"

/**
 * UParticleModuleEventReceiverBase
 *
 * 파티클 이벤트를 수신하는 모듈의 기본 클래스입니다.
 * 다른 이미터에서 발생한 이벤트를 수신하여 반응합니다.
 */
UCLASS(DisplayName="이벤트 수신 기본", Description="파티클 이벤트를 수신하는 기본 모듈입니다")
class UParticleModuleEventReceiverBase : public UParticleModule
{
public:
	GENERATED_REFLECTION_BODY()

public:
	// 수신할 이벤트 타입
	UPROPERTY(EditAnywhere, Category="Event", meta=(ToolTip="수신할 이벤트 타입"))
	EParticleEventType EventType = EParticleEventType::Any;

	// 이벤트 소스 이름 (특정 이미터의 이벤트만 수신, 빈 문자열이면 모두 수신)
	UPROPERTY(EditAnywhere, Category="Event", meta=(ToolTip="이벤트를 발생시킨 이미터 이름 (빈 문자열이면 모두 수신)"))
	FString EventName;

	UParticleModuleEventReceiverBase()
	{
		bSpawnModule = false;
		bUpdateModule = false;
	}

	virtual ~UParticleModuleEventReceiverBase() = default;

	// 이벤트 처리 (파생 클래스에서 구현)
	virtual void HandleEvent(FParticleEmitterInstance* Owner, const FParticleEventData& Event) {}
	virtual void HandleCollisionEvent(FParticleEmitterInstance* Owner, const FParticleEventCollideData& Event) {}

	// 직렬화
	virtual void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;
};
