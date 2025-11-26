#pragma once

#include "ParticleEventTypes.h"
#include "FParticleEventGeneratorInfo.generated.h"

/**
 * FParticleEventGeneratorInfo - 이벤트 생성 설정 구조체
 * 하나의 이벤트 타입에 대한 생성 설정
 */
USTRUCT(DisplayName="이벤트 생성 정보", Description="파티클 이벤트 생성 설정")
struct FParticleEventGeneratorInfo
{
	GENERATED_REFLECTION_BODY()

public:
	// 생성할 이벤트 타입
	UPROPERTY(EditAnywhere, Category="Event", Tooltip="생성할 이벤트 타입")
	EParticleEventType Type = EParticleEventType::Any;

	// 이벤트 빈도 제한 (0 = 무제한)
	UPROPERTY(EditAnywhere, Category="Event", Tooltip="이벤트 빈도 제한 (0=무제한)")
	int32 Frequency = 0;

	// 이벤트 이름 (EventReceiver에서 이 이름으로 필터링하여 수신)
	UPROPERTY(EditAnywhere, Category="Event", Tooltip="이벤트 이름 (EventReceiver에서 이 이름으로 수신)")
	FString EventName;

	// 스폰 이벤트: 첫 스폰 시에만 발생할지 여부
	UPROPERTY(EditAnywhere, Category="Spawn", Tooltip="[Spawn 전용] 첫 파티클 생성 시에만 이벤트 발생")
	bool bFirstSpawnOnly = false;

	// 사망 이벤트: 라이프타임 종료 시에만 발생할지 (Kill에 의한 사망 제외)
	UPROPERTY(EditAnywhere, Category="Death", Tooltip="[Death 전용] 수명 만료로 죽은 파티클만 이벤트 발생 (충돌 Kill 제외)")
	bool bNaturalDeathOnly = false;

	// 충돌 이벤트: 첫 충돌 시에만 발생할지 여부
	UPROPERTY(EditAnywhere, Category="Collision", Tooltip="[Collision 전용] 각 파티클의 첫 충돌에서만 이벤트 발생")
	bool bFirstCollisionOnly = true;

	void Serialize(const bool bInIsLoading, JSON& InOutHandle);
};
