#pragma once
#include "PxPhysicsAPI.h"
#include "Vector.h"
#include <foundation/PxSimpleTypes.h>
#include "FVehicleData.generated.h"
using namespace physx;

// 최종 차량 데이터 에셋
USTRUCT(DisplayName = "FVehicleDataStruct", Description = "FVehicleDataStruct")
struct FVehicleData
{
	GENERATED_REFLECTION_BODY()

public:
	// 차체(Chassis)[
	UPROPERTY(EditAnywhere, Category = "VehicleData")
	float ChassisMass = 1500.0f;
	UPROPERTY(EditAnywhere, Category = "VehicleData")
	FVector ChassisDims = { 1.8f, 0.8f, 4.23f }; // 일반적인 승용차의 너비, 높이, 길이 (미터)
	UPROPERTY(EditAnywhere, Category = "VehicleData")
	FVector CenterOfMassOffset = { 0.0f, -0.25f, 0.0f }; // 바닥 기준이 아닌 중심 기준 오프셋

	int NumWheels = 4;

	UPROPERTY(EditAnywhere, Category = "VehicleData")
	float EnginePeakTorque = 1000.0f;    // 최대 토크 (Nm)
	UPROPERTY(EditAnywhere, Category = "VehicleData")
	float EngineMaxRPM = 12000.0f;       // 최대 회전수
	// 토크 커브 (0~1 정규화된 RPM에 대한 토크 비율)
	// 실제로는 CurveFloat 등을 사용하겠지만, 여기선 단순화

	UPROPERTY(EditAnywhere, Category = "VehicleData")
	float WheelRadius = 0.3f;          // 바퀴 반지름
	UPROPERTY(EditAnywhere, Category = "VehicleData")
	float WheelWidth = 0.1f;           // 바퀴 폭
	UPROPERTY(EditAnywhere, Category = "VehicleData")
	float WheelMass = 20.0f;           // 바퀴 무게
	UPROPERTY(EditAnywhere, Category = "VehicleData")
	float WheelDampingRate = 0.25f;    // 바퀴 회전 감쇠(저항)
	UPROPERTY(EditAnywhere, Category = "VehicleData")
	float WheelMaxBrakeTorque = 1500.0f; // 브레이크 힘
	UPROPERTY(EditAnywhere, Category = "VehicleData")
	float WheelMaxHandBrakeTorque = 4000.0f; // 핸드브레이크 힘

	// 서스펜션 핵심
	UPROPERTY(EditAnywhere, Category = "VehicleData")
	float SuspensionMaxCompression = 0.3f;  // 최대 압축 (m)
	UPROPERTY(EditAnywhere, Category = "VehicleData")
	float SuspensionMaxDroop = 0.1f;        // 최대 늘어짐 (m)
	UPROPERTY(EditAnywhere, Category = "VehicleData")
	float SuspensionSpringStrength = 35000.0f; // 스프링 강도
	UPROPERTY(EditAnywhere, Category = "VehicleData")
	float SuspensionSpringDamper = 4500.0f;    // 댐퍼 강도

	UPROPERTY(EditAnywhere, Category = "VehicleData")
	float GearSwitchTime = 0.5f;      // 기어 변경 시간 (초)
	UPROPERTY(EditAnywhere, Category = "VehicleData")
	float GearFinalRatio = 4.0f;      // 최종 감속비
	// 기어별 비율 (후진, 중립, 1단, 2단, ...)
	TArray<float> Gears = { -4.0f, 0.0f, 4.0f, 2.0f, 1.5f, 1.1f, 1.0f };

	UPROPERTY(EditAnywhere, Category = "VehicleData")
	float ClutchStrength = 10.0f;

	// NOTE: 추후 캐싱 처리
	PxVec3 GetChassisMOI()
	{
		//Now compute the chassis mass and moment of inertia.
		//Use the moment of inertia of a cuboid as an approximate value for the chassis moi.
		PxVec3 chassisMOI
		((ChassisDims.Y * ChassisDims.Y + ChassisDims.Z * ChassisDims.Z) * ChassisMass / 12.0f,
			(ChassisDims.X * ChassisDims.X + ChassisDims.Z * ChassisDims.Z) * ChassisMass / 12.0f,
			(ChassisDims.X * ChassisDims.X + ChassisDims.Y * ChassisDims.Y) * ChassisMass / 12.0f);
		//A bit of tweaking here.  The car will have more responsive turning if we reduce the 	
		//y-component of the chassis moment of inertia.
		chassisMOI.y *= 0.8f;

		return chassisMOI;
	}

	// NOTE: 추후 캐싱 처리
	PxF32 GetWheelMOI()
	{
		return 0.5f * WheelMass * WheelRadius * WheelRadius;
	}

	float GetEngineMaxOmega()
	{
		return (TWO_PI * EngineMaxRPM) / 60.0f;
	}
};
