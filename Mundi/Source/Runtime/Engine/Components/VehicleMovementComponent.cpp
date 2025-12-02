#include "pch.h"
#include "VehicleMovementComponent.h"
#include "SceneComponent.h"
#include "PrimitiveComponent.h"
#include "BodyInstance.h"
#include "PhysScene.h"
#include "Vehicle/VehicleWheel.h"

using namespace physx;

UVehicleMovementComponent::UVehicleMovementComponent()
    : PVehicleDrive(nullptr)
    , PInputData(nullptr)
    , PWheelsSimData(nullptr)
    , PDriveSimData(nullptr)
{
    WheelSetups.SetNum(4); 
}

UVehicleMovementComponent::~UVehicleMovementComponent()
{
}

void UVehicleMovementComponent::InitializeComponent()
{
    Super::InitializeComponent();
    PInputData = new PxVehicleDrive4WRawInputData();
    
    // [수정 1] 여기서 SetupVehicle 하지 않음 (너무 이름)
}

void UVehicleMovementComponent::BeginPlay()
{
    Super::BeginPlay();
    // [수정 1] BeginPlay에서 초기화 (물리 액터 생성 완료 보장)
    SetupVehicle();
}

void UVehicleMovementComponent::SetupVehicle()
{
    // 기존 데이터 정리 (재초기화 대비)
    if (PVehicleDrive) { PVehicleDrive->free(); PVehicleDrive = nullptr; }
    if (PWheelsSimData) { PWheelsSimData->free(); PWheelsSimData = nullptr; }

    UPrimitiveComponent* MeshComp = Cast<UPrimitiveComponent>(UpdatedComponent);
    if (!MeshComp || !MeshComp->GetBodyInstance()) return;

    FBodyInstance* BodyInst = MeshComp->GetBodyInstance();
    // PhysX Actor가 없으면 리턴
    if (!BodyInst->RigidActor) return;

    PxRigidDynamic* RigidActor = BodyInst->RigidActor->is<PxRigidDynamic>();
    if (!RigidActor) return;

    // [중요] 기존 쉐이프 개수 저장 (이 뒤에 바퀴가 추가됨)
    int32 WheelShapeStartIndex = RigidActor->getNbShapes(); 
    
    PxMaterial* WheelMat = nullptr;
    if (GPhysicalMaterial) WheelMat = GPhysicalMaterial->GetPxMaterial();
    if (!WheelMat) WheelMat = GPhysXSDK->createMaterial(0.5f, 0.5f, 0.5f); // 비상용 재질

    // 1. RigidActor에 바퀴 Shape 4개 추가
    for (int i = 0; i < 4; i++)
    {
        if (WheelSetups[i])
        {
            PxSphereGeometry WheelGeom(WheelSetups[i]->WheelRadius);
            PxShape* NewWheelShape = GPhysXSDK->createShape(WheelGeom, *WheelMat);
        
            if (NewWheelShape)
            {
                float X = (i < 2) ? 1.5f : -1.5f;     // 앞/뒤 (차 크기에 맞춰 조정 필요)
                float Y = (i % 2 == 0) ? -0.9f : 0.9f; // 좌/우
                float Z = -0.2f; // 차체 중심보다 약간 아래

                PxTransform LocalPose(PxVec3(X, Y, Z));
                NewWheelShape->setLocalPose(LocalPose);
                NewWheelShape->setFlag(PxShapeFlag::eSIMULATION_SHAPE, true);
                NewWheelShape->setFlag(PxShapeFlag::eSCENE_QUERY_SHAPE, true);

                RigidActor->attachShape(*NewWheelShape);
                NewWheelShape->release();
            }
        }
    }

    // 2. Wheel Sim Data 설정
    PWheelsSimData = physx::PxVehicleWheelsSimData::allocate(4);
    for (int i = 0; i < 4; i++)
    {
        if (WheelSetups[i])
        {
            PWheelsSimData->setWheelData(i, WheelSetups[i]->GetPxWheelData());
            PWheelsSimData->setSuspensionData(i, WheelSetups[i]->GetPxSuspensionData());
            
            // Shape 위치와 동일한 오프셋 설정
            float X = (i < 2) ? 1.5f : -1.5f;     
            float Y = (i % 2 == 0) ? -0.9f : 0.9f;
            float Z = -0.2f;
            PWheelsSimData->setWheelCentreOffset(i, PxVec3(X, Y, Z)); 
            
            PWheelsSimData->setSuspTravelDirection(i, PxVec3(0, 0, -1));

            // [수정 2] 매핑 인덱스 수정 (i -> WheelShapeStartIndex + i)
            // 이걸 안 하면 0번(차체)을 바퀴로 인식해서 비틀립니다.
            PWheelsSimData->setWheelShapeMapping(i, WheelShapeStartIndex + i);
        }
    }

    // 3. Drive Sim Data 설정
    physx::PxVehicleDriveSimData4W DriveSimData;
    DriveSimData.setEngineData(EngineSetup.GetPxEngineData());
    DriveSimData.setGearsData(PxVehicleGearsData());
    
    PxVehicleDifferential4WData DiffData;
    DiffData.mType = PxVehicleDifferential4WData::eDIFF_TYPE_LS_4WD;
    DriveSimData.setDiffData(DiffData);

    // 4. 차량 인스턴스 생성
    PVehicleDrive = physx::PxVehicleDrive4W::allocate(4);
    PVehicleDrive->setup(GPhysXSDK, RigidActor, *PWheelsSimData, DriveSimData, 0);

    // [필수] Actor의 질량 재계산 (바퀴가 추가되었으므로)
    PxRigidBodyExt::updateMassAndInertia(*RigidActor, 1500.0f);
}

