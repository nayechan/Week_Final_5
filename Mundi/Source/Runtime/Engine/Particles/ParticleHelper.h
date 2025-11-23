#pragma once

#include "ParticleDefinitions.h"

// 전방 선언
struct FParticleEmitterInstance;

// 언리얼 엔진 호환: 모듈 업데이트 컨텍스트 구조체
// 매개변수 전달을 간소화하고 확장성을 높임
struct FModuleUpdateContext
{
	FParticleEmitterInstance& Owner;      // 이미터 인스턴스 참조
	int32                     Offset;     // 파티클 데이터 오프셋
	float                     DeltaTime;  // 델타 타임
};

// 파티클 데이터에서 파티클 포인터를 선언하는 헬퍼 매크로 (언리얼 엔진 호환)
// SpawnParticles 내부에서 사용 (ParticleBase를 Particle로 캐스팅)
// 사용법: DECLARE_PARTICLE_PTR(Particle, ParticleBase);
#define DECLARE_PARTICLE_PTR(Name, Address) \
	FBaseParticle* Name = (FBaseParticle*)Address

// 언리얼 엔진 호환: 파티클 페이로드 데이터 접근 매크로
// 모듈별 추가 데이터를 파티클에서 가져오는 헬퍼
// CurrentOffset을 자동으로 증가시키므로 순차적으로 여러 페이로드를 읽을 때 편리
// 사용법: PARTICLE_ELEMENT(FParticleVelocityPayload, VelData);
#define PARTICLE_ELEMENT(Type, Name) \
	Type& Name = *((Type*)((uint8*)ParticleBase + CurrentOffset)); \
	CurrentOffset += sizeof(Type);

// 파티클 업데이트 루프를 시작하는 헬퍼 매크로 (언리얼 엔진 완전 호환)
// Context 구조체를 사용하여 매개변수 전달
// 역방향 순회로 파티클 제거 시 안전성 확보
// Freeze 상태의 파티클은 자동으로 스킵
#define BEGIN_UPDATE_LOOP \
	{ \
		int32&            ActiveParticles  = Context.Owner.ActiveParticles; \
		int32             Offset           = Context.Offset; \
		uint32            CurrentOffset    = Offset; \
		float             DeltaTime        = Context.DeltaTime; \
		const uint8*      ParticleData     = Context.Owner.ParticleData; \
		const uint32      ParticleStride   = Context.Owner.ParticleStride; \
		uint16*           ParticleIndices  = Context.Owner.ParticleIndices; \
		for(int32 i=ActiveParticles-1; i>=0; i--) \
		{ \
			const int32    CurrentIndex = ParticleIndices[i]; \
			const uint8*   ParticleBase = ParticleData + CurrentIndex * ParticleStride; \
			FBaseParticle& Particle     = *((FBaseParticle*) ParticleBase); \
			if ((Particle.Flags & STATE_Particle_Freeze) == 0) \
			{

// 파티클 업데이트 루프를 종료하는 헬퍼 매크로
#define END_UPDATE_LOOP \
			} \
			CurrentOffset = Offset; \
		} \
	}

// 파티클 기본 크기를 가져오는 헬퍼 함수
inline FVector GetParticleBaseSize(const FBaseParticle& Particle)
{
	return Particle.BaseSize;
}

// 언리얼 엔진 호환: 인덱스로 파티클을 가져오는 헬퍼 함수
// 구현은 ParticleEmitterInstance.h에서 정의 (순환 의존성 방지)
inline FBaseParticle* GetParticleAtIndex(FParticleEmitterInstance* Instance, int32 Index);
