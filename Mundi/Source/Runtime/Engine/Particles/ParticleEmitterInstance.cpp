#include "pch.h"
#include "ParticleEmitterInstance.h"
#include "ParticleSystemComponent.h"
#include "Modules/ParticleModule.h"
#include "Modules/ParticleModuleSpawn.h"
#include "Modules/ParticleModuleMeshRotation.h"
#include "ParticleModuleTypeDataSprite.h"
#include "ParticleModuleTypeDataMesh.h"
#include "ParticleModuleTypeDataBeam.h"
#include "ParticleModuleTypeDataRibbon.h"

FParticleEmitterInstance::FParticleEmitterInstance()
	: SpriteTemplate(nullptr)
	, Component(nullptr)
	, CurrentLODLevelIndex(0)
	, CurrentLODLevel(nullptr)
	, ParticleData(nullptr)
	, ParticleIndices(nullptr)
	, InstanceData(nullptr)
	, InstancePayloadSize(0)
	, PayloadOffset(0)
	, ParticleSize(0)
	, ParticleStride(0)
	, ActiveParticles(0)
	, ParticleCounter(0)
	, MaxActiveParticles(0)
	, SpawnFraction(0.0f)
	, bBurstFired(false)  // 언리얼 엔진 호환: Burst 초기화
	, EmitterTime(0.0f)
	, SecondsSinceCreation(0.0f)
	, EmitterDurationActual(0.0f)
	, EmitterDelayActual(0.0f)
	, bDelayComplete(false)
	, CurrentLoopCount(0)
	, bEmitterEnabled(true)
	, CachedEmitterOrigin(0.0f, 0.0f, 0.0f)
	, CachedEmitterRotation(0.0f, 0.0f, 0.0f)
	, EmitterToWorld(FMatrix::Identity())
{
}

FParticleEmitterInstance::~FParticleEmitterInstance()
{
	// 언리얼 엔진 호환: FParticleDataContainer가 자동으로 메모리 해제
	// ParticleDataContainer.Free()는 소멸자에서 자동 호출됨
	ParticleData = nullptr;
	ParticleIndices = nullptr;

	if (InstanceData)
	{
		delete[] InstanceData;
		InstanceData = nullptr;
	}
}

void FParticleEmitterInstance::Init(UParticleSystemComponent* InComponent, UParticleEmitter* InTemplate)
{
	Component = InComponent;
	SpriteTemplate = InTemplate;

	// 필수 객체 nullptr 체크
	if (!SpriteTemplate)
		return;

	// 현재 LOD 레벨 가져오기 (기본값 0)
	CurrentLODLevelIndex = 0;
	CurrentLODLevel = SpriteTemplate->GetLODLevel(CurrentLODLevelIndex);

	if (!CurrentLODLevel)
		return;

	ParticleSize = SpriteTemplate->ParticleSize;

	// 언리얼 엔진 호환: 타이밍 상태 초기화
	EmitterTime = 0.0f;
	SecondsSinceCreation = 0.0f;
	CurrentLoopCount = 0;
	bEmitterEnabled = true;
	bDelayComplete = false;
	bBurstFired = false;  // Burst도 초기화
	SpawnFraction = 0.0f;

	// 언리얼 엔진 호환: Required 모듈에서 설정 읽기
	if (CurrentLODLevel->RequiredModule)
	{
		UParticleModuleRequired* RequiredModule = CurrentLODLevel->RequiredModule;

		// EmitterDelay 초기화 (랜덤 범위 지원)
		if (RequiredModule->EmitterDelayLow > 0.0f)
		{
			EmitterDelayActual = RandomStream.GetRangeFloat(
				RequiredModule->EmitterDelayLow,
				RequiredModule->EmitterDelay
			);
		}
		else
		{
			EmitterDelayActual = RequiredModule->EmitterDelay;
		}

		// EmitterDuration 초기화 (랜덤 범위 지원)
		if (RequiredModule->EmitterDurationLow > 0.0f)
		{
			EmitterDurationActual = RandomStream.GetRangeFloat(
				RequiredModule->EmitterDurationLow,
				RequiredModule->EmitterDuration
			);
		}
		else
		{
			EmitterDurationActual = RequiredModule->EmitterDuration;
		}

		// 이미터 트랜스폼 캐시
		CachedEmitterOrigin = RequiredModule->EmitterOrigin;
		CachedEmitterRotation = RequiredModule->EmitterRotation;

		// 이미터 회전 행렬 생성 (Euler angles → Quaternion → Matrix)
		if (CachedEmitterRotation.X != 0.0f || CachedEmitterRotation.Y != 0.0f || CachedEmitterRotation.Z != 0.0f)
		{
			FQuat RotationQuat = FQuat::MakeFromEulerZYX(CachedEmitterRotation);
			EmitterToWorld = RotationQuat.ToMatrix();
		}
		else
		{
			EmitterToWorld = FMatrix::Identity();
		}
	}

	// 초기화 로직 호출
	SetupEmitter();

	// 초기 파티클 데이터 할당
	Resize(100); // Default to 100 particles
}

