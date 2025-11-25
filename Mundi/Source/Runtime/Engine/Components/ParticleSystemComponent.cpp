#include "pch.h"
#include "ParticleSystemComponent.h"
#include "SceneView.h"
#include "MeshBatchElement.h"
#include "Material.h"
#include "Shader.h"
#include "ResourceManager.h"
#include "Texture.h"
#include "StaticMesh.h"
#include "Modules/ParticleModuleSpawn.h"
#include "Modules/ParticleModuleLifetime.h"
#include "Modules/ParticleModuleVelocity.h"
#include "Modules/ParticleModuleTypeDataMesh.h"
#include "Modules/ParticleModuleTypeDataSprite.h"
#include "Modules/ParticleModuleMeshRotation.h"
#include "Modules/ParticleModuleSize.h"
#include "Modules/ParticleModuleColor.h"
#include "Modules/ParticleModuleRotation.h"
#include "Modules/ParticleModuleRotationRate.h"
#include "Modules/ParticleModuleLocation.h"

// Static 멤버 정의
ID3D11Buffer* UParticleSystemComponent::SpriteQuadVertexBuffer = nullptr;
ID3D11Buffer* UParticleSystemComponent::SpriteQuadIndexBuffer = nullptr;
bool UParticleSystemComponent::bQuadBuffersInitialized = false;

// Quad 버텍스 구조체 (UV만 포함)
struct FSpriteQuadVertex
{
	FVector2D UV;
};

void UParticleSystemComponent::InitializeQuadBuffers()
{
	if (bQuadBuffersInitialized)
		return;

	ID3D11Device* Device = GEngine.GetRHIDevice()->GetDevice();

	// 4개 코너 버텍스 (UV만)
	FSpriteQuadVertex Vertices[4] = {
		{ FVector2D(0.0f, 0.0f) },  // 좌상
		{ FVector2D(1.0f, 0.0f) },  // 우상
		{ FVector2D(0.0f, 1.0f) },  // 좌하
		{ FVector2D(1.0f, 1.0f) }   // 우하
	};

	D3D11_BUFFER_DESC VBDesc = {};
	VBDesc.ByteWidth = sizeof(Vertices);
	VBDesc.Usage = D3D11_USAGE_IMMUTABLE;
	VBDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

	D3D11_SUBRESOURCE_DATA VBData = {};
	VBData.pSysMem = Vertices;

	Device->CreateBuffer(&VBDesc, &VBData, &SpriteQuadVertexBuffer);

	// 6개 인덱스 (2개 삼각형)
	uint32 Indices[6] = { 0, 1, 2, 2, 1, 3 };

	D3D11_BUFFER_DESC IBDesc = {};
	IBDesc.ByteWidth = sizeof(Indices);
	IBDesc.Usage = D3D11_USAGE_IMMUTABLE;
	IBDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

	D3D11_SUBRESOURCE_DATA IBData = {};
	IBData.pSysMem = Indices;

	Device->CreateBuffer(&IBDesc, &IBData, &SpriteQuadIndexBuffer);

	bQuadBuffersInitialized = true;
}

void UParticleSystemComponent::ReleaseQuadBuffers()
{
	if (SpriteQuadVertexBuffer)
	{
		SpriteQuadVertexBuffer->Release();
		SpriteQuadVertexBuffer = nullptr;
	}
	if (SpriteQuadIndexBuffer)
	{
		SpriteQuadIndexBuffer->Release();
		SpriteQuadIndexBuffer = nullptr;
	}
	bQuadBuffersInitialized = false;
}

UParticleSystemComponent::UParticleSystemComponent()
{
	bAutoActivate = true;
	bCanEverTick = true;   // Tick 활성화
	bTickInEditor = true;  // 에디터에서도 파티클 미리보기 가능
}

UParticleSystemComponent::~UParticleSystemComponent()
{
	// OnUnregister에서 이미 정리되었으므로, 안전 체크만 수행
	// (만약 OnUnregister가 호출되지 않은 경우를 대비)
	if (EmitterInstances.Num() > 0)
	{
		ClearEmitterInstances();
	}

	if (EmitterRenderData.Num() > 0)
	{
		for (int32 i = 0; i < EmitterRenderData.Num(); i++)
		{
			if (EmitterRenderData[i])
			{
				delete EmitterRenderData[i];
				EmitterRenderData[i] = nullptr;
			}
		}
		EmitterRenderData.Empty();
	}

	// 스프라이트 인스턴스 버퍼 해제
	if (SpriteInstanceBuffer)
	{
		SpriteInstanceBuffer->Release();
		SpriteInstanceBuffer = nullptr;
	}

	// 메시 인스턴스 버퍼 해제
	if (MeshInstanceBuffer)
	{
		MeshInstanceBuffer->Release();
		MeshInstanceBuffer = nullptr;
	}
}

