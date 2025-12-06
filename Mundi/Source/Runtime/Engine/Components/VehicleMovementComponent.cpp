#include "pch.h"
#include "VehicleMovementComponent.h"
#include "SceneComponent.h"
#include "PrimitiveComponent.h"
#include "BodyInstance.h"
#include "Keyboard.h"
#include "PhysScene.h"
#include "Vehicle/VehicleWheel.h"

/** 차량 자신 (RayCast가 자기 자신을 맞추지 않기 위함) */
static const PxU32 VEHICLE_SURFACE_TYPE_CHASSIS = 0xffff;
static const float VEHICLE_MASS = 1500.0f;
static const uint32 MAX_WHEELS = 4;


/** 레이캐스트가 닿은 물체가 차량 자신이면 무시하기위한 필터 */
static PxQueryHitType::Enum WheelRaycastPreFilter(
    PxFilterData filterData0, // Ray (바퀴)
    PxFilterData filterData1, // Hit Object (지형, 차체 등)
    const void* constantBlock,
    PxU32 constantBlockSize,
    PxHitFlags& queryFlags)
{
    if (filterData1.word3 == VEHICLE_SURFACE_TYPE_CHASSIS)
    {
        return PxQueryHitType::eNONE;
    }

    return PxQueryHitType::eBLOCK; 
}

// ====================================================================
// 입력용 스무딩 데이터
// ====================================================================

static PxVehiclePadSmoothingData gPadSmoothingData =
{
    { 6.0f, 6.0f, 12.0f, 1.0f, 1.0f },
    { 10.0f, 10.0f, 12.0f, 5.0f, 5.0f }
};

static PxF32 gSteerVsForwardSpeedData[2 * 8] =
{
    0.0f, 0.75f, 5.0f, 0.75f, 30.0f, 0.125f, 120.0f, 0.1f,
    PX_MAX_F32, PX_MAX_F32, PX_MAX_F32, PX_MAX_F32,
    PX_MAX_F32, PX_MAX_F32, PX_MAX_F32, PX_MAX_F32
};

static PxFixedSizeLookupTable<8> gSteerVsForwardSpeedTable(gSteerVsForwardSpeedData, 4);

// ====================================================================
// 생성자 & 소멸자
// ====================================================================

UVehicleMovementComponent::UVehicleMovementComponent()
    : PVehicleDrive(nullptr)
    , PInputData(nullptr)
    , PWheelsSimData(nullptr)
    , FrictionPairs(nullptr)
    , SuspensionBatchQuery(nullptr)
    , BatchQueryResults(nullptr)
    , BatchQueryTouchBuffer(nullptr)
{
    VehicleWheels.SetNum(MAX_WHEELS);
    for (int32 i = 0; i < MAX_WHEELS; i++)
    {
        VehicleWheels[i] = NewObject<UVehicleWheel>();

        // @todo 현재 하드코딩된 스티어링 각도 사용
        if (i < 2) 
        {
            VehicleWheels[i]->SteerAngle = 45.0f; 
        }
        else
        {
            VehicleWheels[i]->SteerAngle = 0.0f; 
        }
    }
    
    // @todo 현재 하드코딩된 바퀴 설정 사용
    FVector COM = FVector(0, 0, 0.723);
    WheelSetups.SetNum(MAX_WHEELS);
    WheelSetups[0].BoneOffset = FVector( 1.289f, -0.939f, 0.354f) - COM;
    WheelSetups[1].BoneOffset = FVector( 1.289f,  0.939f, 0.354f) - COM;
    WheelSetups[2].BoneOffset = FVector(-1.062f, -0.954f, 0.411f) - COM;
    WheelSetups[3].BoneOffset = FVector(-1.062f,  0.954f, 0.411f) - COM;
    for (int i = 0; i < MAX_WHEELS; i++)
    {
        VehicleWheels[i]->MaxSuspensionCompression = 0.1f;
        VehicleWheels[i]->MaxSuspensionDroop = 0.1f;
    }
}