void FParticleEmitterInstance::SetLODLevel(int32 NewLODIndex)
{
	if (NewLODIndex == CurrentLODLevelIndex || !SpriteTemplate)
		return;

	UParticleLODLevel* NewLODLevel = SpriteTemplate->GetLODLevel(NewLODIndex);
	if (!NewLODLevel || !NewLODLevel->bEnabled)
		return;

	// Update State
	CurrentLODLevelIndex = NewLODIndex;
	CurrentLODLevel = NewLODLevel;

	// 수명/스폰 관련 상태 초기화
	SpawnFraction = 0.f;
	bBurstFired = false;

	// 언리얼 엔진 호환: 타이밍 상태 리셋
	EmitterTime = 0.0f;
	CurrentLoopCount = 0;
	bDelayComplete = false;
	bEmitterEnabled = true;

	// 새 LOD의 Delay/Duration 재계산
	if (NewLODLevel->RequiredModule)
	{
		UParticleModuleRequired* RequiredModule = NewLODLevel->RequiredModule;

		if (RequiredModule->EmitterDelayLow > 0.0f)
		{
			EmitterDelayActual = RandomStream.GetRangeFloat(
				RequiredModule->EmitterDelayLow,
				RequiredModule->EmitterDelay
			);
		}
		else
		{
			EmitterDelayActual = RequiredModule->EmitterDelay;
		}

		if (RequiredModule->EmitterDurationLow > 0.0f)
		{
			EmitterDurationActual = RandomStream.GetRangeFloat(
				RequiredModule->EmitterDurationLow,
				RequiredModule->EmitterDuration
			);
		}
		else
		{
			EmitterDurationActual = RequiredModule->EmitterDuration;
		}

		// 이미터 트랜스폼 재캐시
		CachedEmitterOrigin = RequiredModule->EmitterOrigin;
		CachedEmitterRotation = RequiredModule->EmitterRotation;

		if (CachedEmitterRotation.X != 0.0f || CachedEmitterRotation.Y != 0.0f || CachedEmitterRotation.Z != 0.0f)
		{
			FQuat RotationQuat = FQuat::MakeFromEulerZYX(CachedEmitterRotation);
			EmitterToWorld = RotationQuat.ToMatrix();
		}
		else
		{
			EmitterToWorld = FMatrix::Identity();
		}
	}

	KillAllParticles();
	SetupEmitter();

	// ParticleDataContainer 재할당
	Resize(MaxActiveParticles);
}