void UParticleSystemComponent::OnRegister(UWorld* InWorld)
{
	Super::OnRegister(InWorld);

	// 이미 초기화되어 있으면 스킵 (OnRegister는 여러 번 호출될 수 있음)
	if (EmitterInstances.Num() > 0)
	{
		return;
	}

	// Template이 없으면 디버그용 기본 파티클 시스템 생성
	if (!Template)
	{
		//CreateDebugParticleSystem();        // 메시 파티클 테스트
		CreateDebugSpriteParticleSystem();  // 스프라이트 파티클 테스트
	}

	// 에디터에서도 파티클 미리보기를 위해 자동 활성화
	if (bAutoActivate)
	{
		ActivateSystem();
	}
}

void UParticleSystemComponent::CreateDebugParticleSystem()
{
	// 디버그/테스트용 메시 파티클 시스템 생성
	// Editor 통합 완료 후에는 Editor에서 설정한 Template 사용
	Template = NewObject<UParticleSystem>();

	// 이미터 생성
	UParticleEmitter* Emitter = NewObject<UParticleEmitter>();

	// LOD 레벨 생성
	UParticleLODLevel* LODLevel = NewObject<UParticleLODLevel>();
	LODLevel->bEnabled = true;

	// 필수 모듈 생성
	LODLevel->RequiredModule = NewObject<UParticleModuleRequired>();
	// Material은 null로 두면 CreateMeshParticleBatch에서 메시의 내장 머터리얼을 자동으로 로드

	// 메시 타입 데이터 모듈 생성 (스프라이트 대신 메시 사용)
	UParticleModuleTypeDataMesh* MeshTypeData = NewObject<UParticleModuleTypeDataMesh>();
	// 테스트용 메시 로드 (큐브)
	MeshTypeData->Mesh = UResourceManager::GetInstance().Load<UStaticMesh>(GDataDir + "/cube-tex.obj");
	LODLevel->TypeDataModule = MeshTypeData;

	// 스폰 모듈 생성
	UParticleModuleSpawn* SpawnModule = NewObject<UParticleModuleSpawn>();
	SpawnModule->SpawnRate = 10000.0f;   // 초당 100개 파티클 (메시는 무거우므로 줄임)
	SpawnModule->BurstCount = 100;     // 시작 시 100개 버스트
	LODLevel->SpawnModule = SpawnModule;
	LODLevel->Modules.Add(SpawnModule);

	// 라이프타임 모듈 생성 (파티클 수명 설정)
	UParticleModuleLifetime* LifetimeModule = NewObject<UParticleModuleLifetime>();
	LifetimeModule->MinLifetime = 1.0f;  // 최소 1초
	LifetimeModule->MaxLifetime = 1.5f;  // 최대 1.5초
	LODLevel->Modules.Add(LifetimeModule);

	// 속도 모듈 생성 (테스트용: 위쪽으로 퍼지는 랜덤 속도)
	UParticleModuleVelocity* VelocityModule = NewObject<UParticleModuleVelocity>();
	VelocityModule->StartVelocity = FVector(0.0f, 0.0f, 10.0f);   // 기본 위쪽 속도
	VelocityModule->StartVelocityRange = FVector(3.0f, 3.0f, 3.0f);  // XYZ 랜덤 범위
	LODLevel->Modules.Add(VelocityModule);

	// 메시 회전 모듈 생성 (3축 회전 테스트)
	UParticleModuleMeshRotation* MeshRotModule = NewObject<UParticleModuleMeshRotation>();
	MeshRotModule->StartRotation = FVector(0.0f, 0.0f, 0.0f);  // 초기 회전
	MeshRotModule->RotationRandomness = FVector(0.5f, 0.5f, 0.5f);  // 랜덤 초기 회전
	MeshRotModule->StartRotationRate = FVector(1.0f, 0.5f, 2.0f);  // 회전 속도 (라디안/초)
	MeshRotModule->RotationRateRandomness = FVector(0.5f, 0.3f, 1.0f);  // 랜덤 회전 속도
	LODLevel->Modules.Add(MeshRotModule);

	// 위치 모듈 생성
	UParticleModuleLocation* LocationModule = NewObject<UParticleModuleLocation>();
	LODLevel->Modules.Add(LocationModule);

	// 크기 모듈 생성
	UParticleModuleSize* SizeModule = NewObject<UParticleModuleSize>();
	LODLevel->Modules.Add(SizeModule);

	// 모듈 캐싱
	LODLevel->CacheModuleInfo();

	// LOD 레벨을 이미터에 추가
	Emitter->LODLevels.Add(LODLevel);
	Emitter->CacheEmitterModuleInfo();

	// 이미터를 시스템에 추가
	Template->Emitters.Add(Emitter);
}