UVehicleMovementComponent::~UVehicleMovementComponent()
{
}

// ====================================================================
// 언리얼 엔진 생명주기 (Lifecycle)
// ====================================================================

void UVehicleMovementComponent::InitializeComponent()
{
    Super::InitializeComponent();
}

void UVehicleMovementComponent::BeginPlay()
{
    Super::BeginPlay();

    // SetupVehicle은 TickComponent에서 Scene이 준비된 후 호출
    // Standalone 환경에서는 BeginPlay 시점에 Scene이 아직 null일 수 있음
}

void UVehicleMovementComponent::OnRegister(UWorld* InWorld)
{
    Super::OnRegister(InWorld);
    
    if (!PInputData)
    {
        PInputData = new PxVehicleDrive4WRawInputData();
    }
}

void UVehicleMovementComponent::OnUnregister()
{
    Super::OnUnregister();

    ReleaseVehicle();
    
    if (PInputData)
    {
        delete PInputData;
        PInputData = nullptr;
    }
}

// ====================================================================
// Vehicle 생성 및 초기화
// ====================================================================

void UVehicleMovementComponent::SetupVehicle()
{
    ReleaseVehicle();

    PxRigidDynamic* RigidActor = GetValidDynamicActor();
    if (!RigidActor)
    {
        return;
    }

    const float OldMass = RigidActor->getMass();
    const float NewMass = VEHICLE_MASS; 
    
    RigidActor->setMass(NewMass);
    
    if (OldMass > 0.0f)
    {
        const float ScaleRatio = NewMass / OldMass;
        PxVec3 InertiaTensor = RigidActor->getMassSpaceInertiaTensor();
        RigidActor->setMassSpaceInertiaTensor(InertiaTensor * ScaleRatio);
    }

    SetupWheelShape(RigidActor);

    SetupWheelSimulationData(RigidActor);

    SetupDriveSimulationData(RigidActor);

    SetupFrictionPairs();

    SetupBatchQuery();

    // PVD 버그 회피: Shape가 PVD에 등록되려면 simulate/fetchResults 사이클이 필요
    // 따라서 다음 프레임에 FilterData를 설정하도록 플래그만 설정
    // (PhysX Issue #286: isInstanceValid assertion 방지)
    bPendingFilterDataSetup = true;
}

void UVehicleMovementComponent::ReleaseVehicle()
{
    if (PVehicleDrive)
    {
        PVehicleDrive->free(); PVehicleDrive = nullptr;
    }
    
    if (PWheelsSimData)
    {
        PWheelsSimData->free(); PWheelsSimData = nullptr;
    }
    
    ReleaseBatchQuery();
    
    ReleaseFrictionPairs();
}

void UVehicleMovementComponent::SetupWheelShape(physx::PxRigidDynamic* RigidActor)
{
    // 기존 Shape들 (차체)을 PendingChassisShapes에 저장 (FilterData는 나중에 설정)
    const PxU32 NumShapes = RigidActor->getNbShapes();
    PendingChassisShapes.SetNum(NumShapes);
    RigidActor->getShapes(PendingChassisShapes.GetData(), NumShapes);

    // NOTE: setQueryFilterData는 FinalizeShapeFilterData()에서 호출
    // PVD 연결 시 isInstanceValid assertion 방지를 위해 지연 설정

    PxMaterial* WheelMat = GPhysicalMaterial ? GPhysicalMaterial->GetPxMaterial() : nullptr;
    if (!WheelMat)
    {
        return;
    }

    PendingWheelShapes.Empty();

    for (int32 i = 0; i < MAX_WHEELS; i++)
    {
        if (VehicleWheels[i])
        {
            PxSphereGeometry WheelGeom(VehicleWheels[i]->WheelRadius);

            PxShape* NewWheelShape = PxRigidActorExt::createExclusiveShape(*RigidActor, WheelGeom, *WheelMat);

            if (NewWheelShape)
            {
                PxVec3 Position = U2PVector(WheelSetups[i].BoneOffset + WheelSetups[i].VisualOffset);
                NewWheelShape->setLocalPose(PxTransform(Position));

                // NOTE: setQueryFilterData는 FinalizeShapeFilterData()에서 호출
                NewWheelShape->setFlag(PxShapeFlag::eSIMULATION_SHAPE, false);
                NewWheelShape->setFlag(PxShapeFlag::eSCENE_QUERY_SHAPE, false);

                NewWheelShape->setContactOffset(0.02f);
                NewWheelShape->setRestOffset(0.0f);

                PendingWheelShapes.Add(NewWheelShape);
            }
        }
    }
}