void FParticleEmitterInstance::SetupEmitter()
{
	if (!CurrentLODLevel)	return;

	// 언리얼 엔진 호환: 페이로드 크기 계산
	// 각 모듈이 필요로 하는 추가 데이터 크기를 계산하고 오프셋 할당
	uint32 TotalPayloadSize = 0;
	InstancePayloadSize = 0;

	// 모든 모듈의 페이로드 크기 계산
	for (UParticleModule* Module : CurrentLODLevel->Modules)
	{
		if (Module && Module->bEnabled)
		{
			// 파티클별 페이로드
			uint32 ModulePayloadSize = Module->RequiredBytes(this);
			Module->ModuleOffsetInParticle = TotalPayloadSize;
			TotalPayloadSize += ModulePayloadSize;

			// 인스턴스별 페이로드
			InstancePayloadSize += Module->RequiredBytesPerInstance();
		}
	}

	// TypeDataModule도 페이로드가 필요하면 같이 포함
	UParticleModuleTypeDataBase* TypeData = CurrentLODLevel->TypeDataModule;
	if (TypeData && TypeData->bEnabled)
	{
		uint32 TypePayloadSize = TypeData->RequiredBytes();
		TypeData->ModuleOffsetInParticle = TotalPayloadSize;
		TotalPayloadSize += TypePayloadSize;

		// Emitter 초기화: 추후에는 Beam/Ribbon 등의 모든 특수 타입 이미터 대상으로 이루어질 것
		TypeData->SetupEmitterInstance(this);
	}

	// 파티클 크기와 스트라이드 계산 (기본 파티클 + 페이로드)
	PayloadOffset = ParticleSize;  // 페이로드는 기본 파티클 뒤에 위치
	ParticleStride = ParticleSize + TotalPayloadSize;

	// 인스턴스 데이터 할당 (필요한 경우)
	if (InstancePayloadSize > 0)
	{
		if (InstanceData)
		{
			delete[] InstanceData;
		}
		InstanceData = new uint8[InstancePayloadSize];
		memset(InstanceData, 0, InstancePayloadSize);
	}
}

void FParticleEmitterInstance::Resize(int32 NewMaxActiveParticles)
{
	if (NewMaxActiveParticles == MaxActiveParticles)
	{
		return;
	}

	MaxActiveParticles = NewMaxActiveParticles;

	if (MaxActiveParticles > 0)
	{
		// 언리얼 엔진 호환: FParticleDataContainer를 사용하여 16바이트 정렬 메모리 할당
		int32 ParticleDataSize = MaxActiveParticles * ParticleStride;
		bool bAllocSuccess = ParticleDataContainer.Alloc(ParticleDataSize, MaxActiveParticles);

		if (bAllocSuccess)
		{
			// 컨테이너에서 포인터 가져오기
			ParticleData = ParticleDataContainer.ParticleData;
			ParticleIndices = ParticleDataContainer.ParticleIndices;

			// 인덱스 초기화
			if (ParticleIndices)
			{
				for (int32 i = 0; i < MaxActiveParticles; i++)
				{
					ParticleIndices[i] = i;
				}
			}
		}
		else
		{
			// 할당 실패 시 폴백: 이전 상태 유지
			UE_LOG("[ParticleEmitterInstance] Failed to allocate particle memory: requested %d particles (%d bytes)\n",
				MaxActiveParticles, ParticleDataSize);
			ParticleData = nullptr;
			ParticleIndices = nullptr;
			MaxActiveParticles = 0;
		}
	}
	else
	{
		// 크기가 0이면 해제
		ParticleDataContainer.Free();
		ParticleData = nullptr;
		ParticleIndices = nullptr;
	}

	ActiveParticles = 0;
}

