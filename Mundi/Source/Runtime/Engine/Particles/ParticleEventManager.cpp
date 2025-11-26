#include "pch.h"
#include "ParticleEventManager.h"
#include "ParticleSystemComponent.h"

AParticleEventManager::AParticleEventManager()
{
	bCanEverTick = true;
	bTickInEditor = false;  // 에디터에서는 틱 불필요
	bHiddenInOutliner = true;  // 시스템 액터이므로 아웃라이너에서 숨김
}

void AParticleEventManager::BroadcastCollisionEvent(UParticleSystemComponent* PSC, const FParticleEventCollideData& Event)
{
	OnParticleCollision.Broadcast(PSC, Event);
}

void AParticleEventManager::BroadcastSpawnEvent(UParticleSystemComponent* PSC, const FParticleEventData& Event)
{
	OnParticleSpawn.Broadcast(PSC, Event);
}

void AParticleEventManager::BroadcastDeathEvent(UParticleSystemComponent* PSC, const FParticleEventData& Event)
{
	OnParticleDeath.Broadcast(PSC, Event);
}

void AParticleEventManager::ProcessPendingEvents()
{
	// 등록된 모든 파티클 시스템 컴포넌트의 이벤트 처리
	for (UParticleSystemComponent* PSC : RegisteredPSCs)
	{
		if (!PSC)
		{
			continue;
		}

		// 충돌 이벤트 브로드캐스트
		for (const FParticleEventCollideData& CollisionEvent : PSC->CollisionEvents)
		{
			BroadcastCollisionEvent(PSC, CollisionEvent);
		}

		// 스폰 이벤트 브로드캐스트
		for (const FParticleEventData& SpawnEvent : PSC->SpawnEvents)
		{
			BroadcastSpawnEvent(PSC, SpawnEvent);
		}

		// 사망 이벤트 브로드캐스트
		for (const FParticleEventData& DeathEvent : PSC->DeathEvents)
		{
			BroadcastDeathEvent(PSC, DeathEvent);
		}
	}
}

void AParticleEventManager::RegisterParticleSystemComponent(UParticleSystemComponent* PSC)
{
	if (PSC && !RegisteredPSCs.Contains(PSC))
	{
		RegisteredPSCs.Add(PSC);
	}
}

void AParticleEventManager::UnregisterParticleSystemComponent(UParticleSystemComponent* PSC)
{
	RegisteredPSCs.Remove(PSC);
}

void AParticleEventManager::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// 매 프레임 이벤트 처리
	ProcessPendingEvents();

	// 무효한 PSC 제거
	for (int32 i = RegisteredPSCs.Num() - 1; i >= 0; --i)
	{
		UParticleSystemComponent* PSC = RegisteredPSCs[i];
		if (PSC == nullptr || PSC->IsPendingDestroy())
		{
			RegisteredPSCs.RemoveAt(i);
		}
	}
}
