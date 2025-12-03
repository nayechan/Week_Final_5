#pragma once
#include "PhysXPublic.h"
#include "UVehicleWheel.generated.h"

/**
 * 휠의 반경, 질량, 서스펜션 강도 등을 정의하는 데이터 클래스
 */
UCLASS()
class UVehicleWheel : public UObject
{
public:
    GENERATED_REFLECTION_BODY()

    UVehicleWheel();
    virtual ~UVehicleWheel() {}

    // ====================================================================
    // Wheel Properties (PxVehicleWheelData 대응)
    // ====================================================================

    /** 바퀴 반경 (m 단위) */
    UPROPERTY(EditAnywhere, Category = "Wheel")
    float WheelRadius = 0.35f;

    /** 바퀴 폭 (m) */
    UPROPERTY(EditAnywhere, Category = "Wheel")
    float WheelWidth = 0.2f;

    /** 바퀴 질량 (kg) */
    UPROPERTY(EditAnywhere, Category = "Wheel")
    float Mass = 20.0f;

    /** 댐핑 비율 (회전 저항) */
    UPROPERTY(EditAnywhere, Category = "Wheel")
    float DampingRate = 0.25f;

    /** 핸들 조향 시 최대 각도 (도) - 앞바퀴는 보통 40~45도, 뒷바퀴는 0도 */
    UPROPERTY(EditAnywhere, Category = "Wheel")
    float SteerAngle = 0.0f;

    /** 핸드브레이크 사용 시 영향을 받는지 여부 */
    UPROPERTY(EditAnywhere, Category = "Wheel")
    bool bAffectedByHandbrake = true;

    // ====================================================================
    // Suspension Properties (PxVehicleSuspensionData 대응)
    // ====================================================================

    /** 서스펜션 스프링 강도 (값이 클수록 딱딱함) */
    UPROPERTY(EditAnywhere, Category = "Suspension")
    float SprungMass = 0.0f; 

    /** 서스펜션 댐퍼 강도 (값이 클수록 출렁거림이 적음) */
    UPROPERTY(EditAnywhere, Category = "Suspension")
    float SuspensionDamping = 1000.0f;

    /** 서스펜션 스프링 강성 */
    UPROPERTY(EditAnywhere, Category = "Suspension")
    float SuspensionStiffness = 50000.0f;

    /** 서스펜션 최대 압축 거리 (m) - 바퀴가 위로 얼마나 올라갈 수 있는지 */
    UPROPERTY(EditAnywhere, Category = "Suspension")
    float MaxSuspensionCompression = 0.1f;

    /** 서스펜션 최대 이완 거리 (m) - 바퀴가 아래로 얼마나 내려갈 수 있는지 */
    UPROPERTY(EditAnywhere, Category = "Suspension")
    float MaxSuspensionDroop = 0.1f;

    // ====================================================================
    // PhysX 변환 함수
    // ====================================================================

    /** PhysX WheelData 구조체로 변환하여 반환 */
    PxVehicleWheelData GetPxWheelData() const
    {
        PxVehicleWheelData Data;
        Data.mRadius             = WheelRadius;
        Data.mWidth              = WheelWidth;
        Data.mMass               = Mass;
        Data.mMOI                = 0.5f * Mass * Data.mRadius * Data.mRadius; // 관성 모멘트(I = 0.5 * m * r^2)
        Data.mMaxHandBrakeTorque = bAffectedByHandbrake ? 3000.0f : 0.0f;
        Data.mMaxSteer           = DegreesToRadians(SteerAngle);
        return Data;
    }

    /** PhysX SuspensionData 구조체로 변환하여 반환 */
    PxVehicleSuspensionData GetPxSuspensionData() const
    {
        PxVehicleSuspensionData Data;
        Data.mMaxCompression   = MaxSuspensionCompression;
        Data.mMaxDroop         = MaxSuspensionDroop;
        Data.mSpringStrength   = SuspensionStiffness;
        Data.mSpringDamperRate = SuspensionDamping;
        Data.mSprungMass       = (SprungMass > 0.0f) ? SprungMass : 1500.0f / 4.0f;
        return Data;
    }
};