void UVehicleMovementComponent::FinalizeShapeFilterData()
{
    // 차체 Shape들에 FilterData 설정
    for (PxShape* Shape : PendingChassisShapes)
    {
        if (Shape)
        {
            PxFilterData QueryFilterData = Shape->getQueryFilterData();
            QueryFilterData.word3 = VEHICLE_SURFACE_TYPE_CHASSIS;
            Shape->setQueryFilterData(QueryFilterData);
        }
    }

    // 휠 Shape들에 FilterData 설정
    for (PxShape* Shape : PendingWheelShapes)
    {
        if (Shape)
        {
            PxFilterData QueryData = Shape->getQueryFilterData();
            QueryData.word3 = VEHICLE_SURFACE_TYPE_CHASSIS;
            Shape->setQueryFilterData(QueryData);
        }
    }

    PendingChassisShapes.Empty();
    PendingWheelShapes.Empty();
}

void UVehicleMovementComponent::SetupWheelSimulationData(physx::PxRigidDynamic* RigidActor)
{
    const PxU32 TotalShapes = RigidActor->getNbShapes();
    const int32 WheelShapeStartIndex = TotalShapes - MAX_WHEELS;
    assert(WheelShapeStartIndex >= 0);

    PWheelsSimData = PxVehicleWheelsSimData::allocate(MAX_WHEELS);

    const float TotalMass = RigidActor->getMass();
    const PxVec3 BodyCMOffset = RigidActor->getCMassLocalPose().p;

    PxF32 SprungMasses[MAX_WHEELS];
    PxVec3 WheelOffsets[MAX_WHEELS];
    TArray<uint32> ValidWheelIndices;

    for (int32 i = 0; i < MAX_WHEELS; i++)
    {
        if (VehicleWheels[i])
        {
            WheelOffsets[ValidWheelIndices.Num()] = U2PVector(WheelSetups[i].BoneOffset);
            ValidWheelIndices.Add(i);
        }
    }

    // 마지막 파라미터: 1 = Y-up (PhysX 좌표계), 2 = Z-up
    PxVehicleComputeSprungMasses(ValidWheelIndices.Num(), WheelOffsets, BodyCMOffset, TotalMass, 1, SprungMasses);

    for (int32 i = 0; i < ValidWheelIndices.Num(); i++)
    {
        int32 WheelIndex = ValidWheelIndices[i];
        VehicleWheels[WheelIndex]->SprungMass = SprungMasses[i];
        PWheelsSimData->setSuspensionData(WheelIndex, VehicleWheels[WheelIndex]->GetPxSuspensionData());
    }

    for (int32 i = 0; i < MAX_WHEELS; i++)
    {
        if (VehicleWheels[i])
        {
            PWheelsSimData->setWheelData(i, VehicleWheels[i]->GetPxWheelData());
            PWheelsSimData->setSuspensionData(i, VehicleWheels[i]->GetPxSuspensionData());

            // 타이어 데이터
            PxVehicleTireData TireData;
            PWheelsSimData->setTireData(i, TireData);

            PxVec3 Offset = U2PVector(WheelSetups[i].BoneOffset);
            PWheelsSimData->setWheelCentreOffset(i, Offset);
            // 서스펜션 레이캐스트 방향: 아래로 (-Y)
            PWheelsSimData->setSuspTravelDirection(i, PxVec3(0, -1, 0));
            // 힘 적용 지점은 휠 중심
            PWheelsSimData->setSuspForceAppPointOffset(i, Offset);
            PWheelsSimData->setTireForceAppPointOffset(i, Offset);
            PWheelsSimData->setWheelShapeMapping(i, WheelShapeStartIndex + i);
        }
    }
}

