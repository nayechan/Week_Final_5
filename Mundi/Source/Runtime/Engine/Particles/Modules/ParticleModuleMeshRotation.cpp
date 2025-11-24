#include "pch.h"
#include "ParticleModuleMeshRotation.h"
#include "ParticleEmitterInstance.h"

void UParticleModuleMeshRotation::Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle* ParticleBase)
{
	if (!ParticleBase)
	{
		return;
	}

	// CurrentOffset 초기화
	uint32 CurrentOffset = Offset;

	// 페이로드 가져오기
	PARTICLE_ELEMENT(FMeshRotationPayload, Payload);

	// 랜덤 오프셋 계산 (-Randomness ~ +Randomness)
	auto RandomRange = [](float Range) -> float
	{
		return (static_cast<float>(rand()) / RAND_MAX * 2.0f - 1.0f) * Range;
	};

	// 초기 회전값 설정 (StartRotation + Random)
	Payload.InitialRotation = StartRotation;
	Payload.Rotation = FVector(
		StartRotation.X + RandomRange(RotationRandomness.X),
		StartRotation.Y + RandomRange(RotationRandomness.Y),
		StartRotation.Z + RandomRange(RotationRandomness.Z)
	);

	// 회전 속도 설정 (StartRotationRate + Random)
	Payload.RotationRate = FVector(
		StartRotationRate.X + RandomRange(RotationRateRandomness.X),
		StartRotationRate.Y + RandomRange(RotationRateRandomness.Y),
		StartRotationRate.Z + RandomRange(RotationRateRandomness.Z)
	);

	// 기본 Z축 회전도 동기화 (스프라이트 호환)
	ParticleBase->Rotation = Payload.Rotation.Z;
	ParticleBase->RotationRate = Payload.RotationRate.Z;
}

void UParticleModuleMeshRotation::Update(FModuleUpdateContext& Context)
{
	BEGIN_UPDATE_LOOP;
		// 페이로드 가져오기
		PARTICLE_ELEMENT(FMeshRotationPayload, Payload);

		// 회전 속도 적용
		Payload.Rotation.X += Payload.RotationRate.X * DeltaTime;
		Payload.Rotation.Y += Payload.RotationRate.Y * DeltaTime;
		Payload.Rotation.Z += Payload.RotationRate.Z * DeltaTime;

		// 기본 Z축 회전도 동기화
		Particle.Rotation = Payload.Rotation.Z;
	END_UPDATE_LOOP;
}

void UParticleModuleMeshRotation::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	UParticleModule::Serialize(bInIsLoading, InOutHandle);

	// UPROPERTY 속성은 리플렉션 시스템에 의해 자동으로 직렬화됨
}