void FParticleEmitterInstance::Tick(float DeltaTime, bool bSuppressSpawning)
{
	if (!CurrentLODLevel || !bEmitterEnabled)
	{
		return;
	}

	UParticleModuleRequired* RequiredModule = CurrentLODLevel->RequiredModule;

	// 언리얼 엔진 호환: 타이머 업데이트
	EmitterTime += DeltaTime;
	SecondsSinceCreation += DeltaTime;

	// ====== PHASE 1: DELAY PHASE ======
	// 딜레이가 끝나지 않았으면 스폰 억제
	if (!bDelayComplete)
	{
		if (EmitterTime < EmitterDelayActual)
		{
			// 아직 딜레이 중 - 기존 파티클만 업데이트
			UpdateParticles(DeltaTime);
			return;  // 새 파티클 생성 안 함
		}
		else
		{
			// 딜레이 완료
			bDelayComplete = true;
		}
	}

	// ====== PHASE 2: ACTIVE EMISSION PHASE ======
	// 기존 파티클 업데이트
	UpdateParticles(DeltaTime);

	// Duration 체크: 생성 시간이 만료되었는지 확인
	bool bSuppressSpawningDuration = false;

	if (RequiredModule && EmitterDurationActual > 0.0f)  // 0 = 무한
	{
		float TimeIntoEmission = EmitterTime - EmitterDelayActual;

		if (TimeIntoEmission >= EmitterDurationActual)
		{
			// Duration 만료
			bSuppressSpawningDuration = true;

			// ====== PHASE 3: DURATION END / LOOP CHECK ======
			// 루프 체크
			if (RequiredModule->EmitterLoops == 0 ||
				CurrentLoopCount < RequiredModule->EmitterLoops - 1)
			{
				// 루프 재시작
				CurrentLoopCount++;
				EmitterTime = 0.0f;
				bBurstFired = false;
				SpawnFraction = 0.0f;
				bSuppressSpawningDuration = false;

				// 첫 루프에만 딜레이 적용하는 옵션
				if (!RequiredModule->bDelayFirstLoopOnly)
				{
					bDelayComplete = false;
					return;  // 딜레이 페이즈로 돌아감
				}
			}
			else
			{
				// 더 이상 루프 없음 - 이미터 비활성화
				bEmitterEnabled = false;
				return;
			}
		}
	}

	// 새 파티클 생성
	if (!bSuppressSpawning && !bSuppressSpawningDuration)
	{
		UParticleModuleSpawn* SpawnModule = CurrentLODLevel->SpawnModule;

		if (SpawnModule && SpawnModule->bEnabled)
		{
			int32 SpawnCount = SpawnModule->CalculateSpawnCount(DeltaTime, SpawnFraction, bBurstFired);

			if (SpawnCount > 0)
			{
				float Increment = (SpawnCount > 1) ? (DeltaTime / SpawnCount) : 0.0f;

				// 언리얼 엔진 호환: EmitterOrigin을 InitialLocation으로 전달
				SpawnParticles(SpawnCount, 0.0f, Increment, CachedEmitterOrigin, FVector(0.0f, 0.0f, 0.0f));
			}
		}
	}
}

void FParticleEmitterInstance::SpawnParticles(int32 Count, float StartTime, float Increment, const FVector& InitialLocation, const FVector& InitialVelocity)
{
	// 필수 객체 nullptr 체크
	if (!CurrentLODLevel || !ParticleData)
	{
		return;
	}

	for (int32 i = 0; i < Count; i++)
	{
		// 공간이 있는지 확인
		if (ActiveParticles >= MaxActiveParticles)
		{
			// 더 많은 파티클을 수용하도록 크기 조정
			Resize(MaxActiveParticles * 2);

			// Resize 실패 시 중단
			if (!ParticleData)
			{
				return;
			}
		}

		// 새 파티클의 슬롯 가져오기 (인덱스 시스템 사용)
		// ParticleIndices[ActiveParticles]는 다음 사용 가능한 슬롯을 가리킴
		// (KillParticle에서 스왑된 빈 슬롯 또는 초기화 시 순차 슬롯)
		int32 ParticleSlot = ParticleIndices[ActiveParticles];
		ActiveParticles++;

		// 파티클 포인터 가져오기 (언리얼 방식)
		uint8* ParticleBase = ParticleData + (ParticleSlot * ParticleStride);
		DECLARE_PARTICLE_PTR(Particle, ParticleBase);

		// 생성 전
		PreSpawn(Particle, InitialLocation, InitialVelocity);
		float SpawnTime = StartTime + i * Increment;

		// 생성 모듈 적용
		for (UParticleModule* Module : CurrentLODLevel->SpawnModules)
		{
			if (Module && Module->bEnabled)
			{
				// PayloadOffset 추가: 페이로드는 FBaseParticle 뒤에 위치
				Module->Spawn(this, PayloadOffset + Module->ModuleOffsetInParticle, SpawnTime, Particle);
			}
		}

		// 언리얼 엔진 호환: EmitterRotation 적용
		// 모든 spawn 모듈이 실행된 후 파티클 속도를 회전시킴
		if (CachedEmitterRotation.X != 0.0f || CachedEmitterRotation.Y != 0.0f || CachedEmitterRotation.Z != 0.0f)
		{
			Particle->Velocity = EmitterToWorld.TransformVector(Particle->Velocity);
			Particle->BaseVelocity = EmitterToWorld.TransformVector(Particle->BaseVelocity);
		}

		// 생성 후
		PostSpawn(Particle, static_cast<float>(i) / Count, SpawnTime);

		ParticleCounter++;
	}
}

