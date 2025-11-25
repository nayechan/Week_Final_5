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

	// ===== 소유 데이터: 모든 모듈의 단일 소유자 =====
	UPROPERTY(EditAnywhere, Category="Modules")
	TArray<UParticleModule*> Modules;

	// ===== 캐시 포인터: Modules 배열 내 객체 참조 (직접 삭제 금지) =====
	// CacheModuleInfo()에서 Modules 배열을 순회하여 설정됨
	UParticleModuleRequired* RequiredModule = nullptr;       // 필수 모듈 (항상 존재)
	UParticleModuleTypeDataBase* TypeDataModule = nullptr;   // 타입 데이터 모듈 (스프라이트/메시)
	UParticleModuleSpawn* SpawnModule = nullptr;             // 스폰 모듈

	// 캐시된 모듈 배열 (직렬화하면 안 됨)
	TArray<UParticleModule*> SpawnModules;
	TArray<UParticleModule*> UpdateModules;

	UParticleLODLevel() = default;
	virtual ~UParticleLODLevel();

	// 모듈 정보 캐싱
	void CacheModuleInfo();

	// 직렬화
	virtual void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;

	// 복제
	virtual void DuplicateSubObjects() override;
};
