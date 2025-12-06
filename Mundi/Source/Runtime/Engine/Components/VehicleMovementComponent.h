#pragma once
#include "MovementComponent.h"
#include "Vehicle/VehicleWheel.h"
#include "VehicleDefinitions.h"
#include "UVehicleMovementComponent.generated.h"

namespace physx
{
    class PxVehicleDrive4W;
    class PxVehicleWheelsSimData;
    class PxVehicleDriveSimData4W;
    class PxVehicleDrive4WRawInputData;
    class PxBatchQuery;
    class PxVehicleDrivableSurfaceToTireFrictionPairs;
}

struct FVehicleWheelSetup
{
    /** 차체 중심 (Center of Mass)으로부터의 상대적 위치 */
    FVector BoneOffset;

    /** 시각적 메시를 위한 추가 오프셋 */
    FVector VisualOffset;

    FVehicleWheelSetup()
        : BoneOffset(FVector::Zero())
        , VisualOffset(FVector::Zero())
    {}
};

UCLASS()
class UVehicleMovementComponent : public UMovementComponent
{
    GENERATED_REFLECTION_BODY()

public:
    UVehicleMovementComponent();
    virtual ~UVehicleMovementComponent() override;

    // ====================================================================
    // 언리얼 엔진 생명주기 (Lifecycle)
    // ====================================================================
    virtual void InitializeComponent() override;
    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaSeconds) override;
    virtual void OnRegister(UWorld* InWorld) override;
    virtual void OnUnregister() override;

    // ====================================================================
    // 차량 설정 데이터 (에디터 노출)
    // ====================================================================
    FVehicleEngineData EngineSetup;
    FVehicleTransmissionData TransmissionSetup;

    /** 4개의 휠 설정 (순서: 0:FL, 1:FR, 2:RL, 3:RR) */
    UPROPERTY()
    TArray<UVehicleWheel*> VehicleWheels;

    TArray<FVehicleWheelSetup> WheelSetups;

    // ====================================================================
    // 입력 인터페이스 (PlayerController에서 호출)
    // ====================================================================
    void SetThrottleInput(float Throttle); // 0.0 ~ 1.0 (가속)
    void SetSteeringInput(float Steering); // -1.0 ~ 1.0 (좌우)
    void SetBrakeInput(float Brake);       // 0.0 ~ 1.0 (제동)
    void SetHandbrakeInput(bool bIsHandbrake);

    /** 현재 전진 속도 (m/s) */
    float GetForwardSpeed() const;

    /** 현재 엔진 RPM */
    float GetEngineRPM() const;

    /** 현재 기어 (-1: 후진, 0: 중립, 1~: 전진) */
    int32 GetCurrentGear() const;

    /** 부스터 힘 적용 (전진 방향으로) */
    void ApplyBoostForce(float BoostStrength);

    /** 기어를 전진(Drive/Auto) 모드로 변경 */
    void SetGearToDrive();

    /** 기어를 후진(Reverse) 모드로 변경 */
    void SetGearToReverse();

    // ====================================================================
    // 유틸리티 및 디버깅
    // ====================================================================
    FTransform GetWheelTransform(int32 WheelIndex) const;

private:
    // ====================================================================
    // 내부 구현 함수 (Setup & Helper)
    // ====================================================================
    
    /** 차량 물리 인스턴스 생성 및 초기화 (가장 중요한 함수) */
    void SetupVehicle();

    /** 차량 물리 인스턴스 해제 */
    void ReleaseVehicle();

    /** 바퀴의 물리적 형태(Shape) 생성 및 필터링 설정 */
    void SetupWheelShape(physx::PxRigidDynamic* RigidActor);

    /** 휠 시뮬레이션 데이터(Suspension, Tire, Mass) 설정 */
    void SetupWheelSimulationData(physx::PxRigidDynamic* RigidActor);

    /** PhysX Vehicle 인스턴스 생성 및 초기화 */
    void SetupDriveSimulationData(physx::PxRigidDynamic* RigidActor);

    /** 유효한 PhysX 다이나믹 액터를 반환 */
    PxRigidDynamic* GetValidDynamicActor() const;

    /** 서스펜션 레이캐스트를 위한 배치 쿼리 시스템 초기화 */
    void SetupBatchQuery();

    /** 배치 쿼리 및 관련 메모리 안전 해제 */
    void ReleaseBatchQuery();
    
    /** */
    void SetupFrictionPairs();

    /** */
    void ReleaseFrictionPairs();

    /** 매 프레임 서스펜션 레이캐스트 수행 */
    void PerformSuspensionRaycasts(float DeltaTime);

    /** 차량이 공중에 떠 있는지 확인 (스티어링 보정용) */
    bool IsVehicleInAir() const;

    /** Raw 입력을 스무딩하여 PhysX Drive에 적용 */
    void ProcessVehicleInput(float DeltaTime);

    /** 물리 시뮬레이션 (레이캐스트 및 차량 업데이트) */
    void UpdateVehicleSimulation(float DeltaTime);

    // ====================================================================
    // PhysX 멤버 변수 (Sim Data)
    // ====================================================================
    
    /** PhysX 차량 드라이브 인스턴스 (핵심 객체) */
    physx::PxVehicleDrive4W* PVehicleDrive;

    /** 휠 시뮬레이션 데이터 (질량, 서스펜션, 타이어 등) */
    physx::PxVehicleWheelsSimData* PWheelsSimData;

    /** 입력 데이터 버퍼 (키보드 -> PhysX 변환용) */
    physx::PxVehicleDrive4WRawInputData* PInputData;

    /** 타이어 마찰력 데이터 쌍 */
    physx::PxVehicleDrivableSurfaceToTireFrictionPairs* FrictionPairs;

    // ====================================================================
    // Raycast Batch Query (메모리 관리 주의)
    // ====================================================================
    
    /** 서스펜션 레이캐스트 쿼리 객체 */
    physx::PxBatchQuery* SuspensionBatchQuery;

    /** [결과 버퍼] 레이캐스트 결과가 저장되는 배열 */
    physx::PxRaycastQueryResult* BatchQueryResults;

    /** [더미 버퍼] "TouchBuffer is NULL" 에러 방지용 (사용은 안함) */
    physx::PxRaycastHit* BatchQueryTouchBuffer;

    // ====================================================================
    // Deferred Filter Data (PVD 버그 회피용)
    // ====================================================================

    /** 휠 Shape들 (FilterData 지연 설정용) */
    TArray<physx::PxShape*> PendingWheelShapes;

    /** 차체 Shape들 (FilterData 지연 설정용) */
    TArray<physx::PxShape*> PendingChassisShapes;

    /** 다음 프레임에 FilterData 설정이 필요한지 여부 (PVD 버그 회피) */
    bool bPendingFilterDataSetup = false;

    /** Scene 등록 후 FilterData 설정 */
    void FinalizeShapeFilterData();

    // ====================================================================
    // 수동 자동변속 로직 (PhysX AutoBox 대체)
    // ====================================================================

    /** 수동 자동변속 업데이트 */
    void UpdateManualAutoGearBox(float DeltaTime);

    /** 마지막 변속 후 경과 시간 */
    float GearSwitchTimer = 0.0f;

    /** 변속 쿨다운 시간 */
    float GearSwitchCooldown = 0.3f;

    /** 상향 변속 RPM 비율 (MaxRPM 대비) */
    float UpShiftRatio = 0.85f;

    /** 하향 변속 RPM 비율 (MaxRPM 대비) */
    float DownShiftRatio = 0.35f;
};