void FParticleEmitterInstance::PreSpawn(FBaseParticle* Particle, const FVector& InitialLocation, const FVector& InitialVelocity)
{
	if (!Particle)
	{
		return;
	}

	// 기본값으로 파티클 초기화 (언리얼 엔진 호환)
	Particle->OldLocation = InitialLocation;
	Particle->Location = InitialLocation;
	Particle->BaseVelocity = InitialVelocity;
	Particle->Velocity = InitialVelocity;
	Particle->Rotation = 0.0f;
	Particle->BaseRotationRate = 0.0f;
	Particle->RotationRate = 0.0f;
	Particle->BaseSize = FVector(1.0f, 1.0f, 1.0f);
	Particle->Size = FVector(1.0f, 1.0f, 1.0f);
	Particle->Flags = 0;
	Particle->BaseColor = FLinearColor(1.0f, 1.0f, 1.0f, 1.0f);
	Particle->Color = FLinearColor(1.0f, 1.0f, 1.0f, 1.0f);
	Particle->RelativeTime = 0.0f;
	Particle->OneOverMaxLifetime = 1.0f;  // 기본 수명 1초 (1 / 1.0f = 1.0f)
}

void FParticleEmitterInstance::PostSpawn(FBaseParticle* Particle, float InterpolationParameter, float SpawnTime)
{
	// 생성 후 추가 로직을 여기에 추가할 수 있음
}

void FParticleEmitterInstance::UpdateParticles(float DeltaTime)
{
	if (!CurrentLODLevel || ActiveParticles <= 0)
	{
		return;
	}

	// 모든 파티클의 기본 속성 업데이트 (역방향 순회)
	for (int32 i = ActiveParticles - 1; i >= 0; i--)
	{
		// 파티클 가져오기
		FBaseParticle* Particle = GetParticleAtIndex(i);
		if (!Particle)
		{
			continue;
		}

		// Freeze 상태면 스킵
		if (Particle->Flags & STATE_Particle_Freeze)
		{
			continue;
		}

		// 이전 위치 저장 (충돌 처리용)
		Particle->OldLocation = Particle->Location;

		// 수명 업데이트 (수명 초과 시 파티클 제거)
		Particle->RelativeTime += DeltaTime * Particle->OneOverMaxLifetime;
		if (Particle->RelativeTime >= 1.0f)
		{
			KillParticle(i);
			continue;
		}

		// 위치 업데이트
		if ((Particle->Flags & STATE_Particle_FreezeTranslation) == 0)
		{
			Particle->Location += Particle->Velocity * DeltaTime;
		}

		// 회전 업데이트
		if ((Particle->Flags & STATE_Particle_FreezeRotation) == 0)
		{
			Particle->Rotation += Particle->RotationRate * DeltaTime;
		}

		// 방금 생성된 파티클인지 확인, 다음 프레임을 위해 플래그 즉시 제거
		bool bJustSpawned = (Particle->Flags & STATE_Particle_JustSpawned) != 0;
		Particle->Flags &= ~STATE_Particle_JustSpawned;
	}

	// 업데이트 모듈 적용 (언리얼 엔진 방식: Context 사용)
	for (UParticleModule* Module : CurrentLODLevel->UpdateModules)
	{
		if (Module && Module->bEnabled && Module->bUpdateModule)
		{
			// PayloadOffset 추가: 페이로드는 FBaseParticle 뒤에 위치
			FModuleUpdateContext Context = { *this, PayloadOffset + Module->ModuleOffsetInParticle, DeltaTime };
			Module->Update(Context);
		}
	}
}

