#include "pch.h"
#include "ParticleEmitter.h"
#include "ObjectFactory.h"
#include "JsonSerializer.h"

UParticleEmitter::~UParticleEmitter()
{
	// 모든 LOD 레벨 삭제
	for (UParticleLODLevel* LODLevel : LODLevels)
	{
		if (LODLevel)
		{
			DeleteObject(LODLevel);
		}
	}
	LODLevels.Empty();
}

void UParticleEmitter::CacheEmitterModuleInfo()
{
	// 기본 파티클 크기 (페이로드 제외)
	ParticleSize = sizeof(FBaseParticle);

	// 페이로드 크기는 FParticleEmitterInstance::InitializeParticles()에서
	// 현재 LOD의 모듈에 따라 런타임에 계산됨 (언리얼 방식)

	// 모든 LOD 레벨의 모듈 정보 캐싱
	for (UParticleLODLevel* LODLevel : LODLevels)
	{
		if (LODLevel)
		{
			LODLevel->CacheModuleInfo();
		}
	}
}

UParticleLODLevel* UParticleEmitter::GetLODLevel(int32 LODIndex) const
{
	if (LODIndex >= 0 && LODIndex < LODLevels.size())
	{
		return LODLevels[LODIndex];
	}
	return nullptr;
}

UParticleLODLevel* UParticleEmitter::DuplicateLODLevel(int32 SourceLODIndex, float ScaleMultiplier)
{
	UParticleLODLevel* SourceLOD = GetLODLevel(SourceLODIndex);
	if (!SourceLOD)
	{
		return nullptr;
	}

	// 코드 생성기가 자동 생성한 Duplicate() 사용
	// 내부적으로 DuplicateSubObjects() -> PostDuplicate() 호출됨
	UParticleLODLevel* NewLOD = SourceLOD->Duplicate();
	if (!NewLOD)
	{
		return nullptr;
	}

	// ScaleMultiplier 적용 (1.0이 아닌 경우에만)
	if (ScaleMultiplier != 1.0f)
	{
		for (UParticleModule* Module : NewLOD->Modules)
		{
			if (Module)
			{
				Module->ScaleForLOD(ScaleMultiplier);
			}
		}
	}

	return NewLOD;
}

bool UParticleEmitter::RemoveLODLevel(int32 LODIndex)
{
	// 최소 1개의 LOD는 유지해야 함
	if (LODLevels.Num() <= 1)
	{
		return false;
	}

	if (LODIndex < 0 || LODIndex >= LODLevels.Num())
	{
		return false;
	}

	// LOD 삭제
	UParticleLODLevel* ToDelete = LODLevels[LODIndex];
	LODLevels.RemoveAt(LODIndex);

	if (ToDelete)
	{
		DeleteObject(ToDelete);
	}

	// 남은 LOD들의 Level 인덱스 재정렬
	for (int32 i = 0; i < LODLevels.Num(); ++i)
	{
		if (LODLevels[i])
		{
			LODLevels[i]->Level = i;
		}
	}

	return true;
}

void UParticleEmitter::InsertLODLevel(int32 InsertIndex, UParticleLODLevel* NewLOD)
{
	if (!NewLOD)
	{
		return;
	}

	// 인덱스 클램프
	InsertIndex = FMath::Clamp(InsertIndex, 0, LODLevels.Num());

	// 삽입
	LODLevels.Insert(NewLOD, InsertIndex);

	// 모든 LOD의 Level 인덱스 재정렬
	for (int32 i = 0; i < LODLevels.Num(); ++i)
	{
		if (LODLevels[i])
		{
			LODLevels[i]->Level = i;
		}
	}

	// 캐시 갱신
	CacheEmitterModuleInfo();
}

void UParticleEmitter::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	// 기본 타입 UPROPERTY 자동 직렬화 (EmitterName 등)
	UObject::Serialize(bInIsLoading, InOutHandle);

	if (bInIsLoading)
	{
		// === LODLevels 배열 로드 ===
		JSON LODLevelsJson;
		if (FJsonSerializer::ReadArray(InOutHandle, "LODLevels", LODLevelsJson))
		{
			LODLevels.Empty();
			for (size_t i = 0; i < LODLevelsJson.size(); ++i)
			{
				JSON& LODData = LODLevelsJson.at(i);
				UParticleLODLevel* LOD = NewObject<UParticleLODLevel>();
				if (LOD)
				{
					LOD->Serialize(true, LODData);
					LODLevels.Add(LOD);
				}
			}
		}

		// 로딩 후 캐싱
		CacheEmitterModuleInfo();
	}
	else
	{
		// === LODLevels 배열 저장 ===
		JSON LODLevelsJson = JSON::Make(JSON::Class::Array);
		for (UParticleLODLevel* LOD : LODLevels)
		{
			if (LOD)
			{
				JSON LODData = JSON::Make(JSON::Class::Object);
				LOD->Serialize(false, LODData);
				LODLevelsJson.append(LODData);
			}
		}
		InOutHandle["LODLevels"] = LODLevelsJson;
	}
}

void UParticleEmitter::DuplicateSubObjects()
{
	UObject::DuplicateSubObjects();

	// 모든 LOD 레벨 복제
	TArray<UParticleLODLevel*> NewLODLevels;
	for (UParticleLODLevel* LODLevel : LODLevels)
	{
		if (LODLevel)
		{
			UParticleLODLevel* NewLOD = ObjectFactory::DuplicateObject<UParticleLODLevel>(LODLevel);
			if (NewLOD)
			{
				NewLOD->DuplicateSubObjects(); // 재귀적으로 서브 오브젝트 복제
				NewLODLevels.Add(NewLOD);
			}
		}
	}
	LODLevels = NewLODLevels;

	// 이미터 정보 재캐싱
	CacheEmitterModuleInfo();
}