void UParticleSystemComponent::CreateDebugSpriteParticleSystem()
{
	// 디버그/테스트용 스프라이트 파티클 시스템 생성
	Template = NewObject<UParticleSystem>();

	// 이미터 생성
	UParticleEmitter* Emitter = NewObject<UParticleEmitter>();

	// LOD 레벨 생성
	UParticleLODLevel* LODLevel = NewObject<UParticleLODLevel>();
	LODLevel->bEnabled = true;

	// 필수 모듈 생성
	LODLevel->RequiredModule = NewObject<UParticleModuleRequired>();

	// 스프라이트용 Material 설정
	UMaterial* SpriteMaterial = NewObject<UMaterial>();
	UShader* SpriteShader = UResourceManager::GetInstance().Load<UShader>("Shaders/Particle/ParticleSprite.hlsl");
	SpriteMaterial->SetShader(SpriteShader);

	// 스프라이트 텍스처 설정 (불꽃/연기 등)
	FMaterialInfo MatInfo;
	MatInfo.DiffuseTextureFileName = GDataDir + "/Textures/Particles/Smoke.png";
	SpriteMaterial->SetMaterialInfo(MatInfo);
	SpriteMaterial->ResolveTextures();

	LODLevel->RequiredModule->Material = SpriteMaterial;

	// 스프라이트 타입 데이터 모듈 (TypeDataModule이 nullptr이면 기본적으로 스프라이트)
	UParticleModuleTypeDataSprite* SpriteTypeData = NewObject<UParticleModuleTypeDataSprite>();
	LODLevel->TypeDataModule = SpriteTypeData;

	// 스폰 모듈 생성
	UParticleModuleSpawn* SpawnModule = NewObject<UParticleModuleSpawn>();
	SpawnModule->SpawnRate = 5000.0f;    // 초당 50개 파티클
	SpawnModule->BurstCount = 10000;      // 시작 시 20개 버스트
	LODLevel->SpawnModule = SpawnModule;
	LODLevel->Modules.Add(SpawnModule);

	// 라이프타임 모듈 생성
	UParticleModuleLifetime* LifetimeModule = NewObject<UParticleModuleLifetime>();
	LifetimeModule->MinLifetime = 1.5f;
	LifetimeModule->MaxLifetime = 2.5f;
	LODLevel->Modules.Add(LifetimeModule);

	// 속도 모듈 생성 (위쪽으로 퍼지는 연기 효과)
	UParticleModuleVelocity* VelocityModule = NewObject<UParticleModuleVelocity>();
	VelocityModule->StartVelocity = FVector(0.0f, 0.0f, 30.0f);      // 위쪽 속도
	VelocityModule->StartVelocityRange = FVector(15.0f, 15.0f, 10.0f); // XYZ 랜덤 범위
	LODLevel->Modules.Add(VelocityModule);

	// 크기 모듈 (시간에 따라 커지는 효과)
	UParticleModuleSize* SizeModule = NewObject<UParticleModuleSize>();
	SizeModule->StartSize = FVector(5.0f, 5.0f, 5.0f);
	SizeModule->EndSize = FVector(15.0f, 15.0f, 15.0f);
	SizeModule->bUseSizeOverLife = true;
	LODLevel->Modules.Add(SizeModule);

	// 색상 모듈 (페이드 아웃 효과 - bUpdateModule이 기본 true라 자동 보간)
	UParticleModuleColor* ColorModule = NewObject<UParticleModuleColor>();
	ColorModule->StartColor = FLinearColor(1.0f, 1.0f, 1.0f, 1.0f);  // 불투명 흰색
	ColorModule->EndColor = FLinearColor(1.0f, 1.0f, 1.0f, 0.0f);    // 투명 흰색
	LODLevel->Modules.Add(ColorModule);

	// 회전 모듈 (2D 회전)
	UParticleModuleRotation* RotationModule = NewObject<UParticleModuleRotation>();
	RotationModule->StartRotation = 0.0f;
	RotationModule->RotationRandomness = 3.14159f;  // 랜덤 초기 회전
	LODLevel->Modules.Add(RotationModule);

	// 회전 속도 모듈
	UParticleModuleRotationRate* RotRateModule = NewObject<UParticleModuleRotationRate>();
	RotRateModule->StartRotationRate = 3.0f;  // 천천히 회전
	LODLevel->Modules.Add(RotRateModule);

	// 위치 모듈 생성
	UParticleModuleLocation* LocationModule = NewObject<UParticleModuleLocation>();
	LODLevel->Modules.Add(LocationModule);

	// 모듈 캐싱
	LODLevel->CacheModuleInfo();

	// LOD 레벨을 이미터에 추가
	Emitter->LODLevels.Add(LODLevel);
	Emitter->CacheEmitterModuleInfo();

	// 이미터를 시스템에 추가
	Template->Emitters.Add(Emitter);
}

void UParticleSystemComponent::OnUnregister()
{
	// 이미터 인스턴스 정리
	DeactivateSystem();

	// 렌더 데이터 정리
	for (int32 i = 0; i < EmitterRenderData.Num(); i++)
	{
		if (EmitterRenderData[i])
		{
			delete EmitterRenderData[i];
			EmitterRenderData[i] = nullptr;
		}
	}
	EmitterRenderData.Empty();

	// 인스턴스 버퍼 정리
	if (SpriteInstanceBuffer)
	{
		SpriteInstanceBuffer->Release();
		SpriteInstanceBuffer = nullptr;
	}
	AllocatedSpriteInstanceCount = 0;

	if (MeshInstanceBuffer)
	{
		MeshInstanceBuffer->Release();
		MeshInstanceBuffer = nullptr;
	}
	AllocatedMeshInstanceCount = 0;

	Super::OnUnregister();
}

