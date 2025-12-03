#pragma once
#include "PrimitiveComponent.h"
#include "VehicleData.h"
#include "UVehicleComponent.generated.h"

namespace snippetvehicle
{
	class VehicleSceneQueryData;
}
namespace physx
{
	class PxVehicleDrivableSurfaceToTireFrictionPairs;
}

UCLASS(DisplayName = "자동차 물리 컴포넌트", Description = "자동차 물리 처리 컴포넌트입니다")
class UVehicleComponent : public UPrimitiveComponent
{
public:
	GENERATED_REFLECTION_BODY()

	UVehicleComponent();

	virtual ~UVehicleComponent();
	void EndPlay() override;
	void TickComponent(float DeltaTime) override;
	void DuplicateSubObjects() override;
	void PostPhysicsTick(float DeltaTime) override;
	void OnTransformUpdated() override;
	void SyncByPhysics(const FTransform& NewTransform) override;

	bool CanSimulatingPhysics() const  override { return true; }

	void Simulate(float DeltaTime);

	// 차량 데이터 준비
	UPROPERTY(EditAnywhere, Category = "Vehicle")
	FVehicleData VehicleData;

	// Getter for Physics Scene
	physx::PxVehicleDrive4W* GetPhysXVehicle() const { return PhysXVehicle; }
	snippetvehicle::VehicleSceneQueryData* GetVehicleQueryData() const { return VehicleQueryData; }
	PxBatchQuery* GetBatchQuery() const { return BatchQuery; }
	PxVehicleDrivableSurfaceToTireFrictionPairs* GetFrictionPairs() const { return FrictionPairs; }
	void UpdateDriftEffects();

protected:
	void CreatePhysicsState() override;

private:
	PxVehicleDrive4W* CreateVehicle4W(FVehicleData VehicleData, physx::PxPhysics* Physics, PxCooking* cooking, const FTransform& WorldTransform);

	// PhysX 차량 인스턴스
	physx::PxVehicleDrive4W* PhysXVehicle = nullptr;
	UPhysicalMaterial* ChassisMaterial = nullptr;
	UPhysicalMaterial* WheelMaterial = nullptr;

	// 휠 접지 검사를 위한 PhysX 씬 쿼리 데이터 (PxVehicleSceneQueryData)
	// 각 바퀴가 지면에 레이캐스트를 발사하여 얻은 결과를 저장하는 컨테이너입니다.
	// 바퀴의 서스펜션 길이를 계산하고, 지면의 노멀, 재질 정보 등을 얻는 데 사용됩니다.
	snippetvehicle::VehicleSceneQueryData* VehicleQueryData = nullptr;

	// PhysX 배치 쿼리 (PxBatchQuery)
	// 여러 레이캐스트 쿼리를 한 번에 물리 씬에 전송하여 효율적으로 처리하기 위한 객체입니다.
	// 매 프레임 모든 바퀴에 대해 개별적으로 레이캐스트를 날리는 대신, 한 번의 호출로 모든 바퀴의 쿼리를 처리할 수 있습니다.
	PxBatchQuery* BatchQuery = nullptr;

	// 타이어 마찰력 데이터 (PxVehicleDrivableSurfaceToTireFrictionPairs)
	// 타이어 타입과 접지 가능한 표면(drivable surface) 타입 간의 마찰 계수를 정의합니다.
	// 차량이 특정 재질의 지면(예: 아스팔트, 흙, 얼음) 위를 주행할 때 발생하는 마찰력을 시뮬레이션하는 데 필수적입니다.
	// 이를 통해 차량의 가속, 제동, 선회 시 미끄러짐 정도가 결정됩니다.
	PxVehicleDrivableSurfaceToTireFrictionPairs* FrictionPairs = nullptr;

	PxWheelQueryResult WheelQueryResults[PX_MAX_NB_WHEELS];
};