#pragma once

#include "StaticMeshActor.h"
#include "AFireBallActor.generated.h"

class UPointLightComponent; 
class URotatingMovementComponent;

class AFireBallActor : public AStaticMeshActor
{
public:

	GENERATED_REFLECTION_BODY()

	AFireBallActor();

protected:
	~AFireBallActor();

protected:
	UPointLightComponent* PointLightComponent;  
	URotatingMovementComponent* RotatingComponent;
};