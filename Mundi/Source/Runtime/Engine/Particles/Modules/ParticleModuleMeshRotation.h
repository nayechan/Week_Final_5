#pragma once

#include "ParticleModule.h"
#include "UParticleModuleMeshRotation.generated.h"

// 메시 파티클용 3D 회전 페이로드 (48바이트)
// 스프라이트는 Z축만 사용하지만, 메시는 3축 회전 필요
struct FMeshRotationPayload
{
	FVector InitialRotation;    // 초기 회전값 (라디안) - X:Roll, Y:Pitch, Z:Yaw
	FVector Rotation;           // 현재 회전값 (라디안) - X:Roll, Y:Pitch, Z:Yaw
	FVector RotationRate;       // 회전 속도 (라디안/초)
	float Padding[3];           // 16바이트 정렬
	// 총 48바이트
};

UCLASS(DisplayName="메시 초기 회전", Description="메시 파티클의 3축 초기 회전값을 설정하는 모듈입니다")
class UParticleModuleMeshRotation : public UParticleModule
{
public:
	GENERATED_REFLECTION_BODY()

public:
	// 시작 회전값 (라디안) - X: Roll, Y: Pitch, Z: Yaw
	UPROPERTY(EditAnywhere, Category="Rotation")
	FVector StartRotation = FVector(0.0f, 0.0f, 0.0f);

	// 회전 랜덤 변화량 (라디안)
	// 최종 회전 = StartRotation + Random(-Randomness, +Randomness)
	UPROPERTY(EditAnywhere, Category="Rotation")
	FVector RotationRandomness = FVector(0.0f, 0.0f, 0.0f);

	// 회전 속도 (라디안/초)
	UPROPERTY(EditAnywhere, Category="Rotation")
	FVector StartRotationRate = FVector(0.0f, 0.0f, 0.0f);

	// 회전 속도 랜덤 변화량
	UPROPERTY(EditAnywhere, Category="Rotation")
	FVector RotationRateRandomness = FVector(0.0f, 0.0f, 0.0f);

	UParticleModuleMeshRotation()
	{
		bSpawnModule = true;   // Spawn 시 초기화 필요
		bUpdateModule = true;  // Update에서 회전 적용
	}
	virtual ~UParticleModuleMeshRotation() = default;

	// 파티클별 페이로드 크기 반환
	virtual uint32 RequiredBytes(FParticleEmitterInstance* Owner = nullptr) override
	{
		return sizeof(FMeshRotationPayload);
	}

	// 메시 회전을 건드리는 모듈임을 표시
	virtual bool TouchesMeshRotation() const { return true; }

	virtual void Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle* ParticleBase) override;
	virtual void Update(FModuleUpdateContext& Context) override;
	virtual void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;
};
