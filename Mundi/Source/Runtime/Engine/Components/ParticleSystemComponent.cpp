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
#include "Modules/ParticleModuleTypeDataBeam.h"
#include "Modules/ParticleModuleTypeDataRibbon.h"
#include "Modules/ParticleModuleMeshRotation.h"
#include "Modules/ParticleModuleSize.h"
#include "Modules/ParticleModuleColor.h"
#include "Modules/ParticleModuleRotation.h"
#include "Modules/ParticleModuleRotationRate.h"
#include "Modules/ParticleModuleLocation.h"
#include "Modules/ParticleModuleEventReceiver.h"
#include "Modules/ParticleModuleAcceleration.h"
#include "Modules/ParticleModuleCollision.h"
#include "World.h"
#include "ParticleEventManager.h"

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

	Device->CreateBuffer(&VBDesc, &VBData, SpriteQuadVertexBuffer.GetAddressOf());

	// 6개 인덱스 (2개 삼각형)
	uint32 Indices[6] = { 0, 1, 2, 2, 1, 3 };

	D3D11_BUFFER_DESC IBDesc = {};
	IBDesc.ByteWidth = sizeof(Indices);
	IBDesc.Usage = D3D11_USAGE_IMMUTABLE;
	IBDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

	D3D11_SUBRESOURCE_DATA IBData = {};
	IBData.pSysMem = Indices;

	Device->CreateBuffer(&IBDesc, &IBData, SpriteQuadIndexBuffer.GetAddressOf());

	bQuadBuffersInitialized = true;
}
// Note: ReleaseQuadBuffers 제거됨 - ComPtr이 프로그램 종료 시 자동 해제

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

	// 빔 버퍼 해제
	if (BeamVertexBuffer)
	{
		BeamVertexBuffer->Release();
		BeamVertexBuffer = nullptr;
	}
	if (BeamIndexBuffer)
	{
		BeamIndexBuffer->Release();
		BeamIndexBuffer = nullptr;
	}

	// 리본 버퍼 해제
	if (RibbonVertexBuffer)
	{
		RibbonVertexBuffer->Release();
		RibbonVertexBuffer = nullptr;
	}
	if (RibbonIndexBuffer)
	{
		RibbonIndexBuffer->Release();
		RibbonIndexBuffer = nullptr;
	}

	// 캐싱된 파티클용 Material 정리
	ClearCachedMaterials();

	// 테스트용 리소스 정리 (디버그 함수에서 생성한 리소스)
	CleanupTestResources();
}

void UParticleSystemComponent::CleanupTestResources()
{
	// 테스트용 Material 정리 (Template보다 먼저 - 참조 순서)
	for (UMaterialInterface* Mat : TestMaterials)
	{
		if (Mat)
		{
			DeleteObject(Mat);
		}
	}
	TestMaterials.Empty();

	// 테스트용 Template 정리 (내부 Emitter/LODLevel/Module은 소멸자 체인으로 자동 정리)
	if (TestTemplate)
	{
		DeleteObject(TestTemplate);
		TestTemplate = nullptr;
	}
}

void UParticleSystemComponent::ClearCachedMaterials()
{
	// 캐싱된 파티클용 Material 정리
	for (auto& Pair : CachedParticleMaterials)
	{
		if (Pair.second)
		{
			DeleteObject(Pair.second);
		}
	}
	CachedParticleMaterials.Empty();
}

void UParticleSystemComponent::OnRegister(UWorld* InWorld)
{
	Super::OnRegister(InWorld);

	// 이미 초기화되어 있으면 스킵 (OnRegister는 여러 번 호출될 수 있음)
	if (EmitterInstances.Num() > 0)
	{
		return;
	}

	// Template이 없으면 파티클을 생성하지 않음 (에디터에서 리소스 선택 필요)
	if (!Template)
	{
		//CreateDebugMeshParticleSystem();        // 메시 파티클 테스트
		//CreateDebugSpriteParticleSystem();  // 스프라이트 파티클 테스트
		//CreateDebugBeamParticleSystem();		// 빔 파티클 테스트
		//CreateDebugRibbonParticleSystem(); // 리본 파티클 테스트
	}

	// 에디터에서도 파티클 미리보기를 위해 자동 활성화
	if (bAutoActivate)
	{
		ActivateSystem();
	}
}

void UParticleSystemComponent::CreateDebugMeshParticleSystem()
{
	// 디버그/테스트용 메시 파티클 시스템 생성
	// Editor 통합 완료 후에는 Editor에서 설정한 Template 사용
	TestTemplate = NewObject<UParticleSystem>();
	Template = TestTemplate;  // Template도 같이 설정 (기존 로직 호환)

	// 이미터 생성
	UParticleEmitter* Emitter = NewObject<UParticleEmitter>();

	// LOD 레벨 생성
	UParticleLODLevel* LODLevel = NewObject<UParticleLODLevel>();
	LODLevel->bEnabled = true;

	// 필수 모듈 생성 - Modules 배열에 추가
	UParticleModuleRequired* RequiredModule = NewObject<UParticleModuleRequired>();
	LODLevel->Modules.Add(RequiredModule);

	// 메시 타입 데이터 모듈 생성 - Modules 배열에 추가
	UParticleModuleTypeDataMesh* MeshTypeData = NewObject<UParticleModuleTypeDataMesh>();
	// 테스트용 메시 로드 (큐브) - ResourceManager가 관리하는 공유 리소스
	MeshTypeData->Mesh = UResourceManager::GetInstance().Load<UStaticMesh>(GDataDir + "/cube-tex.obj");
	LODLevel->Modules.Add(MeshTypeData);

	// 메시 파티클용 Material 설정 (테스트용으로 직접 생성)
	UMaterial* MeshParticleMaterial = NewObject<UMaterial>();
	TestMaterials.Add(MeshParticleMaterial);  // 소유권 등록
	UShader* MeshShader = UResourceManager::GetInstance().Load<UShader>("Shaders/Particle/ParticleMesh.hlsl");
	MeshParticleMaterial->SetShader(MeshShader);

	// 메시의 내장 머터리얼에서 텍스처 정보 가져오기
	if (MeshTypeData->Mesh)
	{
		const TArray<FGroupInfo>& GroupInfos = MeshTypeData->Mesh->GetMeshGroupInfo();
		if (!GroupInfos.IsEmpty() && !GroupInfos[0].InitialMaterialName.empty())
		{
			UMaterial* MeshMaterial = UResourceManager::GetInstance().Load<UMaterial>(GroupInfos[0].InitialMaterialName);
			if (MeshMaterial)
			{
				MeshParticleMaterial->SetMaterialInfo(MeshMaterial->GetMaterialInfo());
				MeshParticleMaterial->ResolveTextures();
			}
		}
	}
	RequiredModule->Material = MeshParticleMaterial;

	// 스폰 모듈 생성 - Modules 배열에 추가
	UParticleModuleSpawn* SpawnModule = NewObject<UParticleModuleSpawn>();
	SpawnModule->SpawnRate = FDistributionFloat(10000.0f);   // 초당 100개 파티클 (메시는 무거우므로 줄임)
	SpawnModule->BurstList.Add(FParticleBurst(100, 0.0f));   // 시작 시 100개 버스트
	SpawnModule->SpawnRate = FDistributionFloat(50.0f);       // 초당 50개 파티클 (충돌 테스트용으로 적당히)
	SpawnModule->BurstScale = FDistributionFloat(20.0f);      // 시작 시 20개 버스트
	LODLevel->Modules.Add(SpawnModule);

	// 라이프타임 모듈 생성 (파티클 수명 설정)
	UParticleModuleLifetime* LifetimeModule = NewObject<UParticleModuleLifetime>();
	LifetimeModule->Lifetime = FDistributionFloat(5.0f, 8.0f);  // 5.0 ~ 8.0초 (충돌 후 바운스 관찰용)
	LODLevel->Modules.Add(LifetimeModule);

	// 속도 모듈 생성 (위쪽으로 발사되어 중력에 의해 떨어지도록)
	UParticleModuleVelocity* VelocityModule = NewObject<UParticleModuleVelocity>();
	VelocityModule->StartVelocity = FDistributionVector(FVector(-5.0f, -5.0f, 15.0f), FVector(5.0f, 5.0f, 25.0f));  // 위쪽으로 발사
	LODLevel->Modules.Add(VelocityModule);

	// 메시 회전 모듈 생성 (3축 회전 테스트)
	UParticleModuleMeshRotation* MeshRotModule = NewObject<UParticleModuleMeshRotation>();
	// 초기 회전: Uniform 분포 (-0.5 ~ +0.5 라디안)
	MeshRotModule->StartRotation = FDistributionVector(FVector(-0.5f, -0.5f, -0.5f), FVector(0.5f, 0.5f, 0.5f));
	// 회전 속도: Uniform 분포 (라디안/초)
	MeshRotModule->StartRotationRate = FDistributionVector(FVector(0.5f, 0.2f, 1.0f), FVector(1.5f, 0.8f, 3.0f));
	LODLevel->Modules.Add(MeshRotModule);

	// 위치 모듈 생성
	UParticleModuleLocation* LocationModule = NewObject<UParticleModuleLocation>();
	LODLevel->Modules.Add(LocationModule);

	// 크기 모듈 생성
	UParticleModuleSize* SizeModule = NewObject<UParticleModuleSize>();
	LODLevel->Modules.Add(SizeModule);

	// === 충돌 테스트용 모듈 추가 ===
	// 가속도 모듈 (중력 적용)
	UParticleModuleAcceleration* AccelModule = NewObject<UParticleModuleAcceleration>();
	AccelModule->bApplyGravity = true;       // 중력 활성화
	AccelModule->GravityScale = 1.0f;        // 기본 중력 (-9.8 m/s^2)
	LODLevel->Modules.Add(AccelModule);

	// 충돌 모듈 생성
	UParticleModuleCollision* CollisionModule = NewObject<UParticleModuleCollision>();
	CollisionModule->DampingFactor = FDistributionVector(FVector(0.8f, 0.8f, 0.6f));  // 탄성 계수
	CollisionModule->MaxCollisions = FDistributionFloat(5.0f);      // 5번까지 바운스
	CollisionModule->CollisionCompletionOption = EParticleCollisionComplete::HaltCollisions;  // 5번 후 충돌 무시
	CollisionModule->ParticleRadius = 0.5f;  // 파티클 충돌 반지름
	CollisionModule->bGenerateCollisionEvents = true;  // 충돌 이벤트 생성
	LODLevel->Modules.Add(CollisionModule);

	// 모듈 캐싱
	LODLevel->CacheModuleInfo();

	// LOD 레벨을 이미터에 추가
	Emitter->LODLevels.Add(LODLevel);
	Emitter->CacheEmitterModuleInfo();

	// 이미터를 시스템에 추가
	TestTemplate->Emitters.Add(Emitter);
	// Note: ResourceManager에 등록하지 않음 - TestTemplate은 이 Component가 소유
}