void UParticleSystemComponent::TickComponent(float DeltaTime)
{
	USceneComponent::TickComponent(DeltaTime);

	// 모든 이미터 인스턴스 틱
	for (FParticleEmitterInstance* Instance : EmitterInstances)
	{
		if (Instance)
		{
			Instance->Tick(DeltaTime, false);
		}
	}

	// 렌더 데이터 업데이트
	UpdateRenderData();
}

void UParticleSystemComponent::ActivateSystem()
{
	if (Template)
	{
		InitializeEmitterInstances();
	}
}

void UParticleSystemComponent::DeactivateSystem()
{
	ClearEmitterInstances();
}

void UParticleSystemComponent::ResetParticles()
{
	for (FParticleEmitterInstance* Instance : EmitterInstances)
	{
		if (Instance)
		{
			Instance->KillAllParticles();
		}
	}
}

void UParticleSystemComponent::SetTemplate(UParticleSystem* NewTemplate)
{
	if (Template != NewTemplate)
	{
		Template = NewTemplate;

		// 활성 상태면 재초기화
		if (EmitterInstances.Num() > 0)
		{
			ClearEmitterInstances();
			InitializeEmitterInstances();
		}
	}
}

void UParticleSystemComponent::InitializeEmitterInstances()
{
	ClearEmitterInstances();

	if (!Template)
	{
		return;
	}

	// 이미터 인스턴스 생성
	for (UParticleEmitter* Emitter : Template->Emitters)
	{
		if (Emitter)
		{
			FParticleEmitterInstance* Instance = new FParticleEmitterInstance();
			Instance->Init(this, Emitter);
			EmitterInstances.Add(Instance);
		}
	}
}

void UParticleSystemComponent::ClearEmitterInstances()
{
	for (FParticleEmitterInstance* Instance : EmitterInstances)
	{
		if (Instance)
		{
			delete Instance;
		}
	}
	EmitterInstances.Empty();
}

void UParticleSystemComponent::UpdateRenderData()
{
	// 기존 렌더 데이터 제거
	for (int32 i = 0; i < EmitterRenderData.Num(); i++)
	{
		if (EmitterRenderData[i])
		{
			delete EmitterRenderData[i];
			EmitterRenderData[i] = nullptr;
		}
	}
	EmitterRenderData.Empty();

	// 각 이미터 인스턴스에서 GetDynamicData() 호출 (캡슐화된 패턴)
	for (int32 i = 0; i < EmitterInstances.Num(); i++)
	{
		FParticleEmitterInstance* Instance = EmitterInstances[i];
		if (!Instance)
		{
			continue;
		}

		// EmitterInstance가 자신의 렌더 데이터를 생성
		FDynamicEmitterDataBase* DynamicData = Instance->GetDynamicData(false);
		if (DynamicData)
		{
			DynamicData->EmitterIndex = i;
			EmitterRenderData.Add(DynamicData);
		}
	}
}

// 언리얼 엔진 호환: 인스턴스 파라미터 시스템 구현
void UParticleSystemComponent::SetFloatParameter(const FString& ParameterName, float Value)
{
	// 기존 파라미터 찾기
	for (FParticleParameter& Param : InstanceParameters)
	{
		if (Param.Name == ParameterName)
		{
			Param.FloatValue = Value;
			return;
		}
	}

	// 없으면 새로 추가
	FParticleParameter NewParam(ParameterName);
	NewParam.FloatValue = Value;
	InstanceParameters.Add(NewParam);
}

void UParticleSystemComponent::SetVectorParameter(const FString& ParameterName, const FVector& Value)
{
	// 기존 파라미터 찾기
	for (FParticleParameter& Param : InstanceParameters)
	{
		if (Param.Name == ParameterName)
		{
			Param.VectorValue = Value;
			return;
		}
	}

	// 없으면 새로 추가
	FParticleParameter NewParam(ParameterName);
	NewParam.VectorValue = Value;
	InstanceParameters.Add(NewParam);
}

void UParticleSystemComponent::SetColorParameter(const FString& ParameterName, const FLinearColor& Value)
{
	// 기존 파라미터 찾기
	for (FParticleParameter& Param : InstanceParameters)
	{
		if (Param.Name == ParameterName)
		{
			Param.ColorValue = Value;
			return;
		}
	}

	// 없으면 새로 추가
	FParticleParameter NewParam(ParameterName);
	NewParam.ColorValue = Value;
	InstanceParameters.Add(NewParam);
}

float UParticleSystemComponent::GetFloatParameter(const FString& ParameterName, float DefaultValue) const
{
	for (const FParticleParameter& Param : InstanceParameters)
	{
		if (Param.Name == ParameterName)
		{
			return Param.FloatValue;
		}
	}
	return DefaultValue;
}

