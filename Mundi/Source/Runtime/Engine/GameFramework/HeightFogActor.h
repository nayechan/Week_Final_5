#pragma once

#include "Info.h"
#include "AHeightFogActor.generated.h"

class AHeightFogActor : public AInfo
{
public:

	GENERATED_REFLECTION_BODY()

	AHeightFogActor();

	void DuplicateSubObjects() override;
};