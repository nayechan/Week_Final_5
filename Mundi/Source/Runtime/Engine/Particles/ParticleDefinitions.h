#pragma once

#include "Vector.h"
#include "Color.h"
#include "VertexData.h"

class UMaterialInterface;

// 언리얼 엔진 호환: 렌더링에 필요한 필수 모듈 데이터
// 렌더 스레드에서 안전하게 접근할 수 있도록 데이터를 복사
struct FParticleRequiredModule
{
	UMaterialInterface* Material;       // 파티클 렌더링에 사용할 머티리얼
	FString EmitterName;                // 이미터 이름 (디버깅용)
	int32 ScreenAlignment;              // 화면 정렬 방식 (Billboard, Velocity aligned 등)
	bool bOrientZAxisTowardCamera;      // Z축을 카메라 방향으로 정렬할지 여부

	FParticleRequiredModule()
		: Material(nullptr)
		, EmitterName("Emitter")
		, ScreenAlignment(0)
		, bOrientZAxisTowardCamera(false)
	{
	}
};

// 파티클 상태 플래그 (언리얼 엔진 호환)
#define STATE_Particle_JustSpawned            0x02000000  // 방금 생성된 파티클
#define STATE_Particle_Freeze                 0x04000000  // 파티클 업데이트 무시
#define STATE_Particle_IgnoreCollisions       0x08000000  // 충돌 업데이트 무시
#define STATE_Particle_FreezeTranslation      0x10000000  // 파티클 이동 중지
#define STATE_Particle_FreezeRotation         0x20000000  // 파티클 회전 중지
#define STATE_Particle_CollisionIgnoreCheck   (STATE_Particle_Freeze | STATE_Particle_IgnoreCollisions | STATE_Particle_FreezeTranslation | STATE_Particle_FreezeRotation)
#define STATE_Particle_DelayCollisions        0x40000000  // 충돌 업데이트 지연
#define STATE_Particle_CollisionHasOccurred   0x80000000  // 충돌 발생 플래그
#define STATE_Mask                            0xFE000000  // 상태 마스크
#define STATE_CounterMask                     (~STATE_Mask) // 카운터 마스크

// 동적 이미터 타입
enum class EDynamicEmitterType : uint8
{
	Sprite = 0,
	Mesh = 1,
	Unknown = 255
};

// 기본 파티클 구조체 (언리얼 엔진 완전 호환)
struct FBaseParticle
{
	// 48바이트 - 위치 정보
	FVector      OldLocation;        // 충돌 처리용 이전 프레임 위치
	FVector      Location;            // 현재 위치

	// 16바이트
	FVector      BaseVelocity;        // 매 프레임 시작 시 속도 (Velocity가 리셋되는 기준값)
	float        Rotation;            // 파티클 회전 (라디안)

	// 16바이트
	FVector      Velocity;            // 현재 속도, 매 프레임 BaseVelocity로 리셋됨
	float        BaseRotationRate;    // 초기 각속도 (라디안/초)

	// 16바이트
	FVector      BaseSize;            // 매 프레임 시작 시 크기 (Size가 리셋되는 기준값)
	float        RotationRate;        // 현재 회전 속도, 매 프레임 BaseRotationRate로 리셋됨

	// 16바이트
	FVector      Size;                // 현재 크기, 매 프레임 BaseSize로 리셋됨
	int32        Flags;               // 파티클 상태 플래그

	// 16바이트
	FLinearColor Color;               // 현재 색상

	// 16바이트
	FLinearColor BaseColor;           // 기본 색상

	// 16바이트
	float        RelativeTime;        // 상대 시간, 0 (생성) ~ 1 (소멸)
	float        OneOverMaxLifetime;  // 수명의 역수 (나눗셈 최적화용: RelativeTime += DeltaTime * OneOverMaxLifetime)
	float        Placeholder0;        // 향후 확장용
	float        Placeholder1;        // 향후 확장용

	FBaseParticle()
		: OldLocation(FVector(0.0f, 0.0f, 0.0f))
		, Location(FVector(0.0f, 0.0f, 0.0f))
		, BaseVelocity(FVector(0.0f, 0.0f, 0.0f))
		, Rotation(0.0f)
		, Velocity(FVector(0.0f, 0.0f, 0.0f))
		, BaseRotationRate(0.0f)
		, BaseSize(FVector(1.0f, 1.0f, 1.0f))
		, RotationRate(0.0f)
		, Size(FVector(1.0f, 1.0f, 1.0f))
		, Flags(0)
		, Color(FLinearColor(1.0f, 1.0f, 1.0f, 1.0f))
		, BaseColor(FLinearColor(1.0f, 1.0f, 1.0f, 1.0f))
		, RelativeTime(0.0f)
		, OneOverMaxLifetime(1.0f)
		, Placeholder0(0.0f)
		, Placeholder1(0.0f)
	{
	}
};

