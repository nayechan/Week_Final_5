#include "pch.h"
#include "VehicleActor.h"
#include "VehicleComponent.h"
#include "StaticMeshComponent.h"

static FName VehicleBodyKey = "VehicleBody";	// 바꾸면 안됨 하드코딩됨

AVehicleActor::AVehicleActor()
{
	ObjectName = "자동차 액터";
	VehicleComponent = CreateDefaultSubobject<UVehicleComponent>("VehicleComponent");

	// 루트 교체
	RootComponent = VehicleComponent;

	VehicleBodyComponent = CreateDefaultSubobject<UStaticMeshComponent>("VehicleBodyComponent");
	VehicleBodyComponent->ObjectName = VehicleBodyKey;
	VehicleBodyComponent->SetStaticMesh(GDataDir + "/Model/Vehicles/SportsCar_Body.fbx");
	VehicleBodyComponent->SetupAttachment(RootComponent);
	VehicleBodyComponent->bIsStatic = true;
	VehicleBodyComponent->SetRelativeLocation(FVector(2.048f, 0, -10.0f));

	VehicleWheelComponents.Empty();
	for (int WheelIndex = 0; WheelIndex < VehicleComponent->VehicleData.NumWheels; WheelIndex++)
	{
		UStaticMeshComponent* VehicleWheelComponent = CreateDefaultSubobject<UStaticMeshComponent>("VehicleWheelComponent");
		VehicleWheelComponent->SetStaticMesh(GDataDir + "/Model/Vehicles/SportsCar_Tire.fbx");
		VehicleWheelComponent->SetupAttachment(RootComponent);
		VehicleWheelComponent->bIsStatic = true;
		VehicleWheelComponents.Add(VehicleWheelComponent);
	}
}

void AVehicleActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

AVehicleActor::~AVehicleActor()
{

}

FAABB AVehicleActor::GetBounds() const
{
	// 멤버 변수를 직접 사용하지 않고, 현재의 RootComponent를 확인하도록 수정
	// 기본 컴포넌트가 제거되는 도중에 어떤 로직에 의해 GetBounds() 함수가 호출
	// 이 시점에 AVehicleActor의 멤버변수인 UVehicleComponent는 아직 새로운 컴포넌트
	// ID 751로 업데이트되기 전. 제거된 기본 컴포넌트를 여전히 가리키고 있음. 
	// 유효하지 않은 staticmeshcomponent 포인터의 getworldaabb 함수를 호출 시도.

	UVehicleComponent* CurrentSMC = Cast<UVehicleComponent>(RootComponent);
	if (CurrentSMC)
	{
		return CurrentSMC->GetWorldAABB();
	}

	return FAABB();
}

void AVehicleActor::SetVehicleComponent(UVehicleComponent* InVehicleComponent)
{
	VehicleComponent = InVehicleComponent;
}

void AVehicleActor::BeginPlay()
{
	Super::BeginPlay();

	//if (VehicleBodyComponent)
	//{
	//	//VehicleBodyComponent->BodyInstance.SetCollisionEnabled(false);
	//	VehicleBodyComponent->BodyInstance.TermBody();
	//}

	//for (int32 WheelIndex = 0; WheelIndex < VehicleWheelComponents.Num(); WheelIndex++)
	//{
	//	if (VehicleWheelComponents[WheelIndex])
	//	{
	//		//VehicleWheelComponents[WheelIndex]->BodyInstance.SetCollisionEnabled(false);
	//		VehicleWheelComponents[WheelIndex]->BodyInstance.TermBody();
	//	}
	//}
}

void AVehicleActor::DuplicateSubObjects()
{
	Super::DuplicateSubObjects();

	VehicleComponent = Cast<UVehicleComponent>(RootComponent);

	VehicleWheelComponents.Empty();
	for (UActorComponent* Component : RootComponent->GetAttachChildren())
	{
		if (Component->ObjectName == VehicleBodyKey)
		{
			continue;
		}

		if (UStaticMeshComponent* VehicleComp = Cast<UStaticMeshComponent>(Component))
		{
			VehicleWheelComponents.Add(VehicleComp);
		}
	}
}

void AVehicleActor::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	Super::Serialize(bInIsLoading, InOutHandle);

	if (bInIsLoading)
	{
		VehicleComponent = Cast<UVehicleComponent>(RootComponent);

		VehicleWheelComponents.Empty();
		for (UActorComponent* Component : RootComponent->GetAttachChildren())
		{
			if (Component->ObjectName == VehicleBodyKey)
			{
				continue;
			}

			if (UStaticMeshComponent* VehicleComp = Cast<UStaticMeshComponent>(Component))
			{
				VehicleWheelComponents.Add(VehicleComp);
			}
		}
	}
}

void AVehicleActor::UpdateWheelsTransform(int32 WheelIndex, FVector Translation, FQuat Rotation)
{
	if (WheelIndex < VehicleWheelComponents.Num())
	{
		if (VehicleWheelComponents[WheelIndex])
		{
			VehicleWheelComponents[WheelIndex]->SetLocalLocationAndRotation(Translation, Rotation);
			if (WheelIndex % 2 == 0)
			{
				VehicleWheelComponents[WheelIndex]->AddLocalRotation(FQuat::MakeFromEulerZYX(FVector(0, 0, 90)));
			}
			else
			{
				VehicleWheelComponents[WheelIndex]->AddLocalRotation(FQuat::MakeFromEulerZYX(FVector(0, 0, -90)));
			}
		}
	}
}
