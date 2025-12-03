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
    
    SetupVehicle();
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
    const PxU32 NumShapes = RigidActor->getNbShapes();
    TArray<PxShape*> Shapes;
    Shapes.SetNum(NumShapes);
    RigidActor->getShapes(Shapes.GetData(), NumShapes);

    for (PxShape* Shape : Shapes)
    {
        PxFilterData QueryFilterData = Shape->getQueryFilterData();
        QueryFilterData.word3 = VEHICLE_SURFACE_TYPE_CHASSIS;
        Shape->setQueryFilterData(QueryFilterData);
    }

    PxMaterial* WheelMat = GPhysicalMaterial ? GPhysicalMaterial->GetPxMaterial() : nullptr;
    if (!WheelMat)
    {
        return;
    }

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

                PxFilterData QueryData = NewWheelShape->getQueryFilterData();
                QueryData.word3 = VEHICLE_SURFACE_TYPE_CHASSIS;
                NewWheelShape->setQueryFilterData(QueryData);
                NewWheelShape->setFlag(PxShapeFlag::eSIMULATION_SHAPE, false);
                NewWheelShape->setFlag(PxShapeFlag::eSCENE_QUERY_SHAPE, false);

                NewWheelShape->setContactOffset(0.02f); 
                NewWheelShape->setRestOffset(0.0f);
            }
        }
    }
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

    PxVehicleComputeSprungMasses(ValidWheelIndices.Num(), WheelOffsets, BodyCMOffset, TotalMass, 2, SprungMasses);

    for (int32 i = 0; i < MAX_WHEELS; i++)
    {
        int32 WheelIndex = ValidWheelIndices[i];
        VehicleWheels[i]->SprungMass = SprungMasses[i];
        PWheelsSimData->setSuspensionData(WheelIndex, VehicleWheels[i]->GetPxSuspensionData());
    }

    for (int32 i = 0; i < MAX_WHEELS; i++)
    {
        if (VehicleWheels[i])
        {
            PWheelsSimData->setWheelData(i, VehicleWheels[i]->GetPxWheelData());
            PWheelsSimData->setTireData(i, PxVehicleTireData());
            PWheelsSimData->setSuspensionData(i, VehicleWheels[i]->GetPxSuspensionData());

            PxVec3 Offset = U2PVector(WheelSetups[i].BoneOffset);
            PWheelsSimData->setWheelCentreOffset(i, Offset); 
            PWheelsSimData->setSuspTravelDirection(i, PxVec3(0, 0, -1));
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
    AutoBoxData.setDownRatios(PxVehicleGearsData::eFIRST, 0.5f);
    AutoBoxData.setUpRatios(PxVehicleGearsData::eFIRST, 0.65f);
    DriveSimData.setAutoBoxData(AutoBoxData);
    
    PxVehicleDifferential4WData DiffData;
    DiffData.mType = PxVehicleDifferential4WData::eDIFF_TYPE_LS_4WD;
    DriveSimData.setDiffData(DiffData);

    if (PWheelsSimData)
    {
        PxVec3 FL = PWheelsSimData->getWheelCentreOffset(0);
        PxVec3 FR = PWheelsSimData->getWheelCentreOffset(1);
        PxVec3 RL = PWheelsSimData->getWheelCentreOffset(2);
        PxVec3 RR = PWheelsSimData->getWheelCentreOffset(3);

        PxVehicleAckermannGeometryData Ackermann;
        Ackermann.mAxleSeparation = PxAbs(FL.x - RL.x);
        Ackermann.mFrontWidth = PxAbs(FL.y - FR.y);
        Ackermann.mRearWidth = PxAbs(RL.y - RR.y);
        DriveSimData.setAckermannGeometryData(Ackermann);
    }

    if (PWheelsSimData)
    {
        PVehicleDrive = PxVehicleDrive4W::allocate(4);
    
        PVehicleDrive->setup(GPhysXSDK, RigidActor, *PWheelsSimData, DriveSimData, 0);

        PVehicleDrive->mDriveDynData.setUseAutoGears(true);
        PVehicleDrive->mDriveDynData.forceGearChange(PxVehicleGearsData::eFIRST);
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

    if (!PVehicleDrive || !UpdatedComponent) { return; }

    UPrimitiveComponent* MeshComp = Cast<UPrimitiveComponent>(UpdatedComponent);
    if (!MeshComp || !MeshComp->GetBodyInstance()) { return; }

    UpdateVehicleSimulation(DeltaSeconds);
    
    ProcessVehicleInput(DeltaSeconds);

    // @note 디버그용 (불필요할 시 주석처리 혹은 삭제할 것)
    if (PVehicleDrive)
    {
        // 엔진 회전수 (Rad/s -> RPM)
        float RPM = PVehicleDrive->mDriveDynData.getEngineRotationSpeed() * 9.55f;
        // 현재 기어
        int32 Gear = PVehicleDrive->mDriveDynData.getCurrentGear();
        // 실제 바퀴 회전 속도 (첫번째 바퀴)
        float WheelSpeed = PVehicleDrive->mWheelsDynData.getWheelRotationSpeed(0);
    
        UE_LOG("RPM: %.1f | Gear: %d | WheelSpeed: %.1f", RPM, Gear, WheelSpeed);
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
    PxVec3 Gravity = Scene ? Scene->getGravity() : PxVec3(0, 0, -9.81f);

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
    if (PInputData) PInputData->setAnalogSteer(Steering);
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
        // PhysX: 0=Reverse, 1=Neutral, 2+=Forward gears
        // 사용자 친화적 변환: -1=R, 0=N, 1+=전진기어
        if (Gear == 0) return -1;      // Reverse
        if (Gear == 1) return 0;       // Neutral
        return Gear - 1;               // Forward gears (1, 2, 3...)
    }
    return 0;
}

void UVehicleMovementComponent::ApplyBoostForce(float BoostStrength)
{
    if (!PVehicleDrive) return;

    PxRigidDynamic* Actor = PVehicleDrive->getRigidDynamicActor();
    if (!Actor) return;

    // 차량의 전진 방향 (로컬 X축)
    PxTransform Pose = Actor->getGlobalPose();
    PxVec3 ForwardDir = Pose.q.rotate(PxVec3(1.0f, 0.0f, 0.0f));

    // 부스터 힘 적용
    PxVec3 BoostForce = ForwardDir * BoostStrength;
    Actor->addForce(BoostForce, PxForceMode::eACCELERATION);
}

void UVehicleMovementComponent::SetGearToDrive()
{
    if (PVehicleDrive)
    {
        if (PVehicleDrive->mDriveDynData.getCurrentGear() < physx::PxVehicleGearsData::eFIRST)
        {
            PVehicleDrive->mDriveDynData.forceGearChange(physx::PxVehicleGearsData::eFIRST);
            PVehicleDrive->mDriveDynData.setUseAutoGears(true); 
        }
    }
}

void UVehicleMovementComponent::SetGearToReverse()
{
    if (PVehicleDrive)
    {
        if (PVehicleDrive->mDriveDynData.getCurrentGear() != physx::PxVehicleGearsData::eREVERSE)
        {
            PVehicleDrive->mDriveDynData.setUseAutoGears(false);
            PVehicleDrive->mDriveDynData.forceGearChange(physx::PxVehicleGearsData::eREVERSE);
        }
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