struct FParticleSpriteVertex
{
	FVector WorldPosition;
	float Rotation;
	FVector2D UV;
	FVector2D Size;
	FLinearColor Color;
	float RelativeTime;
};

// 스프라이트 파티클 인스턴싱용 인스턴스 데이터 (48바이트)
// 하나의 쿼드 메시를 모든 파티클이 공유, 인스턴스별 데이터만 전송
struct FSpriteParticleInstanceVertex
{
	FVector WorldPosition;    // 파티클 중심 위치 (12바이트)
	float Rotation;           // Z축 회전 (라디안) (4바이트)
	FVector2D Size;           // 파티클 크기 (8바이트)
	FLinearColor Color;       // 파티클 색상 (16바이트)
	float RelativeTime;       // 상대 시간 (4바이트)
	float Padding[1];         // 16바이트 정렬 (4바이트)
	// 총 48바이트
};

// 추후 GPU 인스턴싱을 위한 인스턴스 데이터 구조체
// 현재는 메시 복제 방식을 사용하지만, 인스턴싱 전환 시 활용
struct FMeshParticleInstanceVertex
{
	FLinearColor Color;           // 파티클 색상
	FVector4 Transform[3];        // 3x4 변환 행렬 (전치됨, 위치는 W에 저장)
	float RelativeTime;           // 파티클 상대 시간 (0-1)
	float Padding[3];             // 16바이트 정렬
};

// 파티클 데이터 컨테이너 (언리얼 엔진 호환)
// 메모리 레이아웃: [ParticleData 영역][ParticleIndices 영역]
// 하나의 메모리 블록에 파티클 데이터와 인덱스 배열을 함께 저장
struct FParticleDataContainer
{
	int32 MemBlockSize;            // 전체 메모리 블록 크기 (바이트) = ParticleDataNumBytes + (ParticleIndicesNumShorts * 2)
	int32 ParticleDataNumBytes;    // 파티클 데이터 영역 크기 (바이트) = MaxParticles * ParticleStride
	int32 ParticleIndicesNumShorts; // 인덱스 배열 개수 (uint16 개수) = MaxParticles
	uint8* ParticleData;           // 할당된 메모리 블록의 시작 포인터 (16바이트 정렬)
	uint16* ParticleIndices;       // 인덱스 배열 포인터 = ParticleData + ParticleDataNumBytes (별도 할당 안함)

	FParticleDataContainer()
		: MemBlockSize(0)
		, ParticleDataNumBytes(0)
		, ParticleIndicesNumShorts(0)
		, ParticleData(nullptr)
		, ParticleIndices(nullptr)
	{
	}

	~FParticleDataContainer()
	{
		Free();
	}

	// 메모리 할당 (언리얼 엔진 호환)
	// InParticleDataNumBytes: 파티클 데이터 영역 크기 (바이트) = MaxParticles * ParticleStride
	// InParticleIndicesNumShorts: 인덱스 배열 개수 (uint16 개수) = MaxParticles
	// 반환값: 할당 성공 시 true, 실패 시 false
	bool Alloc(int32 InParticleDataNumBytes, int32 InParticleIndicesNumShorts);

	// 메모리 해제 (언리얼 엔진 호환)
	void Free();
};

// 동적 이미터 리플레이 데이터 베이스 (렌더 스레드용)
struct FDynamicEmitterReplayDataBase
{
	/** 이미터의 타입 */
	EDynamicEmitterType eEmitterType;
	/** 이 이미터에서 현재 활성화된 파티클들의 숫자*/
	int32 ActiveParticleCount;
	int32 ParticleStride;
	FParticleDataContainer DataContainer;
	FVector Scale;
	int32 SortMode;

	FDynamicEmitterReplayDataBase()
		: eEmitterType(EDynamicEmitterType::Unknown)
		, ActiveParticleCount(0)
		, ParticleStride(0)
		, Scale(FVector(1.0f, 1.0f, 1.0f))
		, SortMode(0)
	{
	}

	virtual ~FDynamicEmitterReplayDataBase() = default;
};

// 스프라이트 이미터 리플레이 데이터
struct FDynamicSpriteEmitterReplayDataBase : public FDynamicEmitterReplayDataBase
{
	UMaterialInterface* MaterialInterface;
	TUniquePtr<FParticleRequiredModule> RequiredModule;

