#pragma once

#include "ParticleModule.h"
#include "Distribution.h"
#include "ParticleEventTypes.h"
#include "UParticleModuleCollision.generated.h"

// 충돌 페이로드 (파티클별 충돌 상태)
struct FParticleCollisionPayload
{
	FVector UsedDampingFactor;      // 12B - 실제 적용된 감쇠값
	int32 UsedCollisionCount;        // 4B - 현재 충돌 횟수
	float DelayTimer;                // 4B - 딜레이 타이머
	float Padding[3];                // 12B - 16바이트 정렬
	// 총 32바이트
};

UCLASS(DisplayName="충돌 모듈", Description="파티클과 ShapeComponent 간의 충돌을 처리하는 모듈입니다")
class UParticleModuleCollision : public UParticleModule
{
public:
	GENERATED_REFLECTION_BODY()

public:
	// 충돌 시 속도 감쇠 계수 (0~1, 각 축별로)
	// X,Y: 표면과 평행한 감쇠 (마찰), Z: 법선 방향 감쇠 (탄성)
	UPROPERTY(EditAnywhere, Category="Collision", meta=(ToolTip="충돌 시 속도 감쇠 (0=완전 흡수, 1=완전 반사)"))
	FDistributionVector DampingFactor = FDistributionVector(FVector(0.5f, 0.5f, 0.3f));

	// 최대 충돌 횟수 (이 횟수에 도달하면 CollisionCompletionOption 적용)
	UPROPERTY(EditAnywhere, Category="Collision", meta=(ClampMin="1", ToolTip="최대 충돌 횟수"))
	int32 MaxCollisions = 1;

	// MaxCollisions 도달 시 동작
	UPROPERTY(EditAnywhere, Category="Collision", meta=(ToolTip="최대 충돌 도달 시 동작"))
	EParticleCollisionComplete CollisionCompletionOption = EParticleCollisionComplete::Kill;

	// 충돌 검사 시작 전 딜레이 (초)
	UPROPERTY(EditAnywhere, Category="Collision", meta=(ClampMin="0.0", ToolTip="충돌 검사 시작 전 딜레이 (초)"))
	float DelayAmount = 0.0f;

	// 파티클 충돌 반지름
	UPROPERTY(EditAnywhere, Category="Collision", meta=(ClampMin="0.1", ToolTip="파티클 충돌 반지름"))
	float ParticleRadius = 5.0f;

	// 충돌 이벤트 생성 여부
	UPROPERTY(EditAnywhere, Category="Collision", meta=(ToolTip="충돌 시 이벤트를 생성할지 여부"))
	bool bGenerateCollisionEvents = true;

	UParticleModuleCollision()
	{
		bSpawnModule = true;   // 스폰 시 페이로드 초기화
		bUpdateModule = true;  // 매 프레임 충돌 검사
	}

	virtual ~UParticleModuleCollision() = default;

	// 페이로드 크기
	virtual uint32 RequiredBytes(FParticleEmitterInstance* Owner = nullptr) override
	{
		return sizeof(FParticleCollisionPayload);
	}

	// 스폰 시 페이로드 초기화
	virtual void Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle* ParticleBase) override;

	// 매 프레임 충돌 검사
	virtual void Update(FModuleUpdateContext& Context) override;

	// 직렬화
	virtual void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;

private:
	// 충돌 완료 처리 (MaxCollisions 도달 시)
	void HandleCollisionComplete(FBaseParticle& Particle, FParticleCollisionPayload& Payload);

	// 반사 속도 계산
	void ApplyDamping(FBaseParticle& Particle, const FParticleCollisionPayload& Payload, const FVector& Normal);
};
