#pragma once

#include "ParticleModule.h"
#include "UParticleModuleRequired.generated.h"

UCLASS(DisplayName="필수 모듈", Description="이미터에 필수적인 설정을 포함하는 모듈입니다")
class UParticleModuleRequired : public UParticleModule
{
public:
	GENERATED_REFLECTION_BODY()

public:
	UPROPERTY(EditAnywhere, Category="Required")
	FString EmitterName = "Emitter";

	UPROPERTY(EditAnywhere, Category="Required")
	class UMaterialInterface* Material = nullptr;

	// 언리얼 엔진 호환: 렌더링 정렬 방식
	UPROPERTY(EditAnywhere, Category="Required")
	int32 ScreenAlignment = 0;  // 0: Billboard, 1: Velocity aligned, 2: Axis aligned 등

	UPROPERTY(EditAnywhere, Category="Required")
	bool bOrientZAxisTowardCamera = false;  // Z축을 카메라로 향하게 할지

	// 언리얼 엔진 호환: 파티클 정렬 모드
	// 0: 정렬 없음, 1: Age (오래된 것부터), 2: Distance (먼 것부터 - 투명도용)
	UPROPERTY(EditAnywhere, Category="Required")
	int32 SortMode = 0;

	// 언리얼 엔진 호환: 이미터 원점 (파티클 스폰 위치 오프셋)
	UPROPERTY(EditAnywhere, Category="Emitter")
	FVector EmitterOrigin = FVector(0.0f, 0.0f, 0.0f);

	// 언리얼 엔진 호환: 이미터 회전 (Euler angles in degrees: X=Roll, Y=Pitch, Z=Yaw)
	// 파티클 속도 방향을 회전시킴
	UPROPERTY(EditAnywhere, Category="Emitter")
	FVector EmitterRotation = FVector(0.0f, 0.0f, 0.0f);

	// 언리얼 엔진 호환: 이미터 지속 시간 (초 단위, 0 = 무한)
	// 이 시간 동안만 파티클을 생성함
	UPROPERTY(EditAnywhere, Category="Duration")
	float EmitterDuration = 0.0f;

	// 언리얼 엔진 호환: 이미터 지속 시간 최소값 (랜덤 범위용)
	// EmitterDurationLow > 0이면 [EmitterDurationLow, EmitterDuration] 범위에서 랜덤 선택
	UPROPERTY(EditAnywhere, Category="Duration")
	float EmitterDurationLow = 0.0f;

	// 언리얼 엔진 호환: 이미터 시작 딜레이 (초 단위)
	// 이 시간 동안 파티클 생성을 지연시킴
	UPROPERTY(EditAnywhere, Category="Delay")
	float EmitterDelay = 0.0f;

	// 언리얼 엔진 호환: 이미터 시작 딜레이 최소값 (랜덤 범위용)
	// EmitterDelayLow > 0이면 [EmitterDelayLow, EmitterDelay] 범위에서 랜덤 선택
	UPROPERTY(EditAnywhere, Category="Delay")
	float EmitterDelayLow = 0.0f;

	// 언리얼 엔진 호환: 이미터 반복 횟수 (0 = 무한 반복)
	UPROPERTY(EditAnywhere, Category="Duration")
	int32 EmitterLoops = 0;

	// 언리얼 엔진 호환: 첫 루프에만 딜레이 적용
	// true면 EmitterDelay는 첫 루프에만 적용되고 이후 루프는 즉시 시작
	UPROPERTY(EditAnywhere, Category="Delay")
	bool bDelayFirstLoopOnly = false;

	// ────────────────────────────────────────────
	// Sub-UV (스프라이트 시트 애니메이션)
	// ────────────────────────────────────────────

	// 스프라이트 시트 가로 분할 수 (열 개수)
	UPROPERTY(EditAnywhere, Category="SubUV")
	int32 SubImages_Horizontal = 1;

	// 스프라이트 시트 세로 분할 수 (행 개수)
	UPROPERTY(EditAnywhere, Category="SubUV")
	int32 SubImages_Vertical = 1;

	// 실제 사용할 프레임 수 (0 = SubImages_Horizontal * SubImages_Vertical 전체 사용)
	// 스프라이트 시트가 완전히 채워지지 않은 경우 사용 (예: 4x4=16칸이지만 14프레임만 있는 경우)
	UPROPERTY(EditAnywhere, Category="SubUV")
	int32 SubUV_MaxElements = 0;

	UParticleModuleRequired() = default;
	virtual ~UParticleModuleRequired();

	// 로드 시 생성한 Material 소유 여부 (true면 소멸자에서 삭제)
	bool bOwnsMaterial = false;

	// LOD 복제 시 Material도 복제 (bOwnsMaterial=true인 경우)
	virtual void DuplicateSubObjects() override;

	// 언리얼 엔진 호환: 렌더 스레드용 데이터로 변환
	FParticleRequiredModule ToRenderThreadData() const;

	virtual void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;

	// Required는 두 번째로 표시 (우선순위 1)
	virtual int32 GetDisplayPriority() const override { return 1; }
};
