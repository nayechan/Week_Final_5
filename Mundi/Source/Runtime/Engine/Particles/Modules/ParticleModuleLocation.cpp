#include "pch.h"
#include "ParticleModuleLocation.h"
#include "ParticleEmitterInstance.h"
#include "ParticleSystemComponent.h"

void UParticleModuleLocation::Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle* ParticleBase)
{
	if (!ParticleBase || !Owner || !Owner->Component)
	{
		return;
	}
	const FTransform& ComponentTransform = Owner->Component->GetWorldTransform();

	// 언리얼 엔진 호환: CurrentOffset 초기화
	uint32 CurrentOffset = Offset;

	// 페이로드 가져오기
	PARTICLE_ELEMENT(FParticleLocationPayload, Payload);

	// 분포 형태에 따라 랜덤 위치 생성
	FVector LocalRandomOffset;
	switch (static_cast<ELocationDistributionShape>(DistributionShape))
	{
	case ELocationDistributionShape::Sphere:
		LocalRandomOffset = GenerateRandomLocationSphere();
		break;
	case ELocationDistributionShape::Cylinder:
		LocalRandomOffset = GenerateRandomLocationCylinder();
		break;
	case ELocationDistributionShape::Box:
	default:
		LocalRandomOffset = GenerateRandomLocationBox();
		break;
	}

	// 총 로컬 오프셋을 컴포넌트의 월드 변환을 적용해 최종 월드 위치로 변환
	FVector TotalLocalOffset = StartLocation + LocalRandomOffset;
	FVector FinalWorldPosition = ComponentTransform.TransformPosition(TotalLocalOffset);

	// 최종 스폰 위치 계산
	Payload.SpawnLocation = FinalWorldPosition;
	ParticleBase->Location = Payload.SpawnLocation;
	ParticleBase->OldLocation = Payload.SpawnLocation;
}

FVector UParticleModuleLocation::GenerateRandomLocationBox()
{
	// 박스 형태: 각 축별로 -Range ~ +Range 범위에서 균등 분포
	return FVector(
		(static_cast<float>(rand()) / RAND_MAX * 2.0f - 1.0f) * StartLocationRange.X,
		(static_cast<float>(rand()) / RAND_MAX * 2.0f - 1.0f) * StartLocationRange.Y,
		(static_cast<float>(rand()) / RAND_MAX * 2.0f - 1.0f) * StartLocationRange.Z
	);
}

FVector UParticleModuleLocation::GenerateRandomLocationSphere()
{
	// 언리얼 엔진 호환: 구 형태 분포
	// 균등한 구 분포를 위해 구좌표계 사용
	float Theta = static_cast<float>(rand()) / RAND_MAX * 2.0f * 3.14159265f;  // 0 ~ 2π
	float Phi = acosf(2.0f * static_cast<float>(rand()) / RAND_MAX - 1.0f);    // 0 ~ π (균등 분포)

	float Radius;
	if (bSurfaceOnly)
	{
		// 표면에만 스폰
		Radius = SphereRadius;
	}
	else
	{
		// 부피 전체에 균등 분포 (r^3 보정)
		float RandomValue = static_cast<float>(rand()) / RAND_MAX;
		Radius = SphereRadius * powf(RandomValue, 1.0f / 3.0f);
	}

	// 구좌표 -> 직교좌표 변환
	float SinPhi = sinf(Phi);
	return FVector(
		Radius * SinPhi * cosf(Theta),
		Radius * SinPhi * sinf(Theta),
		Radius * cosf(Phi)
	);
}

FVector UParticleModuleLocation::GenerateRandomLocationCylinder()
{
	// 언리얼 엔진 호환: 실린더 형태 분포 (Z축 기준)
	float Theta = static_cast<float>(rand()) / RAND_MAX * 2.0f * 3.14159265f;  // 0 ~ 2π

	float Radius;
	if (bSurfaceOnly)
	{
		// 원통 표면에만 스폰
		Radius = SphereRadius;
	}
	else
	{
		// 원통 부피 전체에 균등 분포 (r^2 보정)
		float RandomValue = static_cast<float>(rand()) / RAND_MAX;
		Radius = SphereRadius * sqrtf(RandomValue);
	}

	// Z 높이는 -CylinderHeight/2 ~ +CylinderHeight/2 범위
	float Height = (static_cast<float>(rand()) / RAND_MAX - 0.5f) * CylinderHeight;

	return FVector(
		Radius * cosf(Theta),
		Radius * sinf(Theta),
		Height
	);
}

void UParticleModuleLocation::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	UParticleModule::Serialize(bInIsLoading, InOutHandle);
	// UPROPERTY 속성은 자동으로 직렬화됨
}
