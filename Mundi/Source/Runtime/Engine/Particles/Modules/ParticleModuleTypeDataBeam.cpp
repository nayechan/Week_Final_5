#include "pch.h"
#include "ParticleModuleTypeDataBeam.h"

void UParticleModuleTypeDataBeam::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
    UParticleModuleTypeDataBase::Serialize(bInIsLoading, InOutHandle);
    // UPROPERTY는 자동 직렬화
}