FVector UParticleSystemComponent::GetVectorParameter(const FString& ParameterName, const FVector& DefaultValue) const
{
	for (const FParticleParameter& Param : InstanceParameters)
	{
		if (Param.Name == ParameterName)
		{
			return Param.VectorValue;
		}
	}
	return DefaultValue;
}

FLinearColor UParticleSystemComponent::GetColorParameter(const FString& ParameterName, const FLinearColor& DefaultValue) const
{
	for (const FParticleParameter& Param : InstanceParameters)
	{
		if (Param.Name == ParameterName)
		{
			return Param.ColorValue;
		}
	}
	return DefaultValue;
}

void UParticleSystemComponent::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	USceneComponent::Serialize(bInIsLoading, InOutHandle);

	// UPROPERTY 속성은 리플렉션 시스템에 의해 자동으로 직렬화됨

	if (!bInIsLoading && bAutoActivate)
	{
		// 로딩 후, 자동 활성화가 켜져 있으면 시스템 활성화
		ActivateSystem();
	}
}

void UParticleSystemComponent::DuplicateSubObjects()
{
	Super::DuplicateSubObjects();

	// PIE 복사 시 포인터 배열 초기화 (shallow copy된 포인터는 원본 소유이므로 복사본에서 비움)
	// 원본 객체를 delete하지 않도록 빈 배열로 초기화
	EmitterInstances.Empty();
	EmitterRenderData.Empty();

	// 인스턴스 버퍼도 원본 소유이므로 nullptr로 초기화
	MeshInstanceBuffer = nullptr;
	AllocatedMeshInstanceCount = 0;
	SpriteInstanceBuffer = nullptr;
	AllocatedSpriteInstanceCount = 0;
}

void UParticleSystemComponent::CollectMeshBatches(TArray<FMeshBatchElement>& OutMeshBatchElements, const FSceneView* View)
{
	// 1. 유효성 검사
	if (!IsVisible() || EmitterRenderData.Num() == 0)
	{
		return;
	}

	// 2. 스프라이트 파티클 수 계산 및 정렬
	int32 TotalSpriteParticles = 0;

	for (FDynamicEmitterDataBase* EmitterData : EmitterRenderData)
	{
		if (!EmitterData)
			continue;

		const FDynamicEmitterReplayDataBase& Source = EmitterData->GetSource();

		if (Source.eEmitterType == EDynamicEmitterType::Sprite)
		{
			TotalSpriteParticles += Source.ActiveParticleCount;

			// 스프라이트 이미터에 대해 정렬 수행
			auto* SpriteData = static_cast<FDynamicSpriteEmitterDataBase*>(EmitterData);
			FVector ViewOrigin = View ? View->ViewLocation : FVector(0.0f, 0.0f, 0.0f);
			SpriteData->SortSpriteParticles(Source.SortMode, ViewOrigin);
		}
	}

	// 3. 스프라이트 파티클 처리 (인스턴싱)
	if (TotalSpriteParticles > 0)
	{
		FillSpriteInstanceBuffer(TotalSpriteParticles);
		CreateSpriteParticleBatch(OutMeshBatchElements);
	}

	// 4. 메시 파티클 처리 (인스턴싱)
	int32 TotalMeshInstances = 0;
	for (FDynamicEmitterDataBase* EmitterData : EmitterRenderData)
	{
		if (!EmitterData)
			continue;

		const FDynamicEmitterReplayDataBase& Source = EmitterData->GetSource();
		if (Source.eEmitterType == EDynamicEmitterType::Mesh)
		{
			TotalMeshInstances += Source.ActiveParticleCount;
		}
	}

	if (TotalMeshInstances > 0)
	{
		FillMeshInstanceBuffer(TotalMeshInstances);
		CreateMeshParticleBatch(OutMeshBatchElements);
	}
}

