#pragma once

#include "Info.h"
#include "ADepthOfFieldActor.generated.h"

UCLASS(DisplayName="피사계 심도", Description="피사계 심도 효과를 생성하는 액터입니다")
class ADepthOfFieldActor : public AInfo
{
public:
    GENERATED_REFLECTION_BODY()

    ADepthOfFieldActor();

    void DuplicateSubObjects() override;
};

