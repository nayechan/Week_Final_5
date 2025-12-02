#pragma once
#include "MovementComponent.h"
#include "Vehicle/VehicleEngineData.h"
#include "Vehicle/VehicleWheel.h"
#include "UVehicleMovementComponent.generated.h"

class PxVehicleDrive4W;
class PxVehicleWheelsSimData;
class PxVehicleDriveSimData4W;

UCLASS()
class UVehicleMovementComponent : public UMovementComponent
{
public:
    GENERATED_REFLECTION_BODY()

    UVehicleMovementComponent();
    virtual ~UVehicleMovementComponent() override;

    // ====================================================================
    // 초기화 및 생명주기
    // ====================================================================
    virtual void InitializeComponent() override;
    virtual void TickComponent(float DeltaSeconds) override;
    virtual void OnRegister(UWorld* InWorld) override;
    virtual void OnUnregister() override;
    virtual void BeginPlay() override;

    // 차량 생성 함수 (InitializeComponent에서 호출됨)
    void SetupVehicle();

    // ====================================================================
    // 설정 (에디터 노출용)
    // ====================================================================
    UPROPERTY()
    FVehicleEngineData EngineSetup;

    /** 4개의 휠 설정 (0:FL, 1:FR, 2:RL, 3:RR 순서) */
    UPROPERTY()
    TArray<UVehicleWheel*> WheelSetups;

    // ====================================================================
    // 입력 처리 (플레이어 컨트롤러에서 호출)
    // ====================================================================
    void SetThrottleInput(float Throttle); // 0.0 ~ 1.0
    void SetSteeringInput(float Steering); // -1.0 ~ 1.0
    void SetBrakeInput(float Brake);       // 0.0 ~ 1.0
    void SetHandbrakeInput(bool bNewHandbrake);

    // ====================================================================
    // 유틸리티
    // ====================================================================
    /** 특정 바퀴의 트랜스폼 가져오기 (메쉬 업데이트용) */
    FTransform GetWheelTransform(int32 WheelIndex) const;

protected:
    /** 각 휠의 렌더링용 트랜스폼을 캐싱할 배열 */
    TArray<FTransform> WheelTransforms;
    
    /** PhysX Vehicle 인스턴스 */
    physx::PxVehicleDrive4W* PVehicleDrive;

    /** 입력 데이터 버퍼 */
    physx::PxVehicleDrive4WRawInputData* PInputData;

    /** 시뮬레이션 데이터 (휠) */
    physx::PxVehicleWheelsSimData* PWheelsSimData;

    /** 시뮬레이션 데이터 (엔진/구동계) */
    physx::PxVehicleDriveSimData4W* PDriveSimData;

    // 내부 헬퍼: PhysX 배치 쿼리용 (레이캐스트)
    void PerformSuspensionRaycasts(float DeltaTime);
};