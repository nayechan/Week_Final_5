#pragma once

#include "ParticleModule.h"
#include "ParticleEventTypes.h"
#include "UParticleModuleEventGenerator.generated.h"

/**
 * 이벤트 생성 설정 (하나의 이벤트 타입에 대한 설정)
 */
struct FParticleEventGeneratorInfo
{
	// 생성할 이벤트 타입
	EParticleEventType Type = EParticleEventType::Any;

	// 이벤트 빈도 제한 (0 = 무제한)
	int32 Frequency = 0;

	// 이벤트를 받을 이미터 이름 (빈 문자열이면 전체 브로드캐스트)
	FString CustomName;

	// 스폰 이벤트: 첫 스폰 시에만 발생할지 여부
	bool bFirstSpawnOnly = false;

	// 사망 이벤트: 라이프타임 종료 시에만 발생할지 (Kill에 의한 사망 제외)
	bool bNaturalDeathOnly = false;

	// 충돌 이벤트: 첫 충돌 시에만 발생할지 여부
	bool bFirstCollisionOnly = true;

	void Serialize(const bool bInIsLoading, JSON& InOutHandle);
};

/**
 * UParticleModuleEventGenerator
 *
 * 파티클 이벤트를 생성하는 모듈입니다.
 * 이 모듈은 파티클의 Spawn, Death, Collision 이벤트를 생성하여
 * 다른 이미터나 외부 시스템에서 반응할 수 있도록 합니다.
 *
 * 사용법:
 * 1. 이미터에 이 모듈을 추가합니다.
 * 2. Events 배열에 생성할 이벤트 타입을 설정합니다.
 * 3. EventReceiver 모듈이 있는 다른 이미터에서 이벤트를 수신합니다.
 */
UCLASS(DisplayName="이벤트 생성 모듈", Description="파티클 이벤트(Spawn/Death/Collision)를 생성하는 모듈입니다")
class UParticleModuleEventGenerator : public UParticleModule
{
public:
	GENERATED_REFLECTION_BODY()

public:
	// 생성할 이벤트 목록
	TArray<FParticleEventGeneratorInfo> Events;

	UParticleModuleEventGenerator()
	{
		bSpawnModule = true;   // 스폰 이벤트 생성을 위해
		bUpdateModule = true;  // 사망/충돌 이벤트 체크를 위해
	}

	virtual ~UParticleModuleEventGenerator() = default;

	// 스폰 시 이벤트 생성
	virtual void Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle* ParticleBase) override;

	// 업데이트 시 사망/충돌 이벤트 체크
	virtual void Update(FModuleUpdateContext& Context) override;

	// 직렬화
	virtual void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;

private:
	// 특정 타입의 이벤트 생성 정보 가져오기
	FParticleEventGeneratorInfo* GetEventInfo(EParticleEventType Type);

	// 이벤트 데이터 생성 헬퍼
	FParticleEventData CreateEventData(EParticleEventType Type, const FBaseParticle& Particle, float EmitterTime);
};