void UVehicleMovementComponent::SetupDriveSimulationData(physx::PxRigidDynamic* RigidActor)
{
    PxVehicleDriveSimData4W DriveSimData;
    DriveSimData.setEngineData(EngineSetup.GetPxEngineData());
    DriveSimData.setGearsData(TransmissionSetup.GetPxGearsData());
    
    PxVehicleClutchData ClutchData;
    ClutchData.mStrength = 10.0f;
    DriveSimData.setClutchData(ClutchData);

    PxVehicleAutoBoxData AutoBoxData;
    // 변속 지연 시간 (기본값 2초는 너무 김)
    AutoBoxData.setLatency(0.2f);
    // 상향 변속 비율 (엔진 최대 RPM 대비 이 비율 이상이면 상위 기어로)
    AutoBoxData.setUpRatios(PxVehicleGearsData::eFIRST, 0.65f);
    AutoBoxData.setUpRatios(PxVehicleGearsData::eSECOND, 0.65f);
    AutoBoxData.setUpRatios(PxVehicleGearsData::eTHIRD, 0.65f);
    AutoBoxData.setUpRatios(PxVehicleGearsData::eFOURTH, 0.65f);
    AutoBoxData.setUpRatios(PxVehicleGearsData::eFIFTH, 0.65f);
    // 하향 변속 비율 (이 비율 이하면 하위 기어로)
    AutoBoxData.setDownRatios(PxVehicleGearsData::eSECOND, 0.5f);
    AutoBoxData.setDownRatios(PxVehicleGearsData::eTHIRD, 0.5f);
    AutoBoxData.setDownRatios(PxVehicleGearsData::eFOURTH, 0.5f);
    AutoBoxData.setDownRatios(PxVehicleGearsData::eFIFTH, 0.5f);
    AutoBoxData.setDownRatios(PxVehicleGearsData::eSIXTH, 0.5f);
    DriveSimData.setAutoBoxData(AutoBoxData);
    
    // 4륜 구동 디퍼런셜 설정
    PxVehicleDifferential4WData DiffData;
    DiffData.mType = PxVehicleDifferential4WData::eDIFF_TYPE_LS_4WD;
    DriveSimData.setDiffData(DiffData);

    if (PWheelsSimData)
    {
        PxVec3 FL = PWheelsSimData->getWheelCentreOffset(0);
        PxVec3 FR = PWheelsSimData->getWheelCentreOffset(1);
        PxVec3 RL = PWheelsSimData->getWheelCentreOffset(2);
        PxVec3 RR = PWheelsSimData->getWheelCentreOffset(3);

        // U2PVector 변환 후 좌표계: PhysX X=좌우, PhysX Y=상하, PhysX Z=전후
        PxVehicleAckermannGeometryData Ackermann;
        Ackermann.mAxleSeparation = PxAbs(FL.z - RL.z);  // 전후 거리 (PhysX Z)
        Ackermann.mFrontWidth = PxAbs(FL.x - FR.x);      // 좌우 폭 (PhysX X)
        Ackermann.mRearWidth = PxAbs(RL.x - RR.x);       // 좌우 폭 (PhysX X)
        DriveSimData.setAckermannGeometryData(Ackermann);
    }

    if (PWheelsSimData)
    {
        PVehicleDrive = PxVehicleDrive4W::allocate(4);

        PVehicleDrive->setup(GPhysXSDK, RigidActor, *PWheelsSimData, DriveSimData, 0);

        // setup() 성공 여부 확인 - getRigidDynamicActor()가 유효한 포인터를 반환해야 함
        if (PVehicleDrive->getRigidDynamicActor() != RigidActor)
        {
            UE_LOG("[Vehicle Error] PxVehicleDrive4W::setup() 실패 - RigidActor가 올바르게 설정되지 않음");
            PVehicleDrive->free();
            PVehicleDrive = nullptr;
            return;
        }

        // 자동 기어 사용 (PhysX 기본값)
        PVehicleDrive->mDriveDynData.setUseAutoGears(true);
    }

    RigidActor->setRigidBodyFlag(PxRigidBodyFlag::eKINEMATIC, false);
    RigidActor->setActorFlag(PxActorFlag::eDISABLE_GRAVITY, false);
    
    RigidActor->setLinearDamping(0.05f);
    RigidActor->setAngularDamping(0.05f);

    RigidActor->setMaxDepenetrationVelocity(5.0f);
    RigidActor->setSolverIterationCounts(12, 4);
}

