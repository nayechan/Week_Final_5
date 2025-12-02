#include "pch.h"
#include "DepthOfFieldActor.h"
#include "DepthOfFieldComponent.h"

ADepthOfFieldActor::ADepthOfFieldActor()
{
    ObjectName = "Depth of Field Actor";
    RootComponent = CreateDefaultSubobject<UDepthOfFieldComponent>("DepthOfFieldComponent");
}

void ADepthOfFieldActor::DuplicateSubObjects()
{
    Super::DuplicateSubObjects();
}

