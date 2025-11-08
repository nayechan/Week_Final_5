#include "pch.h"
#include "HeightFogActor.h"
#include "HeightFogComponent.h"
// IMPLEMENT_CLASS is now auto-generated in .generated.cpp
//BEGIN_PROPERTIES(AHeightFogActor)
//	MARK_AS_SPAWNABLE("하이트 포그 액터", "하이트 포그 액터입니다")
//END_PROPERTIES()

AHeightFogActor::AHeightFogActor()
{
	ObjectName = "Height Fog Actor";
	RootComponent = CreateDefaultSubobject<UHeightFogComponent>("HeightFogComponent");

}

void AHeightFogActor::DuplicateSubObjects()
{
	Super::DuplicateSubObjects();
}
