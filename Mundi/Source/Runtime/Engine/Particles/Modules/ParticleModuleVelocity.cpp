#include "pch.h"
#include "ParticleModuleVelocity.h"
#include "ParticleEmitterInstance.h"  // BEGIN_UPDATE_LOOP 매크로에서 FParticleEmitterInstance 정의 필요
#include "ParticleSystemComponent.h"

// 언리얼 엔진 호환: 페이로드 크기 반환
uint32 UParticleModuleVelocity::RequiredBytes(FParticleEmitterInstance* Owner)
{
	return sizeof(FParticleVelocityPayload);
}

void UParticleModuleVelocity::Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle* ParticleBase)
{
	if (!ParticleBase || !Owner || !Owner->Component)
	{
		return;
	}

	// 언리얼 엔진 호환: CurrentOffset 초기화
	uint32 CurrentOffset = Offset;

	// 랜덤 속도 오프셋
	static std::random_device rd;
	static std::mt19937 gen(rd());
	std::uniform_real_distribution<float> dist(-1.0f, 1.0f);

	FVector RandomOffset(
		dist(gen) * StartVelocityRange.X,
		dist(gen) * StartVelocityRange.Y,
		dist(gen) * StartVelocityRange.Z
	);

	// 로컬 공간에서 초기 속도 계산 + 월드 공간 속도로 변환
	const FTransform& ComponentTransform = Owner->Component->GetWorldTransform();
	FVector LocalInitialVel = StartVelocity + RandomOffset;
	FVector WorldInitialVel = ComponentTransform.TransformVector(LocalInitialVel);

	// 월드 공간 속도를 파티클에 적용
	ParticleBase->Velocity = WorldInitialVel;
	ParticleBase->BaseVelocity = WorldInitialVel;

	// 언리얼 엔진 호환: PARTICLE_ELEMENT 매크로 사용 (CurrentOffset 자동 증가)
	PARTICLE_ELEMENT(FParticleVelocityPayload, VelPayload);

	// 페이로드 설정
	VelPayload.InitialVelocity = WorldInitialVel;
	VelPayload.VelocityMagnitude = WorldInitialVel.Size();
}

// 언리얼 엔진 호환: Context를 사용한 업데이트 (페이로드 시스템 예시)
void UParticleModuleVelocity::Update(FModuleUpdateContext& Context)
{
	// VelocityDamping이 0이면 업데이트 불필요
	if (VelocityDamping <= 0.0f)
	{
		return;
	}

	// BEGIN_UPDATE_LOOP/END_UPDATE_LOOP 매크로 사용
	// 언리얼 엔진 표준 패턴: 역방향 순회, Freeze 자동 스킵
	BEGIN_UPDATE_LOOP
		// 언리얼 엔진 호환: PARTICLE_ELEMENT 매크로 (CurrentOffset 자동 증가)
		PARTICLE_ELEMENT(FParticleVelocityPayload, VelPayload);

		// 속도 감쇠 적용 (OverLife 패턴)
		float DampingFactor = 1.0f - (VelocityDamping * DeltaTime);
		if (DampingFactor < 0.0f)
		{
			DampingFactor = 0.0f;
		}

		// 파티클 속도 감쇠
		Particle.Velocity = Particle.Velocity * DampingFactor;
		Particle.BaseVelocity = Particle.BaseVelocity * DampingFactor;

		// 페이로드에 현재 속도 크기 업데이트
		VelPayload.VelocityMagnitude = std::sqrt(
			Particle.Velocity.X * Particle.Velocity.X +
			Particle.Velocity.Y * Particle.Velocity.Y +
			Particle.Velocity.Z * Particle.Velocity.Z
		);
	END_UPDATE_LOOP
}

void UParticleModuleVelocity::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	UParticleModule::Serialize(bInIsLoading, InOutHandle);
	// UPROPERTY 속성은 자동으로 직렬화됨
}
