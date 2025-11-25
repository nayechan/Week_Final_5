#pragma once

#include "Vector.h"

// Forward declarations
class UShapeComponent;
class AActor;

// 파티클 이벤트 타입 (언리얼 엔진 호환)
enum class EParticleEventType : uint8
{
	Any,        // 모든 이벤트
	Spawn,      // 파티클 생성 시
	Death,      // 파티클 소멸 시
	Collision,  // 충돌 시
	Burst       // 버스트 스폰 시
};

// 충돌 완료 시 동작 옵션 (언리얼 엔진 호환)
enum class EParticleCollisionComplete : uint8
{
	HaltCollisions,  // 충돌 검사 중단 (더 이상 충돌 검사 안함)
	Freeze,          // 파티클 프리즈 (위치 고정)
	Kill             // 파티클 제거
};

// 기본 파티클 이벤트 데이터 (언리얼 엔진 호환)
struct FParticleEventData
{
	EParticleEventType Type = EParticleEventType::Any;
	float EmitterTime = 0.0f;          // 이벤트 발생 시 이미터 시간
	FVector Position = FVector(0.0f, 0.0f, 0.0f);
	FVector Velocity = FVector(0.0f, 0.0f, 0.0f);
	FVector Direction = FVector(0.0f, 0.0f, 0.0f);
	int32 ParticleIndex = -1;          // 이벤트를 발생시킨 파티클 인덱스

	FParticleEventData() = default;
};

// 충돌 이벤트 데이터 (언리얼 엔진 호환)
struct FParticleEventCollideData : public FParticleEventData
{
	FVector Normal = FVector(0.0f, 0.0f, 0.0f);     // 충돌 표면 법선
	UShapeComponent* HitComponent = nullptr;  // 충돌한 ShapeComponent
	AActor* HitActor = nullptr;               // 충돌한 Actor
	float Time = 0.0f;                        // 충돌 시간 (0~1, 프레임 내 위치)

	FParticleEventCollideData()
	{
		Type = EParticleEventType::Collision;
	}
};

// 충돌 검사 결과 구조체
struct FParticleCollisionResult
{
	bool bHit = false;                        // 충돌 여부
	FVector HitLocation = FVector(0.0f, 0.0f, 0.0f); // 충돌 위치
	FVector HitNormal = FVector(0.0f, 0.0f, 0.0f);   // 충돌 법선
	float Time = 0.0f;                         // 충돌 시간 (0~1)
	UShapeComponent* HitComponent = nullptr;   // 충돌한 컴포넌트
};

// 이벤트 생성 설정 (EventGenerator 모듈용)
struct FParticleEvent
{
	EParticleEventType Type = EParticleEventType::Collision;
	FString EventName;  // 다른 이미터가 수신할 이벤트명

	FParticleEvent() = default;
	FParticleEvent(EParticleEventType InType, const FString& InName)
		: Type(InType), EventName(InName) {}
};