void UParticleSystemComponent::CreateDebugSpriteParticleSystem()
{
	// 디버그/테스트용 스프라이트 파티클 시스템 생성
	TestTemplate = NewObject<UParticleSystem>();
	Template = TestTemplate;  // Template도 같이 설정 (기존 로직 호환)

	// 이미터 생성
	UParticleEmitter* Emitter = NewObject<UParticleEmitter>();

	// LOD 레벨 생성
	UParticleLODLevel* LODLevel = NewObject<UParticleLODLevel>();
	LODLevel->bEnabled = true;

	// 필수 모듈 생성 - Modules 배열에 추가
	UParticleModuleRequired* RequiredModule = NewObject<UParticleModuleRequired>();
	LODLevel->Modules.Add(RequiredModule);

	// 스프라이트 타입 데이터 모듈 - Modules 배열에 추가
	UParticleModuleTypeDataSprite* SpriteTypeData = NewObject<UParticleModuleTypeDataSprite>();
	LODLevel->Modules.Add(SpriteTypeData);

	// 스프라이트용 Material 설정 (테스트용으로 직접 생성)
	UMaterial* SpriteMaterial = NewObject<UMaterial>();
	TestMaterials.Add(SpriteMaterial);  // 소유권 등록
	UShader* SpriteShader = UResourceManager::GetInstance().Load<UShader>("Shaders/Particle/ParticleSprite.hlsl");
	SpriteMaterial->SetShader(SpriteShader);

	// 스프라이트 텍스처 설정 (불꽃/연기 등)
	FMaterialInfo MatInfo;
	MatInfo.DiffuseTextureFileName = GDataDir + "/Textures/Particles/Smoke.png";
	SpriteMaterial->SetMaterialInfo(MatInfo);
	SpriteMaterial->ResolveTextures();

	RequiredModule->Material = SpriteMaterial;

	// 스폰 모듈 생성 - Modules 배열에 추가
	UParticleModuleSpawn* SpawnModule = NewObject<UParticleModuleSpawn>();
	SpawnModule->SpawnRate = FDistributionFloat(5000.0f);    // 초당 50개 파티클
	SpawnModule->BurstList.Add(FParticleBurst(10000, 0.0f)); // 시작 시 10000개 버스트
	LODLevel->Modules.Add(SpawnModule);

	// 라이프타임 모듈 생성
	UParticleModuleLifetime* LifetimeModule = NewObject<UParticleModuleLifetime>();
	LifetimeModule->Lifetime = FDistributionFloat(1.5f, 2.5f);  // 1.5 ~ 2.5초 랜덤
	LODLevel->Modules.Add(LifetimeModule);

	// 속도 모듈 생성 (위쪽으로 퍼지는 연기 효과)
	UParticleModuleVelocity* VelocityModule = NewObject<UParticleModuleVelocity>();
	VelocityModule->StartVelocity = FDistributionVector(FVector(-15.0f, -15.0f, 20.0f), FVector(15.0f, 15.0f, 40.0f));  // 랜덤 범위
	LODLevel->Modules.Add(VelocityModule);

	// 크기 모듈 (시간에 따라 커지는 효과)
	UParticleModuleSize* SizeModule = NewObject<UParticleModuleSize>();
	SizeModule->StartSize = FDistributionVector(FVector(5.0f, 5.0f, 5.0f));
	SizeModule->EndSize = FDistributionVector(FVector(15.0f, 15.0f, 15.0f));
	SizeModule->bUseSizeOverLife = true;
	LODLevel->Modules.Add(SizeModule);

	// 색상 모듈 (페이드 아웃 효과 - bUpdateModule이 기본 true라 자동 보간)
	UParticleModuleColor* ColorModule = NewObject<UParticleModuleColor>();
	ColorModule->StartColor = FDistributionColor(FLinearColor(1.0f, 1.0f, 1.0f, 1.0f));  // 불투명 흰색
	ColorModule->EndColor = FDistributionColor(FLinearColor(1.0f, 1.0f, 1.0f, 0.0f));    // 투명 흰색
	LODLevel->Modules.Add(ColorModule);

	// 회전 모듈 (2D 회전)
	UParticleModuleRotation* RotationModule = NewObject<UParticleModuleRotation>();
	RotationModule->StartRotation = FDistributionFloat(-3.14159f, 3.14159f);  // 랜덤 초기 회전
	LODLevel->Modules.Add(RotationModule);

	// 회전 속도 모듈
	UParticleModuleRotationRate* RotRateModule = NewObject<UParticleModuleRotationRate>();
	RotRateModule->StartRotationRate = FDistributionFloat(3.0f);  // 천천히 회전
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

void UParticleSystemComponent::CreateDebugBeamParticleSystem()
{
	// 디버그/테스트용 빔 파티클 시스템 생성
	TestTemplate = NewObject<UParticleSystem>();
	Template = TestTemplate;

	// 이미터 생성
	UParticleEmitter* Emitter = NewObject<UParticleEmitter>();

	// LOD 레벨 생성
	UParticleLODLevel* LODLevel = NewObject<UParticleLODLevel>();
	LODLevel->bEnabled = true;

	// 필수 모듈 생성 (Modules 배열에 추가)
	UParticleModuleRequired* RequiredModule = NewObject<UParticleModuleRequired>();
	LODLevel->Modules.Add(RequiredModule);

	// 빔용 Material 설정 (나중에 빔 셰이더로 교체)
	UMaterial* BeamMaterial = NewObject<UMaterial>();
	UShader* BeamShader = UResourceManager::GetInstance().Load<UShader>("Shaders/Particle/ParticleBeam.hlsl");
	BeamMaterial->SetShader(BeamShader);
	RequiredModule->Material = BeamMaterial;

	// 빔 타입 데이터 모듈 생성 (Modules 배열에 추가)
	UParticleModuleTypeDataBeam* BeamTypeData = NewObject<UParticleModuleTypeDataBeam>();
	BeamTypeData->SegmentCount = 6;
	BeamTypeData->BeamWidth = 1.0f;
	BeamTypeData->NoiseStrength = 1.0f;
	LODLevel->Modules.Add(BeamTypeData);

	// 스폰 모듈 생성 - 빔은 최소 2개의 파티클(시작/끝)이 필요 (Modules 배열에 추가)
	UParticleModuleSpawn* SpawnModule = NewObject<UParticleModuleSpawn>();
	SpawnModule->SpawnRate = FDistributionFloat(0.0f);    // 연속 스폰 안함
	SpawnModule->BurstList.Add(FParticleBurst(2, 0.0f));  // 시작 시 2개(시작점, 끝점) 버스트
	LODLevel->Modules.Add(SpawnModule);

	// 라이프타임 모듈 생성 (빔의 전체 수명)
	UParticleModuleLifetime* LifetimeModule = NewObject<UParticleModuleLifetime>();
	LifetimeModule->Lifetime = FDistributionFloat(0.0f, 0.0f);
	LODLevel->Modules.Add(LifetimeModule);

	// 위치 모듈 생성
	UParticleModuleLocation* LocationModule = NewObject<UParticleModuleLocation>();
	LODLevel->Modules.Add(LocationModule);

	// 속도 모듈 생성
	UParticleModuleVelocity* VelocityModule = NewObject<UParticleModuleVelocity>();
	VelocityModule->StartVelocity = FDistributionVector(FVector(0.0f, 0.0f, 0.0f), FVector(0.0f, 0.0f, 0.0f));
	LODLevel->Modules.Add(VelocityModule);

	// 모듈 캐싱
	LODLevel->CacheModuleInfo();

	// LOD 레벨을 이미터에 추가
	Emitter->LODLevels.Add(LODLevel);
	Emitter->CacheEmitterModuleInfo();

	// 이미터를 시스템에 추가
	TestTemplate->Emitters.Add(Emitter);
	// Note: ResourceManager에 등록하지 않음 - TestTemplate은 이 Component가 소유
}

void UParticleSystemComponent::CreateDebugRibbonParticleSystem()
{
	// 디버그/테스트용 리본 파티클 시스템 생성
	TestTemplate = NewObject<UParticleSystem>();
	Template = TestTemplate;

	// 이미터 생성
	UParticleEmitter* Emitter = NewObject<UParticleEmitter>();

	// LOD 레벨 생성
	UParticleLODLevel* LODLevel = NewObject<UParticleLODLevel>();
	LODLevel->bEnabled = true;

	// 필수 모듈 생성
	UParticleModuleRequired* RequiredModule = NewObject<UParticleModuleRequired>();
	LODLevel->Modules.Add(RequiredModule);

	// 리본용 Material 설정
	UMaterial* RibbonMaterial = NewObject<UMaterial>();
	TestMaterials.Add(RibbonMaterial); // 소유권 등록
	UShader* RibbonShader = UResourceManager::GetInstance().Load<UShader>("Shaders/Particle/ParticleRibbon.hlsl");
	RibbonMaterial->SetShader(RibbonShader);
	RequiredModule->Material = RibbonMaterial;

	// 리본 타입 데이터 모듈 생성
	UParticleModuleTypeDataRibbon* RibbonTypeData = NewObject<UParticleModuleTypeDataRibbon>();
	RibbonTypeData->RibbonWidth = 1.0f;
	LODLevel->Modules.Add(RibbonTypeData);

	// 스폰 모듈 생성 - 리본은 연속적인 파티클 스트림이 필요
	UParticleModuleSpawn* SpawnModule = NewObject<UParticleModuleSpawn>();
	SpawnModule->SpawnRate = FDistributionFloat(30.0f);  // 초당 30개 (부드러운 트레일)
	SpawnModule->BurstList.Add(FParticleBurst(1, 0.0f));  // 시작 시 1개 버스트
	LODLevel->Modules.Add(SpawnModule);

	// 라이프타임 모듈 생성
	UParticleModuleLifetime* LifetimeModule = NewObject<UParticleModuleLifetime>();
	LifetimeModule->Lifetime = FDistributionFloat(1.0f);  // 1초 생존 (짧은 트레일)
	LODLevel->Modules.Add(LifetimeModule);

	// 속도 모듈 생성 - 트레일 효과를 위해 속도 0 (파티클이 생성된 위치에 고정)
	UParticleModuleVelocity* VelocityModule = NewObject<UParticleModuleVelocity>();
	VelocityModule->StartVelocity = FDistributionVector(FVector(0.0f, 0.0f, 0.0f));  // 속도 0 = 트레일
	LODLevel->Modules.Add(VelocityModule);
    
    // 위치 모듈 생성 (기즈모 조작 지원)
    UParticleModuleLocation* LocationModule = NewObject<UParticleModuleLocation>();
    LODLevel->Modules.Add(LocationModule);

	// 모듈 캐싱
	LODLevel->CacheModuleInfo();

	// LOD 레벨을 이미터에 추가
	Emitter->LODLevels.Add(LODLevel);
	Emitter->CacheEmitterModuleInfo();

	// 이미터를 시스템에 추가
	TestTemplate->Emitters.Add(Emitter);
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

	// 다이나믹 버퍼 정리
	if (BeamVertexBuffer)
	{
		BeamVertexBuffer->Release();
		BeamVertexBuffer = nullptr;
	}
	AllocatedBeamVertexCount = 0;

	if (BeamIndexBuffer)
	{
		BeamIndexBuffer->Release();
		BeamIndexBuffer = nullptr;
	}
	AllocatedBeamIndexCount = 0;

	// 리본 버퍼 정리
	if (RibbonVertexBuffer)
	{
		RibbonVertexBuffer->Release();
		RibbonVertexBuffer = nullptr;
	}
	AllocatedRibbonVertexCount = 0;

	if (RibbonIndexBuffer)
	{
		RibbonIndexBuffer->Release();
		RibbonIndexBuffer = nullptr;
	}
	AllocatedRibbonIndexCount = 0;

	// 캐싱된 파티클용 Material 정리
	ClearCachedMaterials();

	Super::OnUnregister();
}

void UParticleSystemComponent::TickComponent(float DeltaTime)
{
	USceneComponent::TickComponent(DeltaTime);

	// DeltaTime 제한 (에디터 로딩/탭 전환 시 스파이크 방지)
	// 10fps 미만은 비정상적인 상황으로 간주
	const float MaxDeltaTime = 0.1f;
	DeltaTime = FMath::Min(DeltaTime, MaxDeltaTime);

	// 이벤트 클리어 (매 프레임 시작 시)
	ClearEvents();

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

	// 내부 이벤트 디스패치 (같은 PSC 내의 다른 이미터들에게 이벤트 전달)
	DispatchEventsToReceivers();

	// 외부 이벤트 브로드캐스트 (ParticleEventManager를 통해 다른 PSC에도 전달)
	if (UWorld* World = GetWorld())
	{
		if (AParticleEventManager* EventMgr = World->GetParticleEventManager())
		{
			for (const FParticleEventCollideData& Event : CollisionEvents)
			{
				EventMgr->BroadcastCollisionEvent(this, Event);
			}
			for (const FParticleEventData& Event : SpawnEvents)
			{
				EventMgr->BroadcastSpawnEvent(this, Event);
			}
			for (const FParticleEventData& Event : DeathEvents)
			{
				EventMgr->BroadcastDeathEvent(this, Event);
			}
		}
	}
}

// === 파티클 이벤트 시스템 ===
void UParticleSystemComponent::ClearEvents()
{
	CollisionEvents.Empty();
	SpawnEvents.Empty();
	DeathEvents.Empty();
}

void UParticleSystemComponent::AddCollisionEvent(const FParticleEventCollideData& Event)
{
	CollisionEvents.Add(Event);
}

void UParticleSystemComponent::AddSpawnEvent(const FParticleEventData& Event)
{
	SpawnEvents.Add(Event);
}

void UParticleSystemComponent::AddDeathEvent(const FParticleEventData& Event)
{
	DeathEvents.Add(Event);
}

void UParticleSystemComponent::DispatchEventsToReceivers()
{
	// 이벤트가 없으면 스킵
	if (CollisionEvents.IsEmpty() && SpawnEvents.IsEmpty() && DeathEvents.IsEmpty())
	{
		return;
	}

	// this 컴포넌트 파괴 상태 체크 (언리얼 방식)
	if (IsPendingDestroy())
	{
		return;
	}

	// 각 이미터 인스턴스의 EventReceiver 모듈에 이벤트 전달
	for (FParticleEmitterInstance* Instance : EmitterInstances)
	{
		// 매 반복마다 this 파괴 상태 재확인
		if (IsPendingDestroy())
		{
			return;
		}

		if (!Instance || !Instance->CurrentLODLevel)
		{
			continue;
		}

		// Instance->Component 파괴 상태 체크
		if (!Instance->Component || Instance->Component->IsPendingDestroy())
		{
			continue;
		}

		// LODLevel의 모듈 중 EventReceiver 찾기
		for (UParticleModule* Module : Instance->CurrentLODLevel->Modules)
		{
			UParticleModuleEventReceiverBase* Receiver = Cast<UParticleModuleEventReceiverBase>(Module);
			if (!Receiver)
			{
				continue;
			}

			// 충돌 이벤트 전달 (EventName 필터링 적용)
			if (Receiver->EventType == EParticleEventType::Collision || Receiver->EventType == EParticleEventType::Any)
			{
				for (const FParticleEventCollideData& Event : CollisionEvents)
				{
					// EventName 필터링: Receiver의 EventName이 비어있으면 모두 수신
					if (Receiver->EventName.empty() || Receiver->EventName == Event.EventName)
					{
						Receiver->HandleCollisionEvent(Instance, Event);
					}
				}
			}

			// 스폰 이벤트 전달 (EventName 필터링 적용)
			if (Receiver->EventType == EParticleEventType::Spawn || Receiver->EventType == EParticleEventType::Any)
			{
				for (const FParticleEventData& Event : SpawnEvents)
				{
					if (Receiver->EventName.empty() || Receiver->EventName == Event.EventName)
					{
						Receiver->HandleEvent(Instance, Event);
					}
				}
			}

			// 사망 이벤트 전달 (EventName 필터링 적용)
			if (Receiver->EventType == EParticleEventType::Death || Receiver->EventType == EParticleEventType::Any)
			{
				for (const FParticleEventData& Event : DeathEvents)
				{
					if (Receiver->EventName.empty() || Receiver->EventName == Event.EventName)
					{
						Receiver->HandleEvent(Instance, Event);
					}
				}
			}
		}
	}
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

		// 기존 이미터 인스턴스가 있으면 정리
		if (EmitterInstances.Num() > 0)
		{
			ClearEmitterInstances();
		}

		// 새 Template이 유효하고 World에 등록되어 있으면 초기화
		if (Template && GetWorld())
		{
			InitializeEmitterInstances();
		}
	}
}

