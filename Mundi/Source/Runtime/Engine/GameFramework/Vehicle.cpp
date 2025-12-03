#include "pch.h"
#include "Vehicle.h"
#include "InputComponent.h"

AVehicle::AVehicle()
    : CurrentForwardInput(0.0f)
    , CurrentSteeringInput(0.0f)
    , bIsBoosting(false)
{
    ChassisMesh = CreateDefaultSubobject<UStaticMeshComponent>("ChassisMesh");
    FString ChassisFileName = GDataDir + "/Model/Buggy/Buggy_Chassis.obj";
    ChassisMesh->SetStaticMesh(ChassisFileName);
    SetRootComponent(ChassisMesh);
    
    ChassisMesh->SetSimulatePhysics(true);
    ChassisMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

    FString WheelFileName[4] = {
        "/Model/Buggy/Buggy_Wheel_LF.obj",
        "/Model/Buggy/Buggy_Wheel_RF.obj",
        "/Model/Buggy/Buggy_Wheel_LB.obj",
        "/Model/Buggy/Buggy_Wheel_RB.obj"
    };

    WheelMeshes.SetNum(4);
    for (int i = 0; i < 4; i++)
    {
        FName WheelName = "WheelMesh_" + std::to_string(i);
        WheelMeshes[i] = CreateDefaultSubobject<UStaticMeshComponent>(WheelName);
        WheelMeshes[i]->SetupAttachment(ChassisMesh);
        WheelMeshes[i]->SetSimulatePhysics(false);
        WheelMeshes[i]->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        WheelMeshes[i]->SetStaticMesh(GDataDir + WheelFileName[i]);
    }

    VehicleMovement = CreateDefaultSubobject<UVehicleMovementComponent>("VehicleMovement");

    SpringArm = CreateDefaultSubobject<USpringArmComponent>("SpringArm");
    SpringArm->SetupAttachment(ChassisMesh);
    SpringArm->TargetArmLength = 10.0f;
    SpringArm->SocketOffset = FVector(-4.6f, 0.0f, 4.6f);

    Camera = CreateDefaultSubobject<UCameraComponent>("Camera");
    Camera->SetupAttachment(SpringArm);
    Camera->SetFarClipPlane(1000.f);
}

AVehicle::~AVehicle()
{
}

void AVehicle::BeginPlay()
{
    Super::BeginPlay();
}

void AVehicle::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    if (VehicleMovement)
    {
        VehicleMovement->SetSteeringInput(CurrentSteeringInput);

        const float ForwardSpeed = VehicleMovement->GetForwardSpeed();
        const float SpeedThreshold = 2.0f; // @note 속도가 이 값보다 낮으면 멈춘 것으로 간주

        if (CurrentForwardInput > 0.0f)
        {
            VehicleMovement->SetGearToDrive();
            VehicleMovement->SetThrottleInput(CurrentForwardInput);
            VehicleMovement->SetBrakeInput(0.0f);
        }
        else if (CurrentForwardInput < 0.0f)
        {
            if (ForwardSpeed > SpeedThreshold) 
            {
                VehicleMovement->SetGearToDrive(); 
                VehicleMovement->SetThrottleInput(0.0f);
                VehicleMovement->SetBrakeInput(-CurrentForwardInput); 
            }
            else 
            {
                VehicleMovement->SetGearToReverse(); 
                VehicleMovement->SetThrottleInput(-CurrentForwardInput);
                VehicleMovement->SetBrakeInput(0.0f);
            }
        }
        else
        {
            VehicleMovement->SetThrottleInput(0.0f);
            VehicleMovement->SetBrakeInput(0.0f);
        }
    }

    CurrentForwardInput = 0.0f;
    CurrentSteeringInput = 0.0f;

    // 부스터 적용
    if (bIsBoosting && VehicleMovement)
    {
        VehicleMovement->ApplyBoostForce(BoostStrength);
    }

    SyncWheelVisuals();
}

void AVehicle::SetupPlayerInputComponent(UInputComponent* InInputComponent)
{
    Super::SetupPlayerInputComponent(InInputComponent);

    if (!InInputComponent) return;

    // [축 바인딩] W/S로 가속/감속
    // W 키: Scale 1.0 -> 전진
    InInputComponent->BindAxis<AVehicle>("MoveForward_W", 'W', 1.0f, this, &AVehicle::MoveForward);
    // S 키: Scale -1.0 -> 후진/브레이크
    InInputComponent->BindAxis<AVehicle>("MoveForward_S", 'S', -1.0f, this, &AVehicle::MoveForward);

    // [축 바인딩] A/D로 조향
    // D 키: Scale 1.0 -> 우회전
    InInputComponent->BindAxis<AVehicle>("MoveRight_D", 'D', 1.0f, this, &AVehicle::MoveRight);
    // A 키: Scale -1.0 -> 좌회전
    InInputComponent->BindAxis<AVehicle>("MoveRight_A", 'A', -1.0f, this, &AVehicle::MoveRight);

    // [액션 바인딩] Space로 핸드브레이크
    // VK_SPACE는 0x20
    InInputComponent->BindAction<AVehicle>("Handbrake", VK_SPACE, this, &AVehicle::HandbrakePressed, &AVehicle::HandbrakeReleased);

    // [액션 바인딩] Shift로 부스터
    // VK_SHIFT는 0x10
    InInputComponent->BindAction<AVehicle>("Boost", VK_SHIFT, this, &AVehicle::BoostPressed, &AVehicle::BoostReleased);
}

void AVehicle::MoveForward(float Val)
{
    CurrentForwardInput += Val;
}

void AVehicle::MoveRight(float Val)
{
    CurrentSteeringInput += Val;
}

void AVehicle::HandbrakePressed()
{
    if (VehicleMovement)
    {
        VehicleMovement->SetHandbrakeInput(true);
    }
}

void AVehicle::HandbrakeReleased()
{
    if (VehicleMovement)
    {
        VehicleMovement->SetHandbrakeInput(false);
    }
}

void AVehicle::BoostPressed()
{
    bIsBoosting = true;
}

void AVehicle::BoostReleased()
{
    bIsBoosting = false;
}

void AVehicle::SyncWheelVisuals()
{
    if (!VehicleMovement) return;

    for (int i = 0; i < 4; i++)
    {
        if (WheelMeshes[i])
        {
            FTransform WheelTransform = VehicleMovement->GetWheelTransform(i);
            WheelMeshes[i]->SetRelativeLocation(WheelTransform.Translation);
            WheelMeshes[i]->SetRelativeRotation(WheelTransform.Rotation);
            WheelMeshes[i]->SetRelativeScale(WheelTransform.Scale3D);
        }
    }
}