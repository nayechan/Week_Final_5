#pragma once

#include "Object.h"
#include "ParticleHelper.h"
#include "UParticleModule.generated.h"

struct FParticleEmitterInstance;

UCLASS(DisplayName="파티클 모듈", Description="파티클 이미터의 동작을 정의하는 베이스 모듈입니다")
class UParticleModule : public UObject
{
public:
	GENERATED_REFLECTION_BODY()

public:
	UPROPERTY()
	bool bEnabled = true;

	// 언리얼 엔진 호환: 모듈 타입 플래그
	UPROPERTY()
	bool bSpawnModule = false;  // 스폰 시 호출되는 모듈

	UPROPERTY()
	bool bUpdateModule = false;  // 매 프레임 업데이트 시 호출되는 모듈

	// 언리얼 엔진 호환: 페이로드 시스템
	// 이 모듈의 데이터가 파티클 페이로드 내에서 시작하는 오프셋
	uint32 ModuleOffsetInParticle = 0;

	UParticleModule() = default;
	virtual ~UParticleModule() = default;

	// 언리얼 엔진 호환: 모듈 초기화 및 기본값 설정
	virtual void InitializeDefaults()
	{
		// 파생 클래스에서 오버라이드하여 기본값 설정
	}

	// 이미터 인스턴스가 생성될 때 한 번 호출
	virtual void Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle* ParticleBase)
	{
		// 파생 클래스에서 오버라이드
	}

	// 매 프레임마다 파티클 업데이트를 위해 호출 (언리얼 엔진 호환: Context 사용)
	virtual void Update(FModuleUpdateContext& Context)
	{
		// 파생 클래스에서 오버라이드
	}

	// 언리얼 엔진 호환: 페이로드 시스템
	// 이 모듈이 파티클별로 필요로 하는 추가 데이터 크기를 반환
	virtual uint32 RequiredBytes(FParticleEmitterInstance* Owner = nullptr)
	{
		return 0;  // 기본적으로 추가 데이터 불필요
	}

	// 언리얼 엔진 호환: 이 모듈이 인스턴스별로 필요로 하는 추가 데이터 크기를 반환
	virtual uint32 RequiredBytesPerInstance()
	{
		return 0;  // 기본적으로 추가 데이터 불필요
	}

	// 직렬화
	virtual void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;

	// 에디터 표시 우선순위 (낮을수록 먼저 표시)
	// TypeData: 0, Required: 1, Spawn: 2, 일반 모듈: 100
	virtual int32 GetDisplayPriority() const { return 100; }

	// LOD 스케일링: 하위 LOD 생성 시 값들을 Multiplier로 스케일
	// 파생 클래스에서 오버라이드하여 SpawnRate, BurstCount 등을 조정
	virtual void ScaleForLOD(float Multiplier) {}
};