void UParticleSystemComponent::SetEditorLODLevel(int32 LODLevel)
{
	for (FParticleEmitterInstance* Instance : EmitterInstances)
	{
		if (Instance)
		{
			Instance->SetLODLevel(LODLevel);
		}
	}
}

void UParticleSystemComponent::SetLODLevel(int32 NewLODLevel)
{
	if (!Template || CurrentLODLevel == NewLODLevel)
		return;

	// LOD 레벨 클램프
	int32 MaxLOD = 0;
	for (UParticleEmitter* Emitter : Template->Emitters)
	{
		if (Emitter)
			MaxLOD = FMath::Max(MaxLOD, Emitter->LODLevels.Num() - 1);
	}
	NewLODLevel = FMath::Clamp(NewLODLevel, 0, MaxLOD);

	if (CurrentLODLevel == NewLODLevel)
		return;

	CurrentLODLevel = NewLODLevel;

	// 각 EmitterInstance의 LOD 레벨 변경
	for (FParticleEmitterInstance* Instance : EmitterInstances)
	{
		if (Instance)
		{
			Instance->SetLODLevel(CurrentLODLevel);
		}
	}
}

void UParticleSystemComponent::UpdateLODLevels(const FVector& CameraPosition)
{
	if (!Template || !Template->bUseLOD || Template->LODDistances.IsEmpty())
		return;

	// 카메라와의 거리 계산
	FVector ComponentPosition = GetWorldLocation();
	float Distance = (CameraPosition - ComponentPosition).Size();

	// 거리 기반 LOD 레벨 결정
	int32 NewLOD = 0;
	for (int32 i = 0; i < Template->LODDistances.Num(); i++)
	{
		if (Distance > Template->LODDistances[i])
			NewLOD = i + 1;
	}

	// LOD 변경
	if (NewLOD != CurrentLODLevel)
	{
		SetLODLevel(NewLOD);
	}
}