void UParticleSystemComponent::FillMeshInstanceBuffer(uint32 TotalInstances)
{
	if (TotalInstances == 0)
	{
		return;
	}

	ID3D11Device* Device = GEngine.GetRHIDevice()->GetDevice();
	ID3D11DeviceContext* Context = GEngine.GetRHIDevice()->GetDeviceContext();

	// 인스턴스 버퍼 생성/리사이즈
	if (TotalInstances > AllocatedMeshInstanceCount)
	{
		if (MeshInstanceBuffer)
		{
			MeshInstanceBuffer->Release();
			MeshInstanceBuffer = nullptr;
		}

		uint32 NewCount = FMath::Max(TotalInstances * 2, 64u);
		D3D11_BUFFER_DESC Desc = {};
		Desc.ByteWidth = NewCount * sizeof(FMeshParticleInstanceVertex);
		Desc.Usage = D3D11_USAGE_DYNAMIC;
		Desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		Desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

		if (SUCCEEDED(Device->CreateBuffer(&Desc, nullptr, &MeshInstanceBuffer)))
		{
			AllocatedMeshInstanceCount = NewCount;
		}
	}

	if (!MeshInstanceBuffer)
	{
		return;
	}

	// 버퍼 매핑
	D3D11_MAPPED_SUBRESOURCE MappedData;
	if (FAILED(Context->Map(MeshInstanceBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedData)))
	{
		return;
	}

	FMeshParticleInstanceVertex* Instances = static_cast<FMeshParticleInstanceVertex*>(MappedData.pData);
	uint32 InstanceOffset = 0;

	// 메시 이미터 순회
	for (FDynamicEmitterDataBase* EmitterData : EmitterRenderData)
	{
		if (!EmitterData)
			continue;

		const FDynamicEmitterReplayDataBase& Source = EmitterData->GetSource();
		if (Source.eEmitterType != EDynamicEmitterType::Mesh)
			continue;

		// 메시 이미터로 캐스팅하여 MeshRotationPayloadOffset 접근
		const FDynamicMeshEmitterReplayDataBase& MeshSource =
			static_cast<const FDynamicMeshEmitterReplayDataBase&>(Source);

		const int32 ParticleCount = Source.ActiveParticleCount;
		const int32 ParticleStride = Source.ParticleStride;
		const uint8* ParticleData = Source.DataContainer.ParticleData;
		const uint16* ParticleIndices = Source.DataContainer.ParticleIndices;
		const int32 MeshRotationOffset = MeshSource.MeshRotationPayloadOffset;

		if (!ParticleData || ParticleCount == 0)
			continue;

		// 각 파티클의 인스턴스 데이터 생성
		for (int32 ParticleIdx = 0; ParticleIdx < ParticleCount; ParticleIdx++)
		{
			const int32 ParticleIndex = ParticleIndices ? ParticleIndices[ParticleIdx] : ParticleIdx;
			const uint8* ParticleBase = ParticleData + ParticleIndex * ParticleStride;
			const FBaseParticle* Particle = reinterpret_cast<const FBaseParticle*>(ParticleBase);

			FMeshParticleInstanceVertex& Instance = Instances[InstanceOffset];

			// 컴포넌트 트랜스폼 적용
			FVector WorldPos = Particle->Location;
			FVector Scale = Particle->Size;

			// 3D 회전 데이터 가져오기
			FVector Rotation(0.0f, 0.0f, Particle->Rotation);  // 기본값: Z축만
			if (MeshRotationOffset >= 0)
			{
				// MeshRotation 페이로드에서 3축 회전 가져오기
				const FMeshRotationPayload* RotPayload =
					reinterpret_cast<const FMeshRotationPayload*>(ParticleBase + MeshRotationOffset);
				Rotation = RotPayload->Rotation;
			}

			// TODO(human): 3D 회전을 포함한 Transform 행렬 계산
			// Scale, Rotation(Pitch/Yaw/Roll), WorldPos를 사용하여 3x4 변환 행렬을 계산합니다.
			// 힌트:
			// 1. 각 축별 회전 행렬: Rx(Pitch), Ry(Yaw), Rz(Roll)
			// 2. 최종 회전 = Rz * Ry * Rx (Roll -> Yaw -> Pitch 순서)
			// 3. Transform = Scale * Rotation * Translation
			// 4. 행렬은 전치하여 저장 (셰이더에서 row_major 사용)

			// 기본 구현: Scale + Translation (회전 없음)
			float RadToDeg = 180.0f / 3.141592f;

			FMatrix FinalMatrix = FMatrix::FromTRS(
				WorldPos, FQuat::MakeFromEulerZYX(Rotation * RadToDeg), Scale);
			FMatrix TransposedMatrix = FinalMatrix.Transpose();

			Instance.Transform[0] = TransposedMatrix.VRows[0];
			Instance.Transform[1] = TransposedMatrix.VRows[1];
			Instance.Transform[2] = TransposedMatrix.VRows[2];

			// 색상
			Instance.Color = Particle->Color;

			// RelativeTime
			Instance.RelativeTime = Particle->RelativeTime;

			InstanceOffset++;
		}
	}

	Context->Unmap(MeshInstanceBuffer, 0);
}