	FDynamicSpriteEmitterReplayDataBase()
		: MaterialInterface(nullptr)
		, RequiredModule(nullptr)
	{
		eEmitterType = EDynamicEmitterType::Sprite;
	}

	virtual ~FDynamicSpriteEmitterReplayDataBase() = default;
};

// 메시 이미터 리플레이 데이터
struct FDynamicMeshEmitterReplayDataBase : public FDynamicEmitterReplayDataBase
{
	UMaterialInterface* MaterialInterface;
	class UStaticMesh* MeshData;

	// 메시 회전 페이로드 오프셋 (FBaseParticle 이후의 오프셋)
	// -1이면 MeshRotation 모듈이 없음
	int32 MeshRotationPayloadOffset;

	FDynamicMeshEmitterReplayDataBase()
		: MaterialInterface(nullptr)
		, MeshData(nullptr)
		, MeshRotationPayloadOffset(-1)
	{
		eEmitterType = EDynamicEmitterType::Mesh;
	}

	virtual ~FDynamicMeshEmitterReplayDataBase() = default;
};

// 동적 이미터 데이터 베이스 (렌더링용)
struct FDynamicEmitterDataBase
{
	int32 EmitterIndex;

	FDynamicEmitterDataBase()
		: EmitterIndex(0)
	{
	}

	virtual ~FDynamicEmitterDataBase() = default;

	virtual const FDynamicEmitterReplayDataBase& GetSource() const = 0;
};

// 스프라이트 이미터 데이터 베이스
struct FDynamicSpriteEmitterDataBase : public FDynamicEmitterDataBase
{
	virtual ~FDynamicSpriteEmitterDataBase() = default;

	// 언리얼 엔진 호환: 파티클 정렬 (투명 렌더링을 위해 필수)
	// SortMode: 0 = 정렬 없음, 1 = Age (오래된 것부터), 2 = Distance (먼 것부터)
	virtual void SortSpriteParticles(int32 SortMode, const FVector& ViewOrigin)
	{
		const FDynamicEmitterReplayDataBase& SourceData = GetSource();

		if (SortMode == 0 || SourceData.ActiveParticleCount <= 1)
		{
			return;  // 정렬 불필요
		}

		uint16* Indices = SourceData.DataContainer.ParticleIndices;
		const uint8* ParticleData = SourceData.DataContainer.ParticleData;
		const int32 ParticleStride = SourceData.ParticleStride;

		if (!Indices || !ParticleData)
		{
			return;
		}

		// std::sort 사용 (O(N log N) - 버블 정렬보다 훨씬 빠름)
		std::sort(Indices, Indices + SourceData.ActiveParticleCount,
			[&](uint16 IndexA, uint16 IndexB) -> bool
			{
				const FBaseParticle* PA = (const FBaseParticle*)(ParticleData + IndexA * ParticleStride);
				const FBaseParticle* PB = (const FBaseParticle*)(ParticleData + IndexB * ParticleStride);

				if (SortMode == 1)  // Age 정렬 (오래된 것부터)
				{
					return PA->RelativeTime > PB->RelativeTime;
				}
				else if (SortMode == 2)  // Distance 정렬 (먼 것부터 - 투명도 렌더링)
				{
					float DistA = (PA->Location - ViewOrigin).SizeSquared();
					float DistB = (PB->Location - ViewOrigin).SizeSquared();
					return DistA > DistB;  // 먼 것을 먼저 렌더링
				}

				return false;
			});
	}

	virtual int32 GetDynamicVertexStride() const = 0;
};

// 스프라이트 이미터 데이터 구현
struct FDynamicSpriteEmitterData : public FDynamicSpriteEmitterDataBase
{
	FDynamicSpriteEmitterReplayDataBase Source;

	virtual ~FDynamicSpriteEmitterData() = default;

	virtual const FDynamicEmitterReplayDataBase& GetSource() const override
	{
		return Source;
	}

	virtual int32 GetDynamicVertexStride() const override
	{
		return sizeof(FParticleSpriteVertex);
	}
};

// 메시 이미터 데이터 구현
struct FDynamicMeshEmitterData : public FDynamicSpriteEmitterData
{
	FDynamicMeshEmitterReplayDataBase MeshSource;

	virtual ~FDynamicMeshEmitterData() = default;

	virtual const FDynamicEmitterReplayDataBase& GetSource() const override
	{
		return MeshSource;
	}

	virtual int32 GetDynamicVertexStride() const override
	{
		// 메시 복제 방식: FVertexDynamic 사용 (StaticMesh와 동일한 버텍스 포맷)
		// 추후 인스턴싱 전환 시: sizeof(FMeshParticleInstanceVertex) 사용
		return sizeof(FVertexDynamic);
	}
};
