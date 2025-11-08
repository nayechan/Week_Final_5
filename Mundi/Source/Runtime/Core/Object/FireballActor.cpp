#include "pch.h"
#include "FireballActor.h"
#include "PointLightComponent.h"
#include "StaticMeshComponent.h"
#include "Material.h"
#include "ResourceManager.h"1
#include "Texture.h"
#include "RotatingMovementComponent.h"
// IMPLEMENT_CLASS is now auto-generated in .generated.cpp
//BEGIN_PROPERTIES(AFireBallActor)
//	MARK_AS_SPAWNABLE("파이어 볼", "파이어 볼을 렌더링하는 액터입니다.")
//END_PROPERTIES()

AFireBallActor::AFireBallActor()
{
	ObjectName = "Fire Ball Actor";
	PointLightComponent = CreateDefaultSubobject<UPointLightComponent>("PointLightComponent");
	PointLightComponent->SetupAttachment(RootComponent);
	 
	StaticMeshComponent->SetStaticMesh(GDataDir  + "/Model/Sphere8.obj");
	StaticMeshComponent->SetMaterialByName(0, "Shaders/Materials/Fireball.hlsl");

	RotatingComponent = CreateDefaultSubobject<URotatingMovementComponent>("RotatingComponent");
	RotatingComponent->SetRotationRate(FVector(0, 0, 400.0f));

	UTexture* Texture = UResourceManager::GetInstance().Load<UTexture>(GDataDir + "/Textures/FireballNoise.jpg");
	StaticMeshComponent->SetMaterialTextureByUser(0, EMaterialTextureSlot::Diffuse, Texture);
}

AFireBallActor::~AFireBallActor()
{

}
