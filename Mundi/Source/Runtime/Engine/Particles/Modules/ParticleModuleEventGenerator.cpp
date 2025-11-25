#include "pch.h"
#include "ParticleModuleEventGenerator.h"
#include "ParticleEmitterInstance.h"
#include "ParticleSystemComponent.h"

void FParticleEventGeneratorInfo::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	if (bInIsLoading)
	{
		int32 TypeValue = 0;
		FJsonSerializer::ReadInt32(InOutHandle, "Type", TypeValue);
		Type = static_cast<EParticleEventType>(TypeValue);
		FJsonSerializer::ReadInt32(InOutHandle, "Frequency", Frequency);
		FJsonSerializer::ReadString(InOutHandle, "CustomName", CustomName);
		FJsonSerializer::ReadBool(InOutHandle, "bFirstSpawnOnly", bFirstSpawnOnly);
		FJsonSerializer::ReadBool(InOutHandle, "bNaturalDeathOnly", bNaturalDeathOnly);
		FJsonSerializer::ReadBool(InOutHandle, "bFirstCollisionOnly", bFirstCollisionOnly);
	}
	else
	{
		InOutHandle["Type"] = static_cast<int32>(Type);
		InOutHandle["Frequency"] = Frequency;
		InOutHandle["CustomName"] = CustomName;
		InOutHandle["bFirstSpawnOnly"] = bFirstSpawnOnly;
		InOutHandle["bNaturalDeathOnly"] = bNaturalDeathOnly;
		InOutHandle["bFirstCollisionOnly"] = bFirstCollisionOnly;
	}
}

void UParticleModuleEventGenerator::Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle* ParticleBase)
{
	if (!ParticleBase || !Owner || !Owner->Component)
	{
		return;
	}

	// 스폰 이벤트 생성 정보 찾기
	FParticleEventGeneratorInfo* SpawnEventInfo = GetEventInfo(EParticleEventType::Spawn);
	if (!SpawnEventInfo)
	{
		return;
	}

	// 첫 스폰만 처리하는 옵션 체크
	if (SpawnEventInfo->bFirstSpawnOnly && Owner->ActiveParticles > 1)
	{
		return;
	}

	// 이벤트 데이터 생성
	FParticleEventData Event = CreateEventData(EParticleEventType::Spawn, *ParticleBase, Owner->EmitterTime);

	// 이벤트 추가
	Owner->Component->AddSpawnEvent(Event);
}

void UParticleModuleEventGenerator::Update(FModuleUpdateContext& Context)
{
	UParticleSystemComponent* PSC = Context.Owner.Component;
	if (!PSC)
	{
		return;
	}

	// 사망 이벤트 설정 확인
	FParticleEventGeneratorInfo* DeathEventInfo = GetEventInfo(EParticleEventType::Death);

	// 충돌 이벤트 설정 확인
	FParticleEventGeneratorInfo* CollisionEventInfo = GetEventInfo(EParticleEventType::Collision);

	// 이벤트 생성할 것이 없으면 스킵
	if (!DeathEventInfo && !CollisionEventInfo)
	{
		return;
	}

	int32 Offset = Context.Offset;

	BEGIN_UPDATE_LOOP
		// 사망 이벤트 체크 (RelativeTime >= 1.0이면 사망 예정)
		if (DeathEventInfo && Particle.RelativeTime >= 1.0f)
		{
			// 자연 사망만 체크하는 옵션
			if (DeathEventInfo->bNaturalDeathOnly)
			{
				// 파티클이 자연 사망인지 확인 (특별한 플래그 없이 시간만료된 경우)
				// 일반적으로 Collision Kill 등은 RelativeTime을 1.1로 설정하므로
				// 정확히 1.0에 가까우면 자연 사망으로 간주
				if (Particle.RelativeTime > 1.05f)
				{
					continue;  // 강제 Kill로 인한 사망은 스킵
				}
			}

			FParticleEventData Event = CreateEventData(EParticleEventType::Death, Particle, Context.Owner.EmitterTime);
			PSC->AddDeathEvent(Event);
		}

		// 충돌 이벤트 체크 (충돌 모듈이 플래그를 설정했는지 확인)
		if (CollisionEventInfo && (Particle.Flags & STATE_Particle_CollisionHasOccurred))
		{
			// 첫 충돌만 처리하는 옵션
			if (CollisionEventInfo->bFirstCollisionOnly)
			{
				// 이미 이전에 충돌 이벤트가 발생했다면 스킵
				// (이 플래그는 한 번 설정되면 유지되므로, 처음 설정될 때만 이벤트 생성)
				// 주의: 충돌 모듈에서 이미 CollisionEvent를 추가했으므로
				// 여기서는 추가적인 처리가 필요한 경우에만 사용
			}

			// 충돌 이벤트는 UParticleModuleCollision에서 직접 생성하므로
			// 여기서는 추가 처리만 수행 (필요시 확장 가능)
		}
	END_UPDATE_LOOP
}

void UParticleModuleEventGenerator::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	UParticleModule::Serialize(bInIsLoading, InOutHandle);

	if (bInIsLoading)
	{
		Events.Empty();
		JSON EventsJson;
		if (FJsonSerializer::ReadArray(InOutHandle, "Events", EventsJson))
		{
			for (uint32 i = 0; i < static_cast<uint32>(EventsJson.size()); ++i)
			{
				JSON EventJson = EventsJson.at(i);
				FParticleEventGeneratorInfo Info;
				Info.Serialize(true, EventJson);
				Events.Add(Info);
			}
		}
	}
	else
	{
		JSON EventsJson = JSON::Make(JSON::Class::Array);
		for (int32 i = 0; i < Events.Num(); ++i)
		{
			JSON EventJson;
			Events[i].Serialize(false, EventJson);
			EventsJson.append(EventJson);
		}
		InOutHandle["Events"] = EventsJson;
	}
}

FParticleEventGeneratorInfo* UParticleModuleEventGenerator::GetEventInfo(EParticleEventType Type)
{
	for (FParticleEventGeneratorInfo& Info : Events)
	{
		if (Info.Type == Type || Info.Type == EParticleEventType::Any)
		{
			return &Info;
		}
	}
	return nullptr;
}

FParticleEventData UParticleModuleEventGenerator::CreateEventData(EParticleEventType Type, const FBaseParticle& Particle, float EmitterTime)
{
	FParticleEventData Event;
	Event.Type = Type;
	Event.Position = Particle.Location;
	Event.Velocity = Particle.Velocity;
	Event.Direction = Particle.Velocity.GetSafeNormal();
	Event.EmitterTime = EmitterTime;
	return Event;
}
