#include "pch.h"
#include "Vehicle.h"
#include "InputComponent.h"

AVehicle::AVehicle()
{
    // ChassisMesh = CreateDefaultSubobject<UStaticMeshComponent>("ChassisMesh");
    ChassisMesh = CreateDefaultSubobject<UStaticMeshComponent>("ChassisMesh");
    SetRootComponent(ChassisMesh);
    
    ChassisMesh->SetSimulatePhysics(true);
    // ChassisMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

    WheelMeshes.SetNum(4);
    for (int i = 0; i < 4; i++)
    {
        FName WheelName = "WheelMesh_" + std::to_string(i);
        WheelMeshes[i] = CreateDefaultSubobject<UStaticMeshComponent>(WheelName);
        WheelMeshes[i]->SetupAttachment(ChassisMesh);

        WheelMeshes[i]->SetSimulatePhysics(false);
        // WheelMeshes[i]->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    }

    VehicleMovement = CreateDefaultSubobject<UVehicleMovementComponent>("VehicleMovement");

    SpringArm = CreateDefaultSubobject<USpringArmComponent>("SpringArm");
    SpringArm->SetupAttachment(ChassisMesh);
    SpringArm->TargetArmLength = 10.0f;
    SpringArm->SetRelativeRotation(FQuat::MakeFromEulerZYX(FVector(0, 15, 0)));

    Camera = CreateDefaultSubobject<UCameraComponent>("Camera");
    Camera->SetupAttachment(SpringArm);
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

    SyncWheelVisuals();
}

void AVehicle::SetupPlayerInputComponent(UInputComponent* InInputComponent)
{
    Super::SetupPlayerInputComponent(InInputComponent);

    if (!InInputComponent) return;

    // [축 바인딩] W/S로 가속/감속
    // InputComponent.h의 BindAxis 템플릿 사용
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
}

void AVehicle::MoveForward(float Val)
{
    if (!VehicleMovement) return;

    // W를 누르면 Val = 1.0 (스로틀)
    // S를 누르면 Val = -1.0 (브레이크/후진)
    // 둘 다 안 누르면 호출되지 않거나, InputManager 구현에 따라 0이 들어올 수 있음

    if (Val > 0.0f)
    {
        // 전진
        VehicleMovement->SetThrottleInput(Val);
        VehicleMovement->SetBrakeInput(0.0f);
    }
    else if (Val < 0.0f)
    {
        // 후진 (브레이크를 밟는 로직)
        VehicleMovement->SetThrottleInput(0.0f);
        VehicleMovement->SetBrakeInput(-Val); // -(-1.0) = 1.0
    }
    else
    {
        // 입력 없음 (관성 주행)
        VehicleMovement->SetThrottleInput(0.0f);
        VehicleMovement->SetBrakeInput(0.0f);
    }
}

void AVehicle::MoveRight(float Val)
{
    if (!VehicleMovement) return;

    // A(-1.0) ~ D(1.0)
    VehicleMovement->SetSteeringInput(Val);
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