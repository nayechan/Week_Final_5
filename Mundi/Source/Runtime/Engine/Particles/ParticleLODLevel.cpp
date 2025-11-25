#include "pch.h"
#include "ParticleLODLevel.h"
#include "ObjectFactory.h"
#include "JsonSerializer.h"
#include "Modules/ParticleModuleRequired.h"
#include "Modules/ParticleModuleTypeDataBase.h"

UParticleLODLevel::~UParticleLODLevel()
{
	// Modules 배열의 모든 모듈 삭제 (단일 소유권)
	for (UParticleModule* Module : Modules)
	{
		if (Module)
		{
			DeleteObject(Module);
		}
	}
	Modules.Empty();

	// 캐시 포인터 정리 (소유권 없음)
	RequiredModule = nullptr;
	TypeDataModule = nullptr;
	SpawnModule = nullptr;
	SpawnModules.Empty();
	UpdateModules.Empty();
}

void UParticleLODLevel::CacheModuleInfo()
{
	// 캐시 초기화
	RequiredModule = nullptr;
	TypeDataModule = nullptr;
	SpawnModule = nullptr;
	SpawnModules.clear();
	UpdateModules.clear();

	// Modules 배열에서 특수 모듈 찾기
	for (UParticleModule* Module : Modules)
	{
		if (!Module) continue;

		// RequiredModule 캐시
		if (UParticleModuleRequired* Required = Cast<UParticleModuleRequired>(Module))
		{
			if (RequiredModule != nullptr)
			{
				UE_LOG("[ParticleLODLevel] WARNING: 중복 RequiredModule 발견! 첫 번째 모듈 사용");
			}
			else
			{
				RequiredModule = Required;
			}
		}
		// TypeDataModule 캐시 (단일 인스턴스 제한)
		else if (UParticleModuleTypeDataBase* TypeData = Cast<UParticleModuleTypeDataBase>(Module))
		{
			if (TypeDataModule != nullptr)
			{
				UE_LOG("[ParticleLODLevel] WARNING: 중복 TypeDataModule 발견! LOD당 1개만 허용됩니다. 첫 번째 모듈 사용");
			}
			else
			{
				TypeDataModule = TypeData;
			}
		}
		// SpawnModule 캐시
		else if (UParticleModuleSpawn* Spawn = Cast<UParticleModuleSpawn>(Module))
		{
			if (SpawnModule != nullptr)
			{
				UE_LOG("[ParticleLODLevel] WARNING: 중복 SpawnModule 발견! 첫 번째 모듈 사용");
			}
			else
			{
				SpawnModule = Spawn;
			}
		}

		// Spawn/Update 모듈 리스트 구성
		if (Module->bEnabled)
		{
			if (Module->bSpawnModule)
			{
				SpawnModules.Add(Module);
			}
			if (Module->bUpdateModule)
			{
				UpdateModules.Add(Module);
			}
		}
	}

	// RequiredModule이 없으면 생성 후 Modules에 추가
	if (RequiredModule == nullptr)
	{
		UE_LOG("[ParticleLODLevel] RequiredModule 없음, 기본 모듈 생성");
		RequiredModule = ObjectFactory::NewObject<UParticleModuleRequired>();
		Modules.Insert(RequiredModule, 0);
	}
}

void UParticleLODLevel::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	// 기본 타입 UPROPERTY 자동 직렬화 (Level, bEnabled)
	UObject::Serialize(bInIsLoading, InOutHandle);

	if (bInIsLoading)
	{
		// === Modules 배열 로드 (모든 모듈 포함) ===
		JSON ModulesJson;
		if (FJsonSerializer::ReadArray(InOutHandle, "Modules", ModulesJson))
		{
			Modules.Empty();
			for (size_t i = 0; i < ModulesJson.size(); ++i)
			{
				JSON& ModuleData = ModulesJson.at(i);
				FString TypeString;
				FJsonSerializer::ReadString(ModuleData, "Type", TypeString);

				UClass* ModuleClass = UClass::FindClass(TypeString);
				if (ModuleClass)
				{
					UParticleModule* Module = Cast<UParticleModule>(ObjectFactory::NewObject(ModuleClass));
					if (Module)
					{
						Module->Serialize(true, ModuleData);
						Modules.Add(Module);
					}
				}
			}
		}

		// 캐시 포인터 설정 (RequiredModule, TypeDataModule, SpawnModule)
		CacheModuleInfo();
	}
	else
	{
		// 저장 전 RequiredModule/TypeDataModule이 Modules에 있는지 확인
		bool bHasRequired = false;
		bool bHasTypeData = false;
		for (UParticleModule* Module : Modules)
		{
			if (Cast<UParticleModuleRequired>(Module)) bHasRequired = true;
			if (Cast<UParticleModuleTypeDataBase>(Module)) bHasTypeData = true;
		}
		if (!bHasRequired && RequiredModule) Modules.Insert(RequiredModule, 0);
		if (!bHasTypeData && TypeDataModule) Modules.Add(TypeDataModule);

		// === Modules 배열 저장 (Type 필드 포함) ===
		JSON ModulesJson = JSON::Make(JSON::Class::Array);
		for (UParticleModule* Module : Modules)
		{
			if (Module)
			{
				JSON ModuleData = JSON::Make(JSON::Class::Object);
				ModuleData["Type"] = Module->GetClass()->Name;
				Module->Serialize(false, ModuleData);
				ModulesJson.append(ModuleData);
			}
		}
		InOutHandle["Modules"] = ModulesJson;
	}
}

void UParticleLODLevel::DuplicateSubObjects()
{
	UObject::DuplicateSubObjects();

	// Modules 배열만 복제 (단일 소유권)
	TArray<UParticleModule*> NewModules;
	for (UParticleModule* Module : Modules)
	{
		if (Module)
		{
			UParticleModule* NewModule = ObjectFactory::DuplicateObject<UParticleModule>(Module);
			NewModules.Add(NewModule);
		}
	}
	Modules = NewModules;

	// 캐시 포인터 재설정 (RequiredModule, TypeDataModule, SpawnModule)
	CacheModuleInfo();
}