void FParticleEmitterInstance::KillParticle(int32 Index)
{
	if (Index < 0 || Index >= ActiveParticles)
	{
		return;
	}

	// 마지막 활성 파티클과 교체
	if (Index != ActiveParticles - 1)
	{
		uint16 Temp = ParticleIndices[Index];
		ParticleIndices[Index] = ParticleIndices[ActiveParticles - 1];
		ParticleIndices[ActiveParticles - 1] = Temp;
	}

	ActiveParticles--;
}

void FParticleEmitterInstance::KillAllParticles()
{
	ActiveParticles = 0;
}

FBaseParticle* FParticleEmitterInstance::GetParticleAtIndex(int32 Index)
{
	if (Index < 0 || Index >= ActiveParticles)
	{
		return nullptr;
	}

	int32 ParticleIndex = ParticleIndices[Index];
	uint8* ParticleBase = ParticleData + (ParticleIndex * ParticleStride);
	return (FBaseParticle*)ParticleBase;
}

FDynamicEmitterDataBase* FParticleEmitterInstance::GetDynamicData(bool bSelected)
{
	// 필수 객체 nullptr 체크
	if (!ParticleData || !CurrentLODLevel || ActiveParticles <= 0)
	{
		return nullptr;
	}

	UParticleModuleTypeDataBase* TypeData = CurrentLODLevel->TypeDataModule;

	// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	// BeamEmitter DynamicData
	// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	if (auto* BeamType = Cast<UParticleModuleTypeDataBeam>(TypeData))
	{
		auto* BeamData = new FDynamicBeamEmitterData();
		if (!BuildBeamDynamicData(BeamData, BeamType))
		{
			delete BeamData;
			return nullptr;
		}
		return BeamData;
	}

	// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	// RibbonEmitter DynamicData
	// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	if (auto* RibbonType = Cast<UParticleModuleTypeDataRibbon>(TypeData))
	{
		auto* RibbonData = new FDynamicRibbonEmitterData();
		if (!BuildRibbonDynamicData(RibbonData, RibbonType))
		{
			delete RibbonData;
			return nullptr;
		}
		return RibbonData;
	}

	// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	// MeshEmitter DynamicData
	// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	if (auto* MeshType = Cast<UParticleModuleTypeDataMesh>(TypeData))
	{
		FDynamicMeshEmitterData* MeshData = new FDynamicMeshEmitterData();
		if (!BuildMeshDynamicData(MeshData, MeshType))
		{
			delete MeshData;
			return nullptr;
		}
		return MeshData;
	}

	// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	// SpriteEmitter DynamicData
	// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	{
		FDynamicSpriteEmitterData* SpriteData = new FDynamicSpriteEmitterData();
		if (!BuildSpriteDynamicData(SpriteData))
		{
			delete SpriteData;
			return nullptr;
		}
		return SpriteData;
	}
}

