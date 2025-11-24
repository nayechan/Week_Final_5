#pragma once

#include "ParticleDefinitions.h"
#include "ParticleHelper.h"
#include "ParticleEmitter.h"
#include "ParticleRandomStream.h"

class UParticleSystemComponent;

struct FParticleEmitterInstance
{
	// 템플릿 이미터
	UParticleEmitter* SpriteTemplate;

	// 소유 컴포넌트
	UParticleSystemComponent* Component;

	// 현재 LOD 레벨
	int32 CurrentLODLevelIndex;
	UParticleLODLevel* CurrentLODLevel;

	// 언리얼 엔진 호환: 파티클 데이터 컨테이너 (16바이트 정렬)
	FParticleDataContainer ParticleDataContainer;

	// 파티클 데이터 (FParticleDataContainer를 통해 할당됨)
	/** 파티클 데이터 배열에 대한 포인터 */
	uint8* ParticleData;
	/** 파티클 인덱스 배열에 대한 포인터 */
	uint16* ParticleIndices;
	/** 인스턴스 데이터 배열에 대한 포인터 */
	uint8* InstanceData;
	/** 인스턴스 데이터 배열의 크기 */
	int32 InstancePayloadSize;
	/** 파티클 데이터의 오프셋 */
	int32 PayloadOffset;
	/** 파티클의 총 크기 (in bytes) */
	int32 ParticleSize;
	/** ParticleData 배열안의 파티클들 간의 stride */
	int32 ParticleStride;
	/** 현재 이미터에서 활성화된 파티클들의 수 */
	int32 ActiveParticles;
	/** Monotonically increasing counter. */
	uint32 ParticleCounter;
	/** 파티클 데이터배열에 저장할 수 있는 최대 파티클 활성 수 */
	int32 MaxActiveParticles;

	// 스폰 분수 (부드러운 스폰을 위함)
	float SpawnFraction;

	// Burst 상태 (언리얼 엔진 호환)
	bool bBurstFired;  // Burst가 이미 발생했는지 여부

	// 랜덤 스트림 (언리얼 엔진 호환)
	FParticleRandomStream RandomStream;

	// 언리얼 엔진 호환: 이미터 타이밍 상태
	float EmitterTime;               // 이미터가 시작된 이후 경과 시간 (초)
	float SecondsSinceCreation;      // 이미터 인스턴스 생성 이후 총 경과 시간 (루프에도 리셋 안됨)
	float EmitterDurationActual;     // 실제로 선택된 지속 시간 (랜덤 범위에서 선택)
	float EmitterDelayActual;        // 실제로 선택된 딜레이 (랜덤 범위에서 선택)
	bool bDelayComplete;             // 초기 딜레이가 끝났는지 여부
	int32 CurrentLoopCount;          // 현재 루프 횟수
	bool bEmitterEnabled;            // 이미터가 활성화되어 있는지 (Duration 만료 시 false)

	// 언리얼 엔진 호환: 이미터 트랜스폼 캐시
	FVector CachedEmitterOrigin;     // Required 모듈의 EmitterOrigin 캐시
	FVector CachedEmitterRotation;   // Required 모듈의 EmitterRotation 캐시 (Euler angles)
	FMatrix EmitterToWorld;          // 이미터 회전 변환 행렬 (파티클 속도 회전용)

	// 생성자 / 소멸자
	FParticleEmitterInstance();
	virtual ~FParticleEmitterInstance();

	// 이미터 인스턴스 초기화
	void Init(UParticleSystemComponent* InComponent, UParticleEmitter* InTemplate);

	// LOD 레벨 전환 
	void SetLODLevel(int32 NewLODIndex);

	// 현재 LOD 기반 이미터 세팅
	void SetupEmitter();

	// 이미터 인스턴스 업데이트
	void Tick(float DeltaTime, bool bSuppressSpawning);

	// 파티클 생성
	void SpawnParticles(int32 Count, float StartTime, float Increment, const FVector& InitialLocation, const FVector& InitialVelocity);

	// 인덱스의 파티클 제거
	void KillParticle(int32 Index);

	// 모든 파티클 제거
	void KillAllParticles();

	// 파티클 데이터 크기 조정
	void Resize(int32 NewMaxActiveParticles);

	// 파티클 사전 생성
	void PreSpawn(FBaseParticle* Particle, const FVector& InitialLocation, const FVector& InitialVelocity);

	// 파티클 사후 생성
	void PostSpawn(FBaseParticle* Particle, float InterpolationParameter, float SpawnTime);

	// 파티클 업데이트
	void UpdateParticles(float DeltaTime);

	// 인덱스의 파티클 가져오기
	FBaseParticle* GetParticleAtIndex(int32 Index);

	// 렌더링을 위한 동적 데이터 생성
	FDynamicEmitterDataBase* GetDynamicData(bool bSelected);
};

// 언리얼 엔진 호환: 인덱스로 파티클을 가져오는 헬퍼 함수 구현
// ParticleHelper.h에서 선언된 함수의 구현 (순환 의존성 방지)
inline FBaseParticle* GetParticleAtIndex(FParticleEmitterInstance* Instance, int32 Index)
{
	if (!Instance || Index < 0 || Index >= Instance->ActiveParticles)
	{
		return nullptr;
	}

	// 간접 인덱싱: ParticleIndices를 통해 실제 파티클 위치 가져오기
	int32 ParticleIndex = Instance->ParticleIndices[Index];
	uint8* ParticleBase = Instance->ParticleData + (ParticleIndex * Instance->ParticleStride);
	return reinterpret_cast<FBaseParticle*>(ParticleBase);
}