// ====================================================================
// 배치 쿼리 (Raycast) 관리
// ====================================================================

void UVehicleMovementComponent::SetupBatchQuery()
{
    ReleaseBatchQuery();
    
    if (!PVehicleDrive) { return; }
    
    PxScene* Scene = PVehicleDrive->getRigidDynamicActor()->getScene();
    if (!Scene) { return; }

    BatchQueryResults     = new PxRaycastQueryResult[MAX_WHEELS];
    BatchQueryTouchBuffer = new PxRaycastHit[MAX_WHEELS];

    PxBatchQueryDesc SqDesc(MAX_WHEELS, 0, 0);

    SqDesc.queryMemory.userRaycastResultBuffer = BatchQueryResults;
    SqDesc.queryMemory.userRaycastTouchBuffer  = BatchQueryTouchBuffer;
    SqDesc.queryMemory.raycastTouchBufferSize  = MAX_WHEELS;

    SqDesc.preFilterShader = WheelRaycastPreFilter;

    SuspensionBatchQuery = Scene->createBatchQuery(SqDesc);
}

void UVehicleMovementComponent::ReleaseBatchQuery()
{
    if (SuspensionBatchQuery)
    {
        SuspensionBatchQuery->release();
        SuspensionBatchQuery = nullptr;
    }

    if (BatchQueryResults)
    {
        delete[] BatchQueryResults;
        BatchQueryResults = nullptr;
    }

    if (BatchQueryTouchBuffer)
    {
        delete[] BatchQueryTouchBuffer;
        BatchQueryTouchBuffer = nullptr;
    }
}

// ====================================================================
// 마찰력 설정 (Helper)
// ====================================================================

void UVehicleMovementComponent::SetupFrictionPairs()
{
    if (FrictionPairs)
    {
        FrictionPairs->release();
        FrictionPairs = nullptr;
    }

    PxMaterial* DefaultMat = nullptr;
    if (GPhysicalMaterial)
    {
        DefaultMat = GPhysicalMaterial->GetPxMaterial();
    }

    if (!DefaultMat) { return; }

    PxVehicleDrivableSurfaceType SurfaceType;
    SurfaceType.mType = 0;

    FrictionPairs = PxVehicleDrivableSurfaceToTireFrictionPairs::allocate(1, 1);

    const PxMaterial* Mats[] = { DefaultMat };
    PxVehicleDrivableSurfaceType SurfaceTypes[] = { SurfaceType };
    
    FrictionPairs->setup(1, 1, Mats, SurfaceTypes);
    
    FrictionPairs->setTypePairFriction(0, 0, 1.0f);
}

void UVehicleMovementComponent::ReleaseFrictionPairs()
{
    if (FrictionPairs)
    {
        FrictionPairs->release();
        FrictionPairs = nullptr;
    }    
}

PxRigidDynamic* UVehicleMovementComponent::GetValidDynamicActor() const
{
    UPrimitiveComponent* MeshComp = Cast<UPrimitiveComponent>(UpdatedComponent);
    if (!MeshComp || !MeshComp->GetBodyInstance())
    {
        return nullptr;
    }

    FBodyInstance* BodyInst = MeshComp->GetBodyInstance();
    if (!BodyInst->RigidActor)
    {
        return nullptr;
    }

    return BodyInst->RigidActor->is<PxRigidDynamic>();
}