bool FParticleEmitterInstance::BuildSpriteDynamicData(FDynamicSpriteEmitterData* Data)
{
	if(!Data)	return false;

	// 소스 데이터 설정
	Data->Source.ActiveParticleCount = ActiveParticles;
	Data->Source.ParticleStride = ParticleStride;

	// 파티클 데이터 복사 (언리얼 엔진 방식: Alloc 사용)
	int32 ParticleDataBytes = ActiveParticles * ParticleStride;
	bool bAllocSuccess = Data->Source.DataContainer.Alloc(ParticleDataBytes, ActiveParticles);

	if (!bAllocSuccess)
	{
		// 할당 실패 시 데이터 삭제 후 nullptr 반환
		UE_LOG("[ParticleEmitterInstance] Failed to allocate render thread data: %d particles (%d bytes)\n",
			ActiveParticles, ParticleDataBytes);
		return false;
	}

	// 컴팩트 복사: 활성 파티클만 연속으로 복사 (sparse array → dense array)
	uint8* DstData = Data->Source.DataContainer.ParticleData;
	for (int32 i = 0; i < ActiveParticles; i++)
	{
		int32 SrcIndex = ParticleIndices[i];
		const uint8* SrcParticle = ParticleData + SrcIndex * ParticleStride;
		memcpy(DstData + i * ParticleStride, SrcParticle, ParticleStride);

		// 인덱스는 컴팩트 복사 후 순차적으로 재매핑
		Data->Source.DataContainer.ParticleIndices[i] = static_cast<uint16>(i);
	}

	// 언리얼 엔진 호환: Required 모듈과 Material 설정 (렌더링 시 필요)
	if (CurrentLODLevel && CurrentLODLevel->RequiredModule)
	{
		// 렌더 스레드용 데이터로 변환하여 저장 (TUniquePtr 사용)
		Data->Source.RequiredModule = std::make_unique<FParticleRequiredModule>(
			CurrentLODLevel->RequiredModule->ToRenderThreadData()
		);
		Data->Source.MaterialInterface = Data->Source.RequiredModule->Material;
		Data->Source.SortMode = CurrentLODLevel->RequiredModule->SortMode;
	}
	else
	{
		Data->Source.RequiredModule = nullptr;
		Data->Source.MaterialInterface = nullptr;
		Data->Source.SortMode = 0;  // 정렬 없음
	}

	// 컴포넌트 스케일 설정
	if (Component)
	{
		Data->Source.Scale = Component->GetRelativeScale();
	}
	else
	{
		Data->Source.Scale = FVector(1.0f, 1.0f, 1.0f);
	}

	return true;
}

bool FParticleEmitterInstance::BuildMeshDynamicData(FDynamicMeshEmitterData* Data, UParticleModuleTypeDataMesh* MeshType)
{
	if (!Data || !MeshType)
	{
		return false;
	}

	Data->MeshSource.ActiveParticleCount = ActiveParticles;
	Data->MeshSource.ParticleStride = ParticleStride;

	// 파티클 데이터 복사 (스프라이트와 동일한 방식: 깊은 복사)
	int32 ParticleDataBytes = ActiveParticles * ParticleStride;
	bool bAllocSuccess = Data->MeshSource.DataContainer.Alloc(ParticleDataBytes, ActiveParticles);

	if (!bAllocSuccess)
	{
		return false;
	}

	// 컴팩트 복사: 활성 파티클만 연속으로 복사
	uint8* DstData = Data->MeshSource.DataContainer.ParticleData;
	for (int32 i = 0; i < ActiveParticles; i++)
	{
		int32 SrcIndex = ParticleIndices[i];
		const uint8* SrcParticle = ParticleData + SrcIndex * ParticleStride;
		memcpy(DstData + i * ParticleStride, SrcParticle, ParticleStride);

		// 인덱스는 컴팩트 복사 후 순차적으로 재매핑
		Data->MeshSource.DataContainer.ParticleIndices[i] = static_cast<uint16>(i);
	}

	// TypeData에서 Mesh 정보 받아오기
	if (MeshType)
	{
		Data->MeshSource.MeshData = MeshType->Mesh;
	}
	Data->MeshSource.MaterialInterface = CurrentLODLevel->RequiredModule ? CurrentLODLevel->RequiredModule->Material : nullptr;
	Data->MeshSource.SortMode = CurrentLODLevel->RequiredModule ? CurrentLODLevel->RequiredModule->SortMode : 0;

	// MeshRotation 모듈 오프셋 찾기
	Data->MeshSource.MeshRotationPayloadOffset = -1;
	for (UParticleModule* Module : CurrentLODLevel->Modules)
	{
		if (Module && Module->bEnabled)
		{
			UParticleModuleMeshRotation* MeshRotModule = Cast<UParticleModuleMeshRotation>(Module);
			if (MeshRotModule)
			{
				// PayloadOffset + ModuleOffsetInParticle = 파티클 내 페이로드 위치
				Data->MeshSource.MeshRotationPayloadOffset = PayloadOffset + MeshRotModule->ModuleOffsetInParticle;
				break;
			}
		}
	}
	return true;
}

