#pragma once

#include "Object.h"
#include "Modules/ParticleModule.h"
#include "Modules/ParticleModuleRequired.h"
#include "Modules/ParticleModuleTypeDataBase.h"
#include "Modules/ParticleModuleSpawn.h"
#include "UParticleLODLevel.generated.h"

UCLASS(DisplayName="파티클 LOD 레벨", Description="파티클 이미터의 LOD 레벨 설정입니다")
class UParticleLODLevel : public UObject
{
public:
	GENERATED_REFLECTION_BODY()

public:
	UPROPERTY(EditAnywhere, Category="LOD")
	int32 Level = 0;

	UPROPERTY(EditAnywhere, Category="LOD")
	bool bEnabled = true;

	// 언리얼 엔진 호환: 필수 모듈 (항상 존재)
	UPROPERTY(EditAnywhere, Category="Modules")
	UParticleModuleRequired* RequiredModule = nullptr;

	// 언리얼 엔진 호환: 파티클 동작을 수정하는 선택적 모듈
	UPROPERTY(EditAnywhere, Category="Modules")
	TArray<UParticleModule*> Modules;

	// 언리얼 엔진 호환: 선택적 타입 데이터 모듈 (스프라이트 vs 메시 결정)
	UPROPERTY(EditAnywhere, Category="Modules")
	UParticleModuleTypeDataBase* TypeDataModule = nullptr;

	// 언리얼 엔진 호환: SpawnRate/Burst 모듈 - 모든 이미터에 필수
	UPROPERTY(EditAnywhere, Category="Modules")
	UParticleModuleSpawn* SpawnModule = nullptr;

	// 언리얼 엔진 호환: 빠른 접근을 위한 캐시된 모듈 배열 (직렬화하면 안 됨)
	TArray<UParticleModule*> SpawnModules;
	TArray<UParticleModule*> UpdateModules;

	UParticleLODLevel() = default;
	virtual ~UParticleLODLevel() = default;

	// 모듈 정보 캐싱
	void CacheModuleInfo();

	// 직렬화
	virtual void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;

	// 복제
	virtual void DuplicateSubObjects() override;
};