void UVehicleMovementComponent::TickComponent(float DeltaSeconds)
{
    Super::TickComponent(DeltaSeconds);

    if (!UpdatedComponent) { return; }

    UPrimitiveComponent* MeshComp = Cast<UPrimitiveComponent>(UpdatedComponent);
    if (!MeshComp || !MeshComp->GetBodyInstance()) { return; }

    // 지연 초기화: PVehicleDrive가 없으면 Scene이 준비되었는지 확인 후 SetupVehicle 호출
    if (!PVehicleDrive)
    {
        PxRigidDynamic* RigidActor = GetValidDynamicActor();
        if (RigidActor && RigidActor->getScene())
        {
            SetupVehicle();
        }
        return;
    }

    // PVD 버그 회피: Shape 생성 후 simulate/fetchResults 사이클이 지난 뒤 FilterData 설정
    // 이 시점이면 PVD가 새 Shape들을 인식했으므로 안전하게 설정 가능
    if (bPendingFilterDataSetup)
    {
        FinalizeShapeFilterData();
        bPendingFilterDataSetup = false;
    }

    // DeltaTime이 0이면 PxVehicleUpdates에서 NaN 발생 가능 (0으로 나누기)
    if (DeltaSeconds < 0.0001f)
    {
        return;
    }

    ProcessVehicleInput(DeltaSeconds);

    UpdateVehicleSimulation(DeltaSeconds);

    // PhysX AutoBox가 작동하지 않아 수동 변속 로직 사용
    UpdateManualAutoGearBox(DeltaSeconds);

    // @note 디버그용 (불필요할 시 주석처리 혹은 삭제할 것)
    if (PVehicleDrive)
    {
        // 엔진 회전수 (Rad/s -> RPM)
        float RPM = PVehicleDrive->mDriveDynData.getEngineRotationSpeed() * 9.55f;
        // 현재 기어
        int32 Gear = PVehicleDrive->mDriveDynData.getCurrentGear();
        // 타겟 기어
        int32 TargetGear = PVehicleDrive->mDriveDynData.getTargetGear();
        // 자동 기어 사용 여부
        bool bAutoGears = PVehicleDrive->mDriveDynData.getUseAutoGears();
        // 실제 바퀴 회전 속도 (첫번째 바퀴)
        float WheelSpeed = PVehicleDrive->mWheelsDynData.getWheelRotationSpeed(0);

        UE_LOG("RPM: %.1f | Gear: %d | Target: %d | Auto: %d | WheelSpeed: %.1f", RPM, Gear, TargetGear, bAutoGears, WheelSpeed);
    }
}


void UVehicleMovementComponent::PerformSuspensionRaycasts(float DeltaTime)
{
    if (!SuspensionBatchQuery)
    {
        SetupBatchQuery();
        if (!SuspensionBatchQuery)
        {
            return;
        }
    }
    
    PxVehicleWheels* Vehicles[1] = { PVehicleDrive };
    
    PxVehicleSuspensionRaycasts(SuspensionBatchQuery, 1, Vehicles, MAX_WHEELS, BatchQueryResults);

    SuspensionBatchQuery->execute();
}

bool UVehicleMovementComponent::IsVehicleInAir() const
{
    if (!BatchQueryResults) return true; 

    for (int i = 0; i < MAX_WHEELS; i++)
    {
        if (BatchQueryResults[i].block.actor != nullptr)
        {
            return false;
        }
    }
    return true;
}

void UVehicleMovementComponent::ProcessVehicleInput(float DeltaTime)
{
    if (!PInputData || !PVehicleDrive)
    {
        return;
    }

    bool bIsInAir = IsVehicleInAir();

    PxVehicleDrive4WSmoothAnalogRawInputsAndSetAnalogInputs(
        gPadSmoothingData,
        gSteerVsForwardSpeedTable,
        *PInputData,
        DeltaTime,
        bIsInAir,
        *PVehicleDrive
    );
}