void UParticleSystemComponent::RefreshEmitterInstances()
{
	// 동일한 템플릿의 내용이 변경되었을 때 EmitterInstances 재생성
	ClearEmitterInstances();
	ClearCachedMaterials();  // Material 캐시도 클리어 (텍스처 변경 반영)

	// 인스턴스 버퍼도 클리어 (TypeData 변경 시 stride 불일치 방지)
	// 스프라이트 -> 메시 전환 시, 이전 프레임의 스프라이트 버퍼(48바이트 stride)가
	// 메시 셰이더의 Input Layout(68바이트 기대)과 불일치하는 것을 방지
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

	InitializeEmitterInstances();

	// EmitterRenderData도 즉시 갱신 (이전 프레임의 오래된 데이터 사용 방지)
	// 이렇게 하지 않으면 TypeData나 Material 변경 후 첫 프레임에서
	// stride 불일치 경고가 발생할 수 있음
	UpdateRenderData();
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

	// 테스트용 리소스 포인터 초기화 (원본 소유, 복사본에서 삭제하면 안됨)
	TestTemplate = nullptr;
	TestMaterials.Empty();

	// 캐싱된 Material도 원본 소유이므로 비움 (delete 하지 않음)
	CachedParticleMaterials.Empty();
}

void UParticleSystemComponent::CollectMeshBatches(TArray<FMeshBatchElement>& OutMeshBatchElements, const FSceneView* View)
{
	// 0. 런타임 LOD 업데이트 (카메라 거리 기반)
	if (View)
	{
		UpdateLODLevels(View->ViewLocation);
	}

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
			FVector ViewDirection = View ? View->ViewRotation.GetForwardVector() : FVector(1.0f, 0.0f, 0.0f);
			SpriteData->SortSpriteParticles(Source.SortMode, ViewOrigin, ViewDirection);
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
		CreateMeshParticleBatch(OutMeshBatchElements, View);
	}

	// 5. 빔 파티클 처리 (동적 메시 생성)
	uint32 TotalBeamVertices = 0;
	uint32 TotalBeamIndices = 0;
	for (FDynamicEmitterDataBase* EmitterData : EmitterRenderData)
	{
		if (!EmitterData)
			continue;

		const FDynamicEmitterReplayDataBase& Source = EmitterData->GetSource();
		if (Source.eEmitterType == EDynamicEmitterType::Beam)
		{
			const auto& BeamSource = static_cast<const FDynamicBeamEmitterReplayDataBase&>(Source);
			if (BeamSource.BeamPoints.Num() > 1)
			{
				const int32 SegmentCount = BeamSource.BeamPoints.Num() - 1;
				TotalBeamVertices += SegmentCount * 4;      // 각 세그먼트마다 4개의 정점
				TotalBeamIndices += SegmentCount * 6;      // 각 세그먼트마다 2개의 삼각형 (6개의 인덱스)
			}
		}
	}

	if (TotalBeamVertices > 0 && TotalBeamIndices > 0)
	{
		FillBeamBuffers(View);
		CreateBeamParticleBatch(OutMeshBatchElements);
	}

	// 6. 리본 파티클 처리 (동적 메시 생성)
	uint32 TotalRibbonVertices = 0;
	uint32 TotalRibbonIndices = 0;
	for (FDynamicEmitterDataBase* EmitterData : EmitterRenderData)
	{
		if (!EmitterData)
			continue;

		const FDynamicEmitterReplayDataBase& Source = EmitterData->GetSource();
		if (Source.eEmitterType == EDynamicEmitterType::Ribbon)
		{
			const auto& RibbonSource = static_cast<const FDynamicRibbonEmitterReplayDataBase&>(Source);
			if (RibbonSource.RibbonPoints.Num() > 1)
			{
				const int32 NumPoints = RibbonSource.RibbonPoints.Num();
				TotalRibbonVertices += NumPoints * 2;      // 각 포인트마다 2개의 정점 (스트립)
				TotalRibbonIndices += (NumPoints - 1) * 6; // 각 세그먼트마다 2개의 삼각형 (6개의 인덱스)
			}
		}
	}

	if (TotalRibbonVertices > 0 && TotalRibbonIndices > 0)
	{
		FillRibbonBuffers(View);
		CreateRibbonParticleBatch(OutMeshBatchElements);
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

void UParticleSystemComponent::CreateMeshParticleBatch(TArray<FMeshBatchElement>& OutMeshBatchElements, const FSceneView* View)
{
	if (!MeshInstanceBuffer)
	{
		return;
	}

	// 파티클 메시 전용 인스턴싱 셰이더 로드 (한 번만)
	static UShader* ParticleMeshShader = UResourceManager::GetInstance().Load<UShader>("Shaders/Particle/ParticleMesh.hlsl");
	if (!ParticleMeshShader)
	{
		return;
	}

	// 인스턴스 버퍼 내 현재 오프셋 (각 이미터별로 증가)
	uint32 InstanceOffset = 0;

	// 각 메시 이미터별로 별도의 배치 생성
	for (FDynamicEmitterDataBase* EmitterData : EmitterRenderData)
	{
		if (!EmitterData)
			continue;

		const FDynamicEmitterReplayDataBase& Source = EmitterData->GetSource();
		if (Source.eEmitterType != EDynamicEmitterType::Mesh)
			continue;

		const auto& MeshSource = static_cast<const FDynamicMeshEmitterReplayDataBase&>(Source);
		UStaticMesh* Mesh = MeshSource.MeshData;
		UMaterialInterface* Material = MeshSource.MaterialInterface;
		bool bOverrideMaterial = MeshSource.bOverrideMaterial;
		int32 EmitterInstanceCount = Source.ActiveParticleCount;

		if (!Mesh || EmitterInstanceCount == 0)
		{
			continue;
		}

		// 메시의 섹션(GroupInfo) 정보 가져오기
		const TArray<FGroupInfo>& MeshGroupInfos = Mesh->GetMeshGroupInfo();
		const bool bHasSections = !MeshGroupInfos.IsEmpty();
		const uint32 NumSectionsToProcess = bHasSections ? static_cast<uint32>(MeshGroupInfos.size()) : 1;

		// Lambda: 섹션별 소스 Material 가져오기
		// bOverrideMaterial이 true면 RequiredModule->Material 사용, false면 메시의 섹션별 Material 사용
		auto GetSourceMaterial = [&](uint32 SectionIndex) -> UMaterialInterface*
		{
			if (bOverrideMaterial && Material != nullptr)
			{
				return Material;
			}

			if (bHasSections && SectionIndex < MeshGroupInfos.size())
			{
				const FGroupInfo& Group = MeshGroupInfos[SectionIndex];
				if (!Group.InitialMaterialName.empty())
				{
					UMaterialInterface* MeshMaterial = UResourceManager::GetInstance().Get<UMaterial>(Group.InitialMaterialName);
					if (MeshMaterial)
					{
						return MeshMaterial;
					}
				}
			}

			return UResourceManager::GetInstance().GetDefaultMaterial();
		};

		// 각 섹션마다 별도의 배치 생성
		for (uint32 SectionIndex = 0; SectionIndex < NumSectionsToProcess; ++SectionIndex)
		{
			uint32 IndexCount = 0;
			uint32 StartIndex = 0;

			if (bHasSections)
			{
				const FGroupInfo& Group = MeshGroupInfos[SectionIndex];
				IndexCount = Group.IndexCount;
				StartIndex = Group.StartIndex;
			}
			else
			{
				IndexCount = Mesh->GetIndexCount();
				StartIndex = 0;
			}

			if (IndexCount == 0)
			{
				continue;
			}

			// 소스 Material 가져오기
			UMaterialInterface* SourceMaterial = GetSourceMaterial(SectionIndex);
			if (!SourceMaterial)
			{
				continue;
			}

			// 캐싱된 Material 확인, 없으면 새로 생성
			// (캐시는 RefreshEmitterInstances() 호출 시 클리어됨)
			UMaterial* ParticleMaterial = nullptr;
			UMaterial** CachedMaterial = CachedParticleMaterials.Find(SourceMaterial);
			if (CachedMaterial && *CachedMaterial)
			{
				ParticleMaterial = *CachedMaterial;
			}
			else
			{
				ParticleMaterial = UMaterial::CreateWithShaderOverride(SourceMaterial, ParticleMeshShader);
				if (ParticleMaterial)
				{
					CachedParticleMaterials.Add(SourceMaterial, ParticleMaterial);
				}
			}

			if (!ParticleMaterial)
			{
				continue;
			}

			// 셰이더 변형 컴파일
			TArray<FShaderMacro> ShaderMacros = View->ViewShaderMacros;
			if (ParticleMaterial->GetShaderMacros().Num() > 0)
			{
				ShaderMacros.Append(ParticleMaterial->GetShaderMacros());
			}
			FShaderVariant* ShaderVariant = ParticleMeshShader->GetOrCompileShaderVariant(ShaderMacros);

			if (!ShaderVariant)
			{
				continue;
			}

			// FMeshBatchElement 생성
			FMeshBatchElement BatchElement;

			BatchElement.VertexShader = ShaderVariant->VertexShader;
			BatchElement.PixelShader = ShaderVariant->PixelShader;
			BatchElement.InputLayout = ShaderVariant->InputLayout;
			BatchElement.Material = ParticleMaterial;

			BatchElement.VertexBuffer = Mesh->GetVertexBuffer();
			BatchElement.IndexBuffer = Mesh->GetIndexBuffer();
			BatchElement.VertexStride = Mesh->GetVertexStride();

			// 이 이미터의 인스턴스 수와 시작 위치 설정
			BatchElement.NumInstances = EmitterInstanceCount;
			BatchElement.InstanceBuffer = MeshInstanceBuffer;
			BatchElement.InstanceStride = sizeof(FMeshParticleInstanceVertex);
			BatchElement.StartInstanceLocation = InstanceOffset;

			BatchElement.IndexCount = IndexCount;
			BatchElement.StartIndex = StartIndex;
			BatchElement.BaseVertexIndex = 0;

			BatchElement.WorldMatrix = FMatrix::Identity();
			BatchElement.ObjectID = InternalIndex;
			BatchElement.PrimitiveTopology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
			BatchElement.RenderMode = EBatchRenderMode::Opaque;

			OutMeshBatchElements.Add(BatchElement);
		}

		// 다음 이미터를 위해 오프셋 증가
		InstanceOffset += EmitterInstanceCount;
	}
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

		// Sub-UV 설정 가져오기
		const FDynamicSpriteEmitterData* SpriteEmitterData = static_cast<const FDynamicSpriteEmitterData*>(EmitterData);
		int32 SubImages_H = 1;
		int32 SubImages_V = 1;
		int32 TotalFrames = 1;

		if (SpriteEmitterData->Source.RequiredModule)
		{
			SubImages_H = SpriteEmitterData->Source.RequiredModule->SubImages_Horizontal;
			SubImages_V = SpriteEmitterData->Source.RequiredModule->SubImages_Vertical;
			int32 MaxElements = SpriteEmitterData->Source.RequiredModule->SubUV_MaxElements;
			TotalFrames = (MaxElements > 0) ? MaxElements : (SubImages_H * SubImages_V);
		}

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

			// Sub-UV 프레임 인덱스 계산
			// RelativeTime (0~1)을 프레임 인덱스 (0~TotalFrames-1)로 변환
			Instance.SubImageIndex = Particle->RelativeTime * (float)(TotalFrames - 1);

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

	// 인스턴스 버퍼 내 현재 오프셋 (각 이미터별로 증가)
	uint32 InstanceOffset = 0;

	// 각 스프라이트 이미터별로 별도의 배치 생성
	for (FDynamicEmitterDataBase* EmitterData : EmitterRenderData)
	{
		if (!EmitterData)
			continue;

		const FDynamicEmitterReplayDataBase& Source = EmitterData->GetSource();
		if (Source.eEmitterType != EDynamicEmitterType::Sprite)
			continue;

		const auto& SpriteSource = static_cast<const FDynamicSpriteEmitterReplayDataBase&>(Source);
		UMaterialInterface* Material = SpriteSource.MaterialInterface;
		int32 EmitterInstanceCount = Source.ActiveParticleCount;

		// Material이 없거나 파티클이 없으면 스킵
		if (!Material || EmitterInstanceCount == 0)
		{
			continue;
		}

		if (!Material->GetShader())
		{
			continue;
		}

		UShader* Shader = Material->GetShader();
		FShaderVariant* ShaderVariant = Shader->GetOrCompileShaderVariant(Material->GetShaderMacros());

		if (!ShaderVariant)
		{
			continue;
		}

		// FMeshBatchElement 생성
		FMeshBatchElement BatchElement;

		BatchElement.VertexShader = ShaderVariant->VertexShader;
		BatchElement.PixelShader = ShaderVariant->PixelShader;
		BatchElement.InputLayout = ShaderVariant->InputLayout;
		BatchElement.Material = Material;

		// Quad 버퍼 사용 (ComPtr에서 raw 포인터 추출)
		BatchElement.VertexBuffer = SpriteQuadVertexBuffer.Get();
		BatchElement.IndexBuffer = SpriteQuadIndexBuffer.Get();
		BatchElement.VertexStride = sizeof(FSpriteQuadVertex);

		// 이 이미터의 인스턴스 수와 시작 위치 설정
		BatchElement.NumInstances = EmitterInstanceCount;
		BatchElement.InstanceBuffer = SpriteInstanceBuffer;
		BatchElement.InstanceStride = sizeof(FSpriteParticleInstanceVertex);
		BatchElement.StartInstanceLocation = InstanceOffset;

		BatchElement.IndexCount = 6;  // 2 triangles
		BatchElement.StartIndex = 0;
		BatchElement.BaseVertexIndex = 0;

		// 월드 행렬은 항등 행렬
		BatchElement.WorldMatrix = FMatrix::Identity();
		BatchElement.ObjectID = InternalIndex;
		BatchElement.PrimitiveTopology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

		// 스프라이트 파티클: 반투명 렌더링 (no culling, depth read-only, alpha blend)
		BatchElement.RenderMode = EBatchRenderMode::Translucent;

		// Sub-UV 설정 (스프라이트 시트 정보)
		if (SpriteSource.RequiredModule)
		{
			float SubH = (float)SpriteSource.RequiredModule->SubImages_Horizontal;
			float SubV = (float)SpriteSource.RequiredModule->SubImages_Vertical;
			// xy = 타일 수, zw = 1/타일 수 (프레임 크기)
			BatchElement.SubImageSize = FVector4(SubH, SubV, 1.0f / SubH, 1.0f / SubV);
		}

		OutMeshBatchElements.Add(BatchElement);

		// 다음 이미터를 위해 오프셋 증가
		InstanceOffset += EmitterInstanceCount;
	}
}

void UParticleSystemComponent::FillBeamBuffers(const FSceneView* View)
{
	if (!View)
	{
		return;
	}

	// 1. 필요한 총 정점 및 인덱스 수 계산
	uint32 TotalVertices = 0;
	uint32 TotalIndices = 0;
	for (FDynamicEmitterDataBase* EmitterData : EmitterRenderData)
	{
		if (EmitterData && EmitterData->GetSource().eEmitterType == EDynamicEmitterType::Beam)
		{
			const auto& BeamSource = static_cast<const FDynamicBeamEmitterReplayDataBase&>(EmitterData->GetSource());
			const int32 NumPoints = BeamSource.BeamPoints.Num();
			if (NumPoints > 1)
			{
				const int32 SegmentCount = NumPoints - 1;
				TotalVertices += (SegmentCount + 1) * 2;
				TotalIndices += SegmentCount * 6;
			}
		}
	}

	if (TotalVertices == 0 || TotalIndices == 0)
	{
		return;
	}

	ID3D11Device* Device = GEngine.GetRHIDevice()->GetDevice();
	ID3D11DeviceContext* Context = GEngine.GetRHIDevice()->GetDeviceContext();

	// 2. 버퍼 생성/리사이즈
	if (TotalVertices > AllocatedBeamVertexCount)
	{
		if (BeamVertexBuffer) BeamVertexBuffer->Release();
		uint32 NewCount = FMath::Max(TotalVertices * 2, 64u);
		D3D11_BUFFER_DESC Desc = {};
		Desc.ByteWidth = NewCount * sizeof(FParticleBeamVertex);
		Desc.Usage = D3D11_USAGE_DYNAMIC;
		Desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		Desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		if (SUCCEEDED(Device->CreateBuffer(&Desc, nullptr, &BeamVertexBuffer)))
		{
			AllocatedBeamVertexCount = NewCount;
		}
	}

	if (TotalIndices > AllocatedBeamIndexCount)
	{
		if (BeamIndexBuffer) BeamIndexBuffer->Release();
		uint32 NewCount = FMath::Max(TotalIndices * 2, 128u);
		D3D11_BUFFER_DESC Desc = {};
		Desc.ByteWidth = NewCount * sizeof(uint32);
		Desc.Usage = D3D11_USAGE_DYNAMIC;
		Desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
		Desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		if (SUCCEEDED(Device->CreateBuffer(&Desc, nullptr, &BeamIndexBuffer)))
		{
			AllocatedBeamIndexCount = NewCount;
		}
	}

	if (!BeamVertexBuffer || !BeamIndexBuffer)
	{
		return;
	}

	// 3. 버퍼 매핑
	D3D11_MAPPED_SUBRESOURCE VBMappedData, IBMappedData;
	if (FAILED(Context->Map(BeamVertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &VBMappedData)) ||
		FAILED(Context->Map(BeamIndexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &IBMappedData)))
	{
		if (VBMappedData.pData) Context->Unmap(BeamVertexBuffer, 0);
		return;
	}

	auto* Vertices = static_cast<FParticleBeamVertex*>(VBMappedData.pData);
	auto* Indices = static_cast<uint32*>(IBMappedData.pData);

	uint32 VertexOffset = 0;
	uint32 IndexOffset = 0;

	FVector ViewDirection = View->ViewRotation.GetForwardVector();
	FVector ViewOrigin = View->ViewLocation;

	// 4. 이미터 순회하며 버퍼 채우기
	for (FDynamicEmitterDataBase* EmitterData : EmitterRenderData)
	{
		if (!EmitterData || EmitterData->GetSource().eEmitterType != EDynamicEmitterType::Beam)
			continue;

		const auto& BeamSource = static_cast<const FDynamicBeamEmitterReplayDataBase&>(EmitterData->GetSource());
		const TArray<FVector>& BeamPoints = BeamSource.BeamPoints;
		const float BeamWidth = BeamSource.Width;
		const float TileU = BeamSource.TileU;
		const FLinearColor BeamColor = BeamSource.Color;

		if (BeamPoints.Num() < 2)
			continue;

		uint32 EmitterBaseVertexIndex = VertexOffset;

		// 1. 모든 정점을 먼저 생성 (Triangle Strip 방식)
		for (int32 i = 0; i < BeamPoints.Num(); ++i)
		{
			const FVector& P = BeamPoints[i];
			FVector SegmentDir = (i < BeamPoints.Num() - 1) ? (BeamPoints[i + 1] - P) : (P - BeamPoints[i - 1]);
			SegmentDir.Normalize();

			FVector Up = FVector::Cross(SegmentDir, ViewDirection);
			Up.Normalize();
			
			float HalfWidth = BeamWidth * 0.5f;

			// UV의 V좌표는 빔의 길이에 따라 0에서 1까지 변함
			float V = (float)i / (float)(BeamPoints.Num() - 1);
			
			// 각 포인트마다 2개의 정점(좌, 우) 생성
			Vertices[VertexOffset++] = { P - Up * HalfWidth, FVector2D(0.0f, V), BeamColor, BeamWidth };
			Vertices[VertexOffset++] = { P + Up * HalfWidth, FVector2D(1.0f, V), BeamColor, BeamWidth };
		}

		// 2. 정점들을 연결하여 인덱스 생성
		for (int32 i = 0; i < BeamPoints.Num() - 1; ++i)
		{
			uint32 V0 = EmitterBaseVertexIndex + i * 2;     // 현재 세그먼트의 좌측 정점
			uint32 V1 = EmitterBaseVertexIndex + i * 2 + 1; // 현재 세그먼트의 우측 정점
			uint32 V2 = EmitterBaseVertexIndex + (i + 1) * 2;     // 다음 세그먼트의 좌측 정점
			uint32 V3 = EmitterBaseVertexIndex + (i + 1) * 2 + 1; // 다음 세그먼트의 우측 정점

			// 첫 번째 삼각형 (V0, V2, V1)
			Indices[IndexOffset++] = V0;
			Indices[IndexOffset++] = V2;
			Indices[IndexOffset++] = V1;

			// 두 번째 삼각형 (V1, V2, V3)
			Indices[IndexOffset++] = V1;
			Indices[IndexOffset++] = V2;
			Indices[IndexOffset++] = V3;
		}
	}

	Context->Unmap(BeamVertexBuffer, 0);
	Context->Unmap(BeamIndexBuffer, 0);
}

void UParticleSystemComponent::CreateBeamParticleBatch(TArray<FMeshBatchElement>& OutMeshBatchElements)
{
	if (!BeamVertexBuffer || !BeamIndexBuffer)
	{
		return;
	}

	uint32 VertexOffset = 0;
	uint32 IndexOffset = 0;

	for (FDynamicEmitterDataBase* EmitterData : EmitterRenderData)
	{
		if (!EmitterData || EmitterData->GetSource().eEmitterType != EDynamicEmitterType::Beam)
			continue;

		const auto& BeamSource = static_cast<const FDynamicBeamEmitterReplayDataBase&>(EmitterData->GetSource());
		if (BeamSource.BeamPoints.Num() < 2)
			continue;

		UMaterialInterface* Material = BeamSource.Material;
		if (!Material)
		{
			Material = UResourceManager::GetInstance().Load<UMaterial>("Shaders/Particle/ParticleBeam.hlsl");
		}

		if (!Material || !Material->GetShader())
		{
			continue;
		}

		UShader* Shader = Material->GetShader();
		FShaderVariant* ShaderVariant = Shader->GetOrCompileShaderVariant(Material->GetShaderMacros());

		const int32 SegmentCount = BeamSource.BeamPoints.Num() - 1;
		const uint32 NumVertices = (SegmentCount + 1) * 2;
		const uint32 NumIndices = SegmentCount * 6;

		FMeshBatchElement BatchElement;
		BatchElement.VertexShader = ShaderVariant->VertexShader;
		BatchElement.PixelShader = ShaderVariant->PixelShader;
		BatchElement.InputLayout = ShaderVariant->InputLayout;
		BatchElement.Material = Material;

		BatchElement.VertexBuffer = BeamVertexBuffer;
		BatchElement.IndexBuffer = BeamIndexBuffer;
		BatchElement.VertexStride = sizeof(FParticleBeamVertex);

		BatchElement.NumInstances = 1; // Not instanced
		BatchElement.InstanceBuffer = nullptr;
		BatchElement.InstanceStride = 0;

		BatchElement.IndexCount = NumIndices;
		BatchElement.StartIndex = IndexOffset;
		BatchElement.BaseVertexIndex = VertexOffset;

		BatchElement.WorldMatrix = FMatrix::Identity();
		BatchElement.ObjectID = InternalIndex;
		BatchElement.PrimitiveTopology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

		// 빔 파티클: 반투명 렌더링 (no culling, depth read-only, alpha blend)
		BatchElement.RenderMode = EBatchRenderMode::Translucent;

		OutMeshBatchElements.Add(BatchElement);

		// Update offsets for the next beam
		VertexOffset += NumVertices;
		IndexOffset += NumIndices;
	}
}

void UParticleSystemComponent::FillRibbonBuffers(const FSceneView* View)
{
	if (!View)
	{
		return;
	}

	// 1. 필요한 총 정점 및 인덱스 수 계산
	uint32 TotalVertices = 0;
	uint32 TotalIndices = 0;
	for (FDynamicEmitterDataBase* EmitterData : EmitterRenderData)
	{
		if (EmitterData && EmitterData->GetSource().eEmitterType == EDynamicEmitterType::Ribbon)
		{
			const auto& RibbonSource = static_cast<const FDynamicRibbonEmitterReplayDataBase&>(EmitterData->GetSource());
			const int32 NumPoints = RibbonSource.RibbonPoints.Num();
			if (NumPoints > 1)
			{
				TotalVertices += NumPoints * 2;
				TotalIndices += (NumPoints - 1) * 6;
			}
		}
	}

	if (TotalVertices == 0 || TotalIndices == 0)
	{
		return;
	}

	ID3D11Device* Device = GEngine.GetRHIDevice()->GetDevice();
	ID3D11DeviceContext* Context = GEngine.GetRHIDevice()->GetDeviceContext();

	// 2. 버퍼 생성/리사이즈
	if (TotalVertices > AllocatedRibbonVertexCount)
	{
		if (RibbonVertexBuffer) RibbonVertexBuffer->Release();
		uint32 NewCount = FMath::Max(TotalVertices * 2, 128u);
		D3D11_BUFFER_DESC Desc = {};
		Desc.ByteWidth = NewCount * sizeof(FParticleRibbonVertex);
		Desc.Usage = D3D11_USAGE_DYNAMIC;
		Desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		Desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		if (SUCCEEDED(Device->CreateBuffer(&Desc, nullptr, &RibbonVertexBuffer)))
		{
			AllocatedRibbonVertexCount = NewCount;
		}
	}

	if (TotalIndices > AllocatedRibbonIndexCount)
	{
		if (RibbonIndexBuffer) RibbonIndexBuffer->Release();
		uint32 NewCount = FMath::Max(TotalIndices * 2, 256u);
		D3D11_BUFFER_DESC Desc = {};
		Desc.ByteWidth = NewCount * sizeof(uint32);
		Desc.Usage = D3D11_USAGE_DYNAMIC;
		Desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
		Desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		if (SUCCEEDED(Device->CreateBuffer(&Desc, nullptr, &RibbonIndexBuffer)))
		{
			AllocatedRibbonIndexCount = NewCount;
		}
	}

	if (!RibbonVertexBuffer || !RibbonIndexBuffer)
	{
		return;
	}

	// 3. 버퍼 매핑
	D3D11_MAPPED_SUBRESOURCE VBMappedData, IBMappedData;
	if (FAILED(Context->Map(RibbonVertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &VBMappedData)) ||
		FAILED(Context->Map(RibbonIndexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &IBMappedData)))
	{
		if (VBMappedData.pData) Context->Unmap(RibbonVertexBuffer, 0);
		if (IBMappedData.pData) Context->Unmap(RibbonIndexBuffer, 0);
		return;
	}

	auto* Vertices = static_cast<FParticleRibbonVertex*>(VBMappedData.pData);
	auto* Indices = static_cast<uint32*>(IBMappedData.pData);

	uint32 VertexOffset = 0;
	uint32 IndexOffset = 0;

	FVector ViewDirection = View->ViewRotation.GetForwardVector();
	
	// 4. 이미터 순회하며 버퍼 채우기
	for (FDynamicEmitterDataBase* EmitterData : EmitterRenderData)
	{
		if (!EmitterData || EmitterData->GetSource().eEmitterType != EDynamicEmitterType::Ribbon)
			continue;
	
		const auto& RibbonSource = static_cast<const FDynamicRibbonEmitterReplayDataBase&>(EmitterData->GetSource());
		const TArray<FVector>& RibbonPoints = RibbonSource.RibbonPoints;
		const TArray<FLinearColor>& RibbonColors = RibbonSource.RibbonColors;
		const float RibbonWidth = RibbonSource.Width;
		const int32 NumPoints = RibbonPoints.Num();

		if (NumPoints < 2 || RibbonColors.Num() != NumPoints)
			continue;

		uint32 EmitterBaseVertexIndex = VertexOffset;

		// 1. 모든 정점을 먼저 생성 (Triangle Strip 방식)
		for (int32 i = 0; i < NumPoints; ++i)
		{
			const FVector& P = RibbonPoints[i];
			const FLinearColor& Color = RibbonColors[i];  // 파티클 색상 (페이드 아웃 포함)

			FVector SegmentDir;
			if (i < NumPoints - 1)
			{
				SegmentDir = RibbonPoints[i + 1] - P;
			}
			else
			{
				SegmentDir = P - RibbonPoints[i - 1];
			}
			SegmentDir.Normalize();

			FVector Up = FVector::Cross(SegmentDir, ViewDirection);
			Up.Normalize();

			// UV의 V좌표는 리본의 길이에 따라 0에서 1까지 변함
			float V = (float)i / (float)(NumPoints - 1);

			// 테이퍼링: 끝으로 갈수록 너비 감소 (뾰족한 끝 방지)
			// V=0 (오래된 파티클, 트레일 끝) → 너비 0
			// V=1 (새로운 파티클, 트레일 시작) → 너비 100%
			float WidthScale = V;  // 선형 테이퍼링 (V*V or sqrt(V))
			float HalfWidth = (RibbonWidth * 0.5f) * WidthScale;

			// 각 포인트마다 2개의 정점(좌, 우) 생성
			Vertices[VertexOffset++] = {
				P - Up * HalfWidth,		// Position
				P,						// ControlPoint
				SegmentDir,				// Tangent
				Color,					// Color (페이드 아웃 알파 포함)
				FVector2D(0.0f, V)		// UV
			};
			Vertices[VertexOffset++] = {
				P + Up * HalfWidth,		// Position
				P,						// ControlPoint
				SegmentDir,				// Tangent
				Color,					// Color (페이드 아웃 알파 포함)
				FVector2D(1.0f, V)		// UV
			};
		}		
		// 2. 정점들을 연결하여 인덱스 생성
		for (int32 i = 0; i < NumPoints - 1; ++i)
		{
			uint32 V0 = EmitterBaseVertexIndex + i * 2;
			uint32 V1 = V0 + 1;
			uint32 V2 = EmitterBaseVertexIndex + (i + 1) * 2;
			uint32 V3 = V2 + 1;

			Indices[IndexOffset++] = V0;
			Indices[IndexOffset++] = V2;
			Indices[IndexOffset++] = V1;

			Indices[IndexOffset++] = V1;
			Indices[IndexOffset++] = V2;
			Indices[IndexOffset++] = V3;
		}
	}

	Context->Unmap(RibbonVertexBuffer, 0);
	Context->Unmap(RibbonIndexBuffer, 0);
}

void UParticleSystemComponent::CreateRibbonParticleBatch(TArray<FMeshBatchElement>& OutMeshBatchElements)
{
	if (!RibbonVertexBuffer || !RibbonIndexBuffer)
	{
		return;
	}

	uint32 VertexOffset = 0;
	uint32 IndexOffset = 0;

	for (FDynamicEmitterDataBase* EmitterData : EmitterRenderData)
	{
		if (!EmitterData || EmitterData->GetSource().eEmitterType != EDynamicEmitterType::Ribbon)
			continue;

		const auto& RibbonSource = static_cast<const FDynamicRibbonEmitterReplayDataBase&>(EmitterData->GetSource());
		const int32 NumPoints = RibbonSource.RibbonPoints.Num();

		if (NumPoints < 2)
			continue;

		UMaterialInterface* Material = RibbonSource.Material;
		if (!Material)
		{
			// Fallback to a default material if none is set
			Material = UResourceManager::GetInstance().Load<UMaterial>("Shaders/Particle/ParticleRibbon.hlsl");
		}

		if (!Material || !Material->GetShader())
		{
			continue;
		}

		UShader* Shader = Material->GetShader();
		FShaderVariant* ShaderVariant = Shader->GetOrCompileShaderVariant(Material->GetShaderMacros());

		const uint32 NumVerticesForThisRibbon = NumPoints * 2;
		const uint32 NumIndicesForThisRibbon = (NumPoints - 1) * 6;

		FMeshBatchElement BatchElement;
		BatchElement.VertexShader = ShaderVariant->VertexShader;
		BatchElement.PixelShader = ShaderVariant->PixelShader;
		BatchElement.InputLayout = ShaderVariant->InputLayout;
		BatchElement.Material = Material;

		BatchElement.VertexBuffer = RibbonVertexBuffer;
		BatchElement.IndexBuffer = RibbonIndexBuffer;
		BatchElement.VertexStride = sizeof(FParticleRibbonVertex);

		BatchElement.NumInstances = 1; // Not instanced
		BatchElement.InstanceBuffer = nullptr;
		BatchElement.InstanceStride = 0;

		BatchElement.IndexCount = NumIndicesForThisRibbon;
		BatchElement.StartIndex = IndexOffset;
		BatchElement.BaseVertexIndex = VertexOffset;

		BatchElement.WorldMatrix = FMatrix::Identity();
		BatchElement.ObjectID = InternalIndex;
		BatchElement.PrimitiveTopology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

		// 리본 파티클: 반투명 렌더링 (알파 블렌딩, 깊이 읽기 전용)
		BatchElement.RenderMode = EBatchRenderMode::Translucent;

		OutMeshBatchElements.Add(BatchElement);

		// Update offsets for the next ribbon emitter
		VertexOffset += NumVerticesForThisRibbon;
		IndexOffset += NumIndicesForThisRibbon;
	}
}