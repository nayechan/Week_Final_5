#pragma once
#include "Pawn.h"
#include "VehicleMovementComponent.h"
#include "StaticMeshComponent.h" // 스태틱 메쉬 헤더 (가정)
#include "CameraComponent.h"
#include "SpringArmComponent.h"
#include "AVehicle.generated.h"

/**
 * PhysX 기반의 4륜 구동 자동차 액터
 * 구조: StaticMesh (차체) + 4x StaticMesh (바퀴)
 */
UCLASS()
class AVehicle : public APawn
{
    GENERATED_REFLECTION_BODY()

public:
    AVehicle();
    virtual ~AVehicle() override;

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;

    virtual void SetupPlayerInputComponent(UInputComponent* InInputComponent) override;

    // ====================================================================
    // 컴포넌트 구성
    // ====================================================================
    
    /** 차체 메쉬 (물리 시뮬레이션의 주체, Root) */
    UStaticMeshComponent* ChassisMesh;

    /** 바퀴 메쉬 배열 (시각적 표현용, 0:FL, 1:FR, 2:RL, 3:RR) */
    TArray<UStaticMeshComponent*> WheelMeshes;

    /** 차량 물리 이동 로직 담당 */
    UPROPERTY(EditAnywhere, Category = "Vehicle")
    UVehicleMovementComponent* VehicleMovement;

    /** 카메라 붐 */
    UPROPERTY(EditAnywhere, Category = "Camera")
    USpringArmComponent* SpringArm;

    /** 메인 카메라 */
    UPROPERTY(EditAnywhere, Category = "Camera")
    UCameraComponent* Camera;

protected:
    // ====================================================================
    // 입력 핸들러
    // ====================================================================
    // W, S 키 처리
    void MoveForward(float Val); 
    // A, D 키 처리
    void MoveRight(float Val);   
    // SpaceBar 처리
    void HandbrakePressed();
    void HandbrakeReleased();
    // Shift 처리 (부스터)
    void BoostPressed();
    void BoostReleased();
    
    // ====================================================================
    // 내부 로직
    // ====================================================================
    /** 매 프레임 PhysX 시뮬레이션 결과에 맞춰 바퀴 메쉬를 회전/이동시킴 */
    void SyncWheelVisuals();

    float CurrentForwardInput;
    float CurrentSteeringInput;
    bool bIsBoosting;

    /** 부스터 힘 (가속도 단위) */
    float BoostStrength = 30.0f;
};