void UVehicleMovementComponent::UpdateVehicleSimulation(float DeltaTime)
{
    PerformSuspensionRaycasts(DeltaTime);

    PxScene* Scene = PVehicleDrive->getRigidDynamicActor()->getScene();
    // PhysX Y-up 좌표계: 중력은 -Y 방향
    PxVec3 Gravity = Scene ? Scene->getGravity() : PxVec3(0, -9.81f, 0);

    if (!FrictionPairs)
    {
        SetupFrictionPairs();
        if (!FrictionPairs)
        {
            return;
        }
    }

    PxVehicleWheels* Vehicles[] = { PVehicleDrive };
    PxVehicleUpdates(DeltaTime, Gravity, *FrictionPairs, 1, Vehicles, nullptr);

    PxRigidDynamic* Actor = PVehicleDrive->getRigidDynamicActor();
    if (Actor && Actor->isSleeping())
    {
        Actor->wakeUp();
    }
}

// ====================================================================
// 입력 인터페이스 (PlayerController에서 호출)
// ====================================================================

void UVehicleMovementComponent::SetThrottleInput(float Throttle)
{
    if (PInputData) PInputData->setAnalogAccel(Throttle);
}

void UVehicleMovementComponent::SetSteeringInput(float Steering)
{
    // 좌표계 변환으로 좌우가 반전되므로 입력 반전
    if (PInputData) PInputData->setAnalogSteer(-Steering);
}

void UVehicleMovementComponent::SetBrakeInput(float Brake)
{
    if (PInputData) PInputData->setAnalogBrake(Brake);
}

void UVehicleMovementComponent::SetHandbrakeInput(bool bNewHandbrake)
{
    if (PInputData) PInputData->setAnalogHandbrake(bNewHandbrake ? 1.0f : 0.0f);
}

float UVehicleMovementComponent::GetForwardSpeed() const
{
    if (PVehicleDrive)
    {
        // 차량 전용 변환으로 좌표계가 정렬되었으므로 정상 속도
        return PVehicleDrive->computeForwardSpeed();
    }
    return 0.0f;
}

float UVehicleMovementComponent::GetEngineRPM() const
{
    if (PVehicleDrive)
    {
        // Rad/s -> RPM 변환 (60 / 2π ≈ 9.55)
        return PVehicleDrive->mDriveDynData.getEngineRotationSpeed() * 9.55f;
    }
    return 0.0f;
}

int32 UVehicleMovementComponent::GetCurrentGear() const
{
    if (PVehicleDrive)
    {
        int32 Gear = PVehicleDrive->mDriveDynData.getCurrentGear();
        // 차량 전용 변환으로 좌표계가 정렬되었으므로 정상 기어 매핑
        // PhysX eREVERSE(0) = 후진(-1)
        // PhysX eNEUTRAL(1) = 중립(0)
        // PhysX eFIRST+(2+) = 전진(1+)
        if (Gear == 0) return -1;      // Reverse
        if (Gear == 1) return 0;       // Neutral
        return Gear - 1;               // Forward gears (eFIRST=2 → 1, eSECOND=3 → 2, etc.)
    }
    return 0;
}

void UVehicleMovementComponent::ApplyBoostForce(float BoostStrength)
{
    if (!PVehicleDrive) return;

    PxRigidDynamic* Actor = PVehicleDrive->getRigidDynamicActor();
    if (!Actor) return;

    // 차량의 전진 방향 (PhysX 로컬 -Z축 = 엔진 +X축)
    // PxVehicleSetBasisVectors로 forward=-Z 설정됨
    PxTransform Pose = Actor->getGlobalPose();
    PxVec3 ForwardDir = Pose.q.rotate(PxVec3(0.0f, 0.0f, -1.0f));

    // 부스터 힘 적용
    PxVec3 BoostForce = ForwardDir * BoostStrength;
    Actor->addForce(BoostForce, PxForceMode::eACCELERATION);
}