void UVehicleMovementComponent::TickComponent(float DeltaSeconds)
{
    Super::TickComponent(DeltaSeconds);

    if (!PVehicleDrive || !UpdatedComponent) return;

    // [수정 4] Actor 유효성 정밀 검사
    UPrimitiveComponent* MeshComp = Cast<UPrimitiveComponent>(UpdatedComponent);
    if (!MeshComp) return;
    
    FBodyInstance* BodyInst = MeshComp->GetBodyInstance();
    // 현재 메쉬의 실제 Actor와 차량이 들고 있는 Actor가 다르면 (재생성된 경우)
    if (!BodyInst || BodyInst->RigidActor != PVehicleDrive->getRigidDynamicActor())
    {
        // 배우가 바뀌었으므로 재설정하거나 리턴 (여기선 안전하게 리턴하고 로그)
        // SetupVehicle(); // 동적 재설정이 필요하면 호출
        return; 
    }

    // 1. Raycast
    PerformSuspensionRaycasts(DeltaSeconds);

    // 2. Update
    PxScene* Scene = PVehicleDrive->getRigidDynamicActor()->getScene();
    PxVec3 Gravity = Scene ? Scene->getGravity() : PxVec3(0, 0, -9.81f);

    // [수정 3] FrictionPairs 안전 초기화
    static PxVehicleDrivableSurfaceToTireFrictionPairs* FrictionPairs = nullptr;
    if (!FrictionPairs)
    {
        const PxMaterial* DefaultMat = GPhysicalMaterial->GetPxMaterial();

        FrictionPairs = PxVehicleDrivableSurfaceToTireFrictionPairs::allocate(1, 1);
        FrictionPairs->setup(1, 1, &DefaultMat, nullptr);
        FrictionPairs->setTypePairFriction(0, 0, 1.0f); // 기본 마찰 1.0
    }

    // 포인터가 null이 아님을 보장하고 호출
    if (FrictionPairs)
    {
        PxVehicleUpdates(DeltaSeconds, Gravity, *FrictionPairs, 1, (PxVehicleWheels**)&PVehicleDrive, nullptr);
    }
}

void UVehicleMovementComponent::OnRegister(UWorld* InWorld)
{
    Super::OnRegister(InWorld);
}

void UVehicleMovementComponent::OnUnregister()
{
    Super::OnUnregister();
    if (PInputData) { delete PInputData; PInputData = nullptr; }
    if (PWheelsSimData) { PWheelsSimData->free(); PWheelsSimData = nullptr; }
    // PxVehicleDrive4W는 free()를 호출해야 함 (PxBase 상속)
    if (PVehicleDrive) { PVehicleDrive->free(); PVehicleDrive = nullptr; }
}

void UVehicleMovementComponent::PerformSuspensionRaycasts(float DeltaTime)
{
    // PhysX Scene Query를 사용하여 바퀴 아래를 검사
    PxVehicleWheels* Vehicles[1] = { PVehicleDrive };
    
    // 레이캐스트 결과를 담을 버퍼
    PxRaycastQueryResult RaycastResults[4];
    PxVehicleSuspensionRaycasts(
        nullptr, // BatchQuery (널이면 내부적으로 즉시 쿼리 수행 - 성능상 BatchQuery 권장)
        1, 
        Vehicles, 
        4, // 쿼리 개수 (바퀴 수)
        RaycastResults
    );
}

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

FTransform UVehicleMovementComponent::GetWheelTransform(int32 WheelIndex) const
{
    // 1. 기본 유효성 검사
    if (!PVehicleDrive || WheelIndex >= 4)
    {
        return FTransform();
    }

    // 2. [핵심 수정] 현재 컴포넌트의 실제 BodyInstance와 Actor 확인
    UPrimitiveComponent* MeshComp = Cast<UPrimitiveComponent>(UpdatedComponent);
    if (!MeshComp || !MeshComp->GetBodyInstance()) 
    {
        return FTransform();
    }

    FBodyInstance* BodyInst = MeshComp->GetBodyInstance();
    
    // 3. [방어 코드] PhysX Vehicle이 알고 있는 Actor가 현재 진짜 Actor와 같은지 비교
    //    다르다면(이미 파괴된 Actor라면) 접근하지 말고 리턴해야 함.
    PxRigidDynamic* CurrentRealActor = BodyInst->RigidActor ? BodyInst->RigidActor->is<PxRigidDynamic>() : nullptr;
    PxRigidDynamic* CachedVehicleActor = PVehicleDrive->getRigidDynamicActor();

    if (!CurrentRealActor || !CachedVehicleActor || CurrentRealActor != CachedVehicleActor)
    {
        // 배우(Actor)가 교체되었거나 유효하지 않음 -> 시뮬레이션 데이터 접근 불가
        return FTransform();
    }

    // 4. Shape 매핑 인덱스 확인
    PxI32 ShapeIndex = PVehicleDrive->mWheelsSimData.getWheelShapeMapping(WheelIndex);
    if (ShapeIndex < 0)
    {
        return FTransform();
    }

    // 5. Shape 가져오기 (이제 Actor가 유효함이 보장됨)
    PxShape* WheelShape = nullptr;
    
    // getShapes의 리턴값(가져온 개수) 확인
    if (CachedVehicleActor->getShapes(&WheelShape, 1, ShapeIndex) == 1 && WheelShape)
    {
        return P2UTransform(WheelShape->getLocalPose());
    }

    return FTransform();
}