void UParticleSystemComponent::CreateMeshParticleBatch(TArray<FMeshBatchElement>& OutMeshBatchElements)
{
	if (!MeshInstanceBuffer)
	{
		return;
	}

	// 첫 번째 메시 이미터에서 Mesh와 Material 가져오기
	UStaticMesh* Mesh = nullptr;
	UMaterialInterface* Material = nullptr;
	int32 NumInstances = 0;

	for (FDynamicEmitterDataBase* EmitterData : EmitterRenderData)
	{
		if (!EmitterData)
			continue;

		const FDynamicEmitterReplayDataBase& Source = EmitterData->GetSource();
		if (Source.eEmitterType == EDynamicEmitterType::Mesh)
		{
			const auto& MeshSource = static_cast<const FDynamicMeshEmitterReplayDataBase&>(Source);
			Mesh = MeshSource.MeshData;
			Material = MeshSource.MaterialInterface;
			NumInstances += Source.ActiveParticleCount;
		}
	}

	if (!Mesh || NumInstances == 0)
	{
		return;
	}

	// Material이 없으면 메시의 내장 머터리얼에서 텍스처를 가져와 사용
	if (!Material)
	{
		// ParticleMesh 셰이더 로드
		UShader* ParticleMeshShader = UResourceManager::GetInstance().Load<UShader>("Shaders/Particle/ParticleMesh.hlsl");

		// 메시의 내장 머터리얼 확인 (StaticMeshComponent와 동일한 패턴)
		const TArray<FGroupInfo>& GroupInfos = Mesh->GetMeshGroupInfo();
		UMaterial* MeshMaterial = nullptr;

		if (!GroupInfos.IsEmpty() && !GroupInfos[0].InitialMaterialName.empty())
		{
			// 메시의 첫 번째 그룹의 머터리얼 로드
			MeshMaterial = UResourceManager::GetInstance().Load<UMaterial>(GroupInfos[0].InitialMaterialName);
		}

		// 새 파티클 머터리얼 생성 (ParticleMesh 셰이더 + 메시 텍스처)
		UMaterial* ParticleMaterial = NewObject<UMaterial>();
		ParticleMaterial->SetShader(ParticleMeshShader);

		if (MeshMaterial)
		{
			// 메시 머터리얼에서 MaterialInfo 복사 (텍스처 경로 포함)
			ParticleMaterial->SetMaterialInfo(MeshMaterial->GetMaterialInfo());
			// 텍스처 경로 기반으로 실제 텍스처 로드
			ParticleMaterial->ResolveTextures();
		}

		Material = ParticleMaterial;
	}

	if (!Material || !Material->GetShader())
	{
		return;
	}

	UShader* Shader = Material->GetShader();
	FShaderVariant* ShaderVariant = Shader->GetOrCompileShaderVariant(Material->GetShaderMacros());

	// FMeshBatchElement 생성
	FMeshBatchElement BatchElement;

	BatchElement.VertexShader = ShaderVariant->VertexShader;
	BatchElement.PixelShader = ShaderVariant->PixelShader;
	BatchElement.InputLayout = ShaderVariant->InputLayout;
	BatchElement.Material = Material;

	// StaticMesh 버퍼 사용
	BatchElement.VertexBuffer = Mesh->GetVertexBuffer();
	BatchElement.IndexBuffer = Mesh->GetIndexBuffer();
	BatchElement.VertexStride = Mesh->GetVertexStride();

	// 인스턴싱 데이터
	BatchElement.NumInstances = NumInstances;
	BatchElement.InstanceBuffer = MeshInstanceBuffer;
	BatchElement.InstanceStride = sizeof(FMeshParticleInstanceVertex);

	BatchElement.IndexCount = Mesh->GetIndexCount();
	BatchElement.StartIndex = 0;
	BatchElement.BaseVertexIndex = 0;

	// 월드 행렬은 항등 행렬 (인스턴스 버퍼에서 Transform 제공)
	BatchElement.WorldMatrix = FMatrix::Identity();
	BatchElement.ObjectID = InternalIndex;
	BatchElement.PrimitiveTopology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	// 텍스처는 Material 시스템을 통해 바인딩됨 (UberLit과 동일)
	// DrawMeshBatches에서 Material->GetTexture()를 사용하여 자동으로 바인딩

	OutMeshBatchElements.Add(BatchElement);
}

void UParticleSystemComponent::FillSpriteInstanceBuffer(uint32 TotalInstances)
{
	if (TotalInstances == 0)
	{
		return;
	}

	ID3D11Device* Device = GEngine.GetRHIDevice()->GetDevice();
	ID3D11DeviceContext* Context = GEngine.GetRHIDevice()->GetDeviceContext();

	// 인스턴스 버퍼 생성/리사이즈
	if (TotalInstances > AllocatedSpriteInstanceCount)
	{
		if (SpriteInstanceBuffer)
		{
			SpriteInstanceBuffer->Release();
			SpriteInstanceBuffer = nullptr;
		}

		uint32 NewCount = FMath::Max(TotalInstances * 2, 64u);
		D3D11_BUFFER_DESC Desc = {};
		Desc.ByteWidth = NewCount * sizeof(FSpriteParticleInstanceVertex);
		Desc.Usage = D3D11_USAGE_DYNAMIC;
		Desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		Desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

		if (SUCCEEDED(Device->CreateBuffer(&Desc, nullptr, &SpriteInstanceBuffer)))
		{
			AllocatedSpriteInstanceCount = NewCount;
		}
	}

	if (!SpriteInstanceBuffer)
	{
		return;
	}

	// 버퍼 매핑
	D3D11_MAPPED_SUBRESOURCE MappedData;
	if (FAILED(Context->Map(SpriteInstanceBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedData)))
	{
		return;
	}

	FSpriteParticleInstanceVertex* Instances = static_cast<FSpriteParticleInstanceVertex*>(MappedData.pData);
	uint32 InstanceOffset = 0;

	// 스프라이트 이미터 순회
	for (FDynamicEmitterDataBase* EmitterData : EmitterRenderData)
	{
		if (!EmitterData)
			continue;

		const FDynamicEmitterReplayDataBase& Source = EmitterData->GetSource();
		if (Source.eEmitterType != EDynamicEmitterType::Sprite)
			continue;

		const int32 ParticleCount = Source.ActiveParticleCount;
		const int32 ParticleStride = Source.ParticleStride;
		const uint8* ParticleData = Source.DataContainer.ParticleData;
		const uint16* ParticleIndices = Source.DataContainer.ParticleIndices;

		if (!ParticleData || ParticleCount == 0)
			continue;

		// 각 파티클의 인스턴스 데이터 생성
		for (int32 ParticleIdx = 0; ParticleIdx < ParticleCount; ParticleIdx++)
		{
			const int32 ParticleIndex = ParticleIndices ? ParticleIndices[ParticleIdx] : ParticleIdx;
			const FBaseParticle* Particle = reinterpret_cast<const FBaseParticle*>(
				ParticleData + ParticleIndex * ParticleStride
			);

			FSpriteParticleInstanceVertex& Instance = Instances[InstanceOffset];

			// 월드 위치
			Instance.WorldPosition =  Particle->Location;

			// 회전 (Z축만)
			Instance.Rotation = Particle->Rotation;

			// 크기 (XY만)
			Instance.Size = FVector2D(Particle->Size.X, Particle->Size.Y);

			// 색상
			Instance.Color = Particle->Color;

			// RelativeTime
			Instance.RelativeTime = Particle->RelativeTime;

			InstanceOffset++;
		}
	}

	Context->Unmap(SpriteInstanceBuffer, 0);
}

