#pragma once

#include "ParticleModuleTypeDataBase.h"
#include "UParticleModuleTypeDataBeam.generated.h"

UCLASS(DisplayName = "빔 타입 데이터", Description = "Beam 파티클을 위한 타입 데이터입니다")
class UParticleModuleTypeDataBeam : public UParticleModuleTypeDataBase
{
public:
    GENERATED_REFLECTION_BODY()

public:
    // 빔 하나가 가질 수 있는 세그먼트 수
    UPROPERTY(EditAnywhere, Category = "Beam")
    int32 SegmentCount = 8;

    // UV 타일링 (전체 길이 기반)
    UPROPERTY(EditAnywhere, Category = "Beam")
    float TileU = 1.0f;

    // 빔 너비
    UPROPERTY(EditAnywhere, Category = "Beam")
    float BeamWidth = 10.0f;

    // 목표 위치가 고정인지, 타겟 파티클 또는 Actor가 있는지 여부
    UPROPERTY(EditAnywhere, Category = "Beam")
    bool bUseTarget = false;

    // 타겟 월드 위치 (bUseTarget=false 시 사용)
    UPROPERTY(EditAnywhere, Category = "Beam", meta = (EditCondition = "!bUseTarget"))
    FVector TargetPoint = FVector(0, 0, 0);

public:
    UParticleModuleTypeDataBeam() = default;
    virtual ~UParticleModuleTypeDataBeam() = default;

    virtual void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;
};