bool FParticleEmitterInstance::BuildBeamDynamicData(FDynamicBeamEmitterData* Data, UParticleModuleTypeDataBeam* BeamType)
{
	if (!Data || !BeamType)
		return false;

	// Beam은 보통 하나의 EmitterInstance에서 한 개의 Beam 생성 (단순 버전)
	// SegmentCount는 TypeData에서 관리한다고 가정
	const int32 SegmentCount = BeamType->SegmentCount;   // 예: 8
	if (SegmentCount <= 0)
		return false;

	// 현 프레임에서 Beam을 구성할 파티클 포인트가 필요함.
	// 간단한 형태: 첫 번째 파티클 = StartPoint, 마지막 파티클 = EndPoint로 사용
	if (ActiveParticles <= 0)
		return false;

	// Source 설정
	Data->Source.Width = BeamType->BeamWidth;
	Data->Source.Material = CurrentLODLevel->RequiredModule
		? CurrentLODLevel->RequiredModule->Material
		: nullptr;

	// -----------------------------
	// 1) BeamPoints 채우기
	// -----------------------------

	Data->Source.BeamPoints.Empty();
	Data->Source.BeamPoints.Reserve(SegmentCount + 1);

	// Start / End를 파티클 데이터에서 얻는다.
	// 첫 번째 활성 파티클 → Start
	// 마지막 활성 파티클 → End
	const FBaseParticle* StartParticle = GetParticleAtIndex(ParticleIndices[0]);
	const FBaseParticle* EndParticle = GetParticleAtIndex(ParticleIndices[ActiveParticles - 1]);

	FVector StartPos = StartParticle->Location;
	FVector EndPos = EndParticle->Location;

	// 세그먼트 분할
	for (int32 i = 0; i <= SegmentCount; i++)
	{
		float T = (float)i / (float)SegmentCount;
		FVector P = FMath::Lerp(StartPos, EndPos, T);
		Data->Source.BeamPoints.Add(P);
	}

	return true;
}

bool FParticleEmitterInstance::BuildRibbonDynamicData(FDynamicRibbonEmitterData* Data, UParticleModuleTypeDataRibbon* RibbonType)
{
	if (!Data || !RibbonType)
		return false;

	if (ActiveParticles <= 1)
		return false; // 최소 2개 파티클 필요

	// Material
	Data->Source.Material = CurrentLODLevel->RequiredModule
		? CurrentLODLevel->RequiredModule->Material
		: nullptr;

	// Width
	float RibbonWidth = RibbonType->RibbonWidth;

	// RibbonPoints 배열 채우기
	Data->Source.RibbonPoints.Empty();
	Data->Source.RibbonPoints.Reserve(ActiveParticles);

	for (int32 i = 0; i < ActiveParticles; i++)
	{
		const FBaseParticle* P = GetParticleAtIndex(ParticleIndices[i]);
		Data->Source.RibbonPoints.Add(P->Location);
	}

	// UV, Color, Tangent 같은 추가 정보는 이후 확장 가능
	// Renderer에서 RibbonPoints를 기반으로 Triangle Strip을 만들면 됨.

	return true;
}