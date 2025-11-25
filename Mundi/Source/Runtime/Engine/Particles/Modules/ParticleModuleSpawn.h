#pragma once

#include "ParticleModule.h"
#include "Distribution.h"
#include "UParticleModuleSpawn.generated.h"

// Forward declaration
class FParticleEmitterInstance;

// 언리얼 엔진 호환: 버스트 정보 구조체
struct FParticleBurst
{
	int32 Count;      // 버스트 시 생성할 파티클 개수
	int32 CountLow;   // 랜덤 범위 최소값 (-1이면 Count 사용, 아니면 CountLow~Count 랜덤)
	float Time;       // 버스트 발생 시점 (0..1: 이미터 수명 비율, Duration=0이면 절대 초)

	FParticleBurst() : Count(0), CountLow(-1), Time(0.0f) {}
	FParticleBurst(int32 InCount, float InTime) : Count(InCount), CountLow(-1), Time(InTime) {}
	FParticleBurst(int32 InCount, int32 InCountLow, float InTime) : Count(InCount), CountLow(InCountLow), Time(InTime) {}

	void Serialize(bool bIsLoading, JSON& InOutHandle);
};

UCLASS(DisplayName="스폰 모듈", Description="파티클의 생성 빈도와 수량을 제어하는 모듈입니다")
class UParticleModuleSpawn : public UParticleModule
{
public:
	GENERATED_REFLECTION_BODY()

public:
	// 초당 파티클 스폰 비율 (Distribution 시스템)
	UPROPERTY(EditAnywhere, Category="Spawn")
	FDistributionFloat SpawnRate = FDistributionFloat(10.0f);

	// 버스트 목록 (여러 시점에 버스트 가능)
	UPROPERTY(EditAnywhere, Category="Burst")
	TArray<FParticleBurst> BurstList;

	// 버스트 스케일 (모든 버스트에 곱해지는 계수, LOD/강도 조절용)
	UPROPERTY(EditAnywhere, Category="Burst")
	FDistributionFloat BurstScale = FDistributionFloat(1.0f);

	UParticleModuleSpawn()
	{
		// 스폰 모듈은 항상 bSpawnModule = true로 설정되어야
		// CacheModuleInfo()에서 SpawnModules 배열에 추가됨
		bSpawnModule = true;
	}
	virtual ~UParticleModuleSpawn() = default;

	// 언리얼 엔진 호환: 스폰 계산 로직을 모듈로 캡슐화
	// 반환값: 이번 프레임에 생성할 파티클 수
	// InOutSpawnFraction: 누적된 스폰 분수 (부드러운 스폰용)
	// (BurstFired 상태는 FParticleEmitterInstance에서 관리)
	int32 CalculateSpawnCount(FParticleEmitterInstance* Owner, float DeltaTime, float& InOutSpawnFraction);

	virtual void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;

	// Spawn은 세 번째로 표시 (우선순위 2)
	virtual int32 GetDisplayPriority() const override { return 2; }
};
