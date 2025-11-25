#pragma once

#include "ParticleModuleTypeDataBase.h"
#include "UParticleModuleTypeDataRibbon.generated.h"

UCLASS(DisplayName = "리본 타입 데이터", Description = "Ribbon(트레일) 파티클을 위한 타입 데이터입니다")
class UParticleModuleTypeDataRibbon : public UParticleModuleTypeDataBase
{
public:
    GENERATED_REFLECTION_BODY()

public:
    // 리본 폭
    UPROPERTY(EditAnywhere, Category = "Ribbon")
    float RibbonWidth = 8.0f;

    // 리본 세그먼트 분할 (높을수록 곡률이 부드러움)
    UPROPERTY(EditAnywhere, Category = "Ribbon")
    int32 Tessellation = 6;

    // UV 타일링
    UPROPERTY(EditAnywhere, Category = "Ribbon")
    float TileU = 1.0f;

    // Ribbon을 Simulation Space(Local/World)에 따라 따라가게 할지 여부
    UPROPERTY(EditAnywhere, Category = "Ribbon")
    bool bScreenSpace = false;

public:
    UParticleModuleTypeDataRibbon() = default;
    virtual ~UParticleModuleTypeDataRibbon() = default;

    virtual void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;
};