void UParticleSystemComponent::CreateSpriteParticleBatch(TArray<FMeshBatchElement>& OutMeshBatchElements)
{
	if (!SpriteInstanceBuffer)
	{
		return;
	}

	// Quad 버퍼 초기화
	InitializeQuadBuffers();

	if (!SpriteQuadVertexBuffer || !SpriteQuadIndexBuffer)
	{
		return;
	}

	// 첫 번째 스프라이트 이미터에서 Material 가져오기
	UMaterialInterface* Material = nullptr;
	int32 NumInstances = 0;

	for (FDynamicEmitterDataBase* EmitterData : EmitterRenderData)
	{
		if (!EmitterData)
			continue;

		const FDynamicEmitterReplayDataBase& Source = EmitterData->GetSource();
		if (Source.eEmitterType == EDynamicEmitterType::Sprite)
		{
			const auto& SpriteSource = static_cast<const FDynamicSpriteEmitterReplayDataBase&>(Source);
			if (!Material)
			{
				Material = SpriteSource.MaterialInterface;
			}
			NumInstances += Source.ActiveParticleCount;
		}
	}

	if (NumInstances == 0)
	{
		return;
	}

	// Material이 없으면 기본 스프라이트 셰이더 + 기본 텍스처로 생성
	if (!Material)
	{
		// ParticleSprite 셰이더 로드
		UShader* ParticleSpriteShader = UResourceManager::GetInstance().Load<UShader>("Shaders/Particle/ParticleSprite.hlsl");

		if (ParticleSpriteShader)
		{
			// 새 파티클 머터리얼 생성
			UMaterial* ParticleMaterial = NewObject<UMaterial>();
			ParticleMaterial->SetShader(ParticleSpriteShader);

			// 기본 파티클 텍스처 설정
			FMaterialInfo MatInfo;
			MatInfo.DiffuseTextureFileName = GDataDir + "/Particles/Particle_Gaussian.png";
			ParticleMaterial->SetMaterialInfo(MatInfo);
			ParticleMaterial->ResolveTextures();

			Material = ParticleMaterial;
		}
	}

	if (!Material || !Material->GetShader())
	{
		return;
	}

	UShader* Shader = Material->GetShader();
	FShaderVariant* ShaderVariant = Shader->GetOrCompileShaderVariant(Material->GetShaderMacros());

	// FMeshBatchElement 생성
	FMeshBatchElement BatchElement;

	BatchElement.VertexShader = ShaderVariant->VertexShader;
	BatchElement.PixelShader = ShaderVariant->PixelShader;
	BatchElement.InputLayout = ShaderVariant->InputLayout;
	BatchElement.Material = Material;

	// Quad 버퍼 사용
	BatchElement.VertexBuffer = SpriteQuadVertexBuffer;
	BatchElement.IndexBuffer = SpriteQuadIndexBuffer;
	BatchElement.VertexStride = sizeof(FSpriteQuadVertex);

	// 인스턴싱 데이터
	BatchElement.NumInstances = NumInstances;
	BatchElement.InstanceBuffer = SpriteInstanceBuffer;
	BatchElement.InstanceStride = sizeof(FSpriteParticleInstanceVertex);

	BatchElement.IndexCount = 6;  // 2 triangles
	BatchElement.StartIndex = 0;
	BatchElement.BaseVertexIndex = 0;

	// 월드 행렬은 항등 행렬
	BatchElement.WorldMatrix = FMatrix::Identity();
	BatchElement.ObjectID = InternalIndex;
	BatchElement.PrimitiveTopology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	OutMeshBatchElements.Add(BatchElement);
}
