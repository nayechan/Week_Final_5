#pragma once

#include "Actor.h"
#include "Enums.h"
#include "AVehicleActor.generated.h"

class UVehicleComponent;
class UStaticMeshComponent;

UCLASS(DisplayName = "자동차", Description = "자동차 액터입니다")
class AVehicleActor : public AActor
{
public:
	GENERATED_REFLECTION_BODY()

		AVehicleActor();

	void Tick(float DeltaTime) override;
protected:
	~AVehicleActor() override;

public:
	FAABB GetBounds() const override;
	UVehicleComponent* GetVehicleComponent() const { return VehicleComponent; }
	void SetVehicleComponent(UVehicleComponent* InVehicleComponent);
	void BeginPlay() override;

	// 복제/직렬화
	void DuplicateSubObjects() override;
	void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;

	void UpdateWheelsTransform(int32 WheelIndex, FVector Translation, FQuat Rotation);
	void UpdateDriftSmoke(float Value);

protected:
	UVehicleComponent* VehicleComponent;
	UStaticMeshComponent* VehicleBodyComponent;
	TArray<UStaticMeshComponent*> VehicleWheelComponents;
	UParticleSystemComponent* SmokeParticleComponent;
};

