#include "pch.h"
#include "FakeSpotLightActor.h"
#include "PerspectiveDecalComponent.h"
#include "BillboardComponent.h"

IMPLEMENT_CLASS(AFakeSpotLightActor)

BEGIN_PROPERTIES(AFakeSpotLightActor)
	MARK_AS_SPAWNABLE("가짜 스포트 라이트", "데칼을 사용한 가짜 스포트 라이트 액터입니다.")
END_PROPERTIES()

AFakeSpotLightActor::AFakeSpotLightActor()
{
	Name = "Fake Spot Light Actor";
	BillboardComponent = CreateDefaultSubobject<UBillboardComponent>("BillboardComponent");
	DecalComponent = CreateDefaultSubobject<UPerspectiveDecalComponent>("DecalComponent");

	BillboardComponent->SetTextureName("Editor/SpotLight_64x.png");
	DecalComponent->SetRelativeScale((FVector(10, 5, 5)));
	DecalComponent->SetRelativeLocation((FVector(0, 0, -5)));
	DecalComponent->SetRelativeRotation(FQuat::MakeFromEulerZYX(FVector(0, 90, 0)));
	DecalComponent->SetDecalTexture("Data/Textures/GreenLight.png");
	DecalComponent->SetFovY(60);

	BillboardComponent->SetupAttachment(RootComponent);
	DecalComponent->SetupAttachment(RootComponent);
}

AFakeSpotLightActor::~AFakeSpotLightActor()
{
}

void AFakeSpotLightActor::DuplicateSubObjects()
{
	Super_t::DuplicateSubObjects();

	for (UActorComponent* Component : OwnedComponents)
	{
		if (UPerspectiveDecalComponent* Decal = Cast<UPerspectiveDecalComponent>(Component))
		{
			DecalComponent = Decal;
			break;
		}
		else if (UBillboardComponent* Billboard = Cast<UBillboardComponent>(Component))
		{
			BillboardComponent = Billboard;
			break;
		}
	}
}

void AFakeSpotLightActor::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	Super::Serialize(bInIsLoading, InOutHandle);

	if (bInIsLoading)
	{
		UPerspectiveDecalComponent* DecalComponentPre = nullptr;
		UBillboardComponent* BillboardCompPre = nullptr;

		for (auto& Component : SceneComponents)
		{
			if (UPerspectiveDecalComponent* PerpectiveDecalComp = Cast<UPerspectiveDecalComponent>(Component))
			{
				DecalComponentPre = DecalComponent;
				DecalComponent->DetachFromParent();
				DecalComponent = PerpectiveDecalComp;
			}
			else if (UBillboardComponent* BillboardCompTemp = Cast<UBillboardComponent>(Component))
			{
				BillboardCompPre = BillboardComponent;
				BillboardComponent->DetachFromParent();
				BillboardComponent = BillboardCompTemp;
			}
		}

		if (DecalComponentPre)
		{
			DecalComponentPre->GetOwner()->RemoveOwnedComponent(DecalComponentPre);

		}
		if (BillboardCompPre)
		{
			BillboardCompPre->GetOwner()->RemoveOwnedComponent(BillboardCompPre);
		}
	}
}
