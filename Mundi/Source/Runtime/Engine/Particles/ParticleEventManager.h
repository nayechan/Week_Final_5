#pragma once

#include "Actor.h"
#include "Delegates.h"
#include "ParticleEventTypes.h"
#include "AParticleEventManager.generated.h"

class UParticleSystemComponent;

// 파티클 이벤트 델리게이트 타입 정의
DECLARE_DELEGATE_TYPE(FOnParticleCollision, UParticleSystemComponent*, const FParticleEventCollideData&);
DECLARE_DELEGATE_TYPE(FOnParticleSpawn, UParticleSystemComponent*, const FParticleEventData&);
DECLARE_DELEGATE_TYPE(FOnParticleDeath, UParticleSystemComponent*, const FParticleEventData&);

/**
 * AParticleEventManager
 * 월드 내 파티클 이벤트를 중앙에서 관리하고 브로드캐스트하는 매니저 액터입니다.
 *
 * 사용법:
 * 1. UWorld::GetParticleEventManager()로 매니저를 가져옵니다.
 * 2. OnParticleCollision/OnParticleSpawn/OnParticleDeath 델리게이트에 핸들러를 등록합니다.
 * 3. 파티클 시스템에서 이벤트가 발생하면 자동으로 브로드캐스트됩니다.
 */
UCLASS(NotSpawnable, DisplayName="파티클 이벤트 매니저", Description="파티클 이벤트를 중앙에서 관리하는 매니저 액터입니다")
class AParticleEventManager : public AActor
{
public:
	GENERATED_REFLECTION_BODY()

public:
	// 파티클 이벤트 델리게이트
	FOnParticleCollision OnParticleCollision;	// 충돌 이벤트
	FOnParticleSpawn OnParticleSpawn;			// 스폰 이벤트
	FOnParticleDeath OnParticleDeath;			// 사망 이벤트

	AParticleEventManager();
	virtual ~AParticleEventManager() = default;

	// 이벤트 브로드캐스트 함수
	void BroadcastCollisionEvent(UParticleSystemComponent* PSC, const FParticleEventCollideData& Event);
	void BroadcastSpawnEvent(UParticleSystemComponent* PSC, const FParticleEventData& Event);
	void BroadcastDeathEvent(UParticleSystemComponent* PSC, const FParticleEventData& Event);

	// 등록된 파티클 시스템 컴포넌트에서 이벤트 수집 및 브로드캐스트
	void ProcessPendingEvents();

	// 파티클 시스템 컴포넌트 등록/해제
	void RegisterParticleSystemComponent(UParticleSystemComponent* PSC);
	void UnregisterParticleSystemComponent(UParticleSystemComponent* PSC);

	// 틱에서 이벤트 처리
	virtual void Tick(float DeltaSeconds) override;

private:
	// 등록된 파티클 시스템 컴포넌트 목록
	TArray<UParticleSystemComponent*> RegisteredPSCs;
};