void UVehicleMovementComponent::SetGearToDrive()
{
    if (PVehicleDrive)
    {
        const PxU32 CurrentGear = PVehicleDrive->mDriveDynData.getCurrentGear();
        const PxU32 TargetGear = PVehicleDrive->mDriveDynData.getTargetGear();

        // PhysX autobox 비활성화 (수동 변속 로직 사용)
        PVehicleDrive->mDriveDynData.setUseAutoGears(false);

        // 변속 중이면 대기
        if (CurrentGear != TargetGear) return;

        // 이미 전진 기어면 수동 변속 로직에 맡김
        if (CurrentGear >= physx::PxVehicleGearsData::eFIRST) return;

        // 후진/중립일 때만 1단으로 변경
        PVehicleDrive->mDriveDynData.forceGearChange(physx::PxVehicleGearsData::eFIRST);
        GearSwitchTimer = 0.0f; // 변속 쿨다운 리셋
    }
}

void UVehicleMovementComponent::SetGearToReverse()
{
    if (PVehicleDrive)
    {
        // 후진 기어는 수동 유지 (자동 변속 시 후진으로 안 바뀜)
        PVehicleDrive->mDriveDynData.setUseAutoGears(false);
        PVehicleDrive->mDriveDynData.forceGearChange(physx::PxVehicleGearsData::eREVERSE);
    }
}

FTransform UVehicleMovementComponent::GetWheelTransform(int32 WheelIndex) const
{
    if (!PVehicleDrive || WheelIndex < 0 || WheelIndex >= static_cast<int32>(PVehicleDrive->mWheelsSimData.getNbWheels()))
    {
        return FTransform();
    }

    physx::PxRigidDynamic* Actor = PVehicleDrive->getRigidDynamicActor();
    if (!Actor)
    {
        return FTransform();
    }

    const int32 ShapeIndex = PVehicleDrive->mWheelsSimData.getWheelShapeMapping(WheelIndex);
    if (ShapeIndex < 0)
    {
        return FTransform();
    }

    physx::PxShape* WheelShape = nullptr;
    const PxU32 NumShapesRetrieved = Actor->getShapes(&WheelShape, 1, ShapeIndex);

    if (NumShapesRetrieved > 0 && WheelShape)
    {
        return P2UTransform(WheelShape->getLocalPose());
    }

    return FTransform();
}

void UVehicleMovementComponent::UpdateManualAutoGearBox(float DeltaTime)
{
    if (!PVehicleDrive) return;

    const PxU32 CurrentGear = PVehicleDrive->mDriveDynData.getCurrentGear();
    const PxU32 TargetGear = PVehicleDrive->mDriveDynData.getTargetGear();

    // 변속 중이면 대기
    if (CurrentGear != TargetGear) return;

    // 전진 기어가 아니면 처리 안 함 (중립, 후진)
    if (CurrentGear < physx::PxVehicleGearsData::eFIRST) return;

    // 변속 쿨다운 타이머
    GearSwitchTimer += DeltaTime;
    if (GearSwitchTimer < GearSwitchCooldown) return;

    // 현재 엔진 RPM 비율 계산
    const float EngineOmega = PVehicleDrive->mDriveDynData.getEngineRotationSpeed();
    const float MaxOmega = EngineSetup.MaxRPM * (PxPi / 30.0f);
    const float RPMRatio = EngineOmega / MaxOmega;

    // 상향 변속: RPM이 UpShiftRatio 이상이고 최대 기어가 아닐 때
    if (RPMRatio >= UpShiftRatio && CurrentGear < physx::PxVehicleGearsData::eFIFTH)
    {
        PVehicleDrive->mDriveDynData.forceGearChange(CurrentGear + 1);
        GearSwitchTimer = 0.0f;
    }
    // 하향 변속: RPM이 DownShiftRatio 이하이고 1단이 아닐 때
    else if (RPMRatio <= DownShiftRatio && CurrentGear > physx::PxVehicleGearsData::eFIRST)
    {
        PVehicleDrive->mDriveDynData.forceGearChange(CurrentGear - 1);
        GearSwitchTimer = 0.0f;
    }
}
