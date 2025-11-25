#include "pch.h"
#include "ParticleModuleTypeDataRibbon.h"

void UParticleModuleTypeDataRibbon::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
    UParticleModuleTypeDataBase::Serialize(bInIsLoading, InOutHandle);
    // UPROPERTY 자동 직렬화
}
