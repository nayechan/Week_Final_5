#pragma once

struct FVehicleEngineData
{
    /** 최대 RPM */
    float MaxRPM = 6000.0f;
    /** 최대 토크 (Nm) */
    float MaxTorque = 600.0f;
    /** 엔진 관성 (kg*m^2) */
    float MOI = 1.0f;
    /** 감속 계수 (악셀 뗐을 때) */
    float DampingRateFullThrottle = 0.15f;
    float DampingRateZeroThrottleClutchEngaged = 2.0f;
    float DampingRateZeroThrottleClutchDisengaged = 0.35f;

    // PhysX 데이터 변환
    PxVehicleEngineData GetPxEngineData() const
    {
        PxVehicleEngineData Data;
        Data.mPeakTorque                              = MaxTorque;
        Data.mMaxOmega                                = MaxRPM * (PxPi / 30.0f); // RPM -> rad/s 변환 (RPM * Pi / 30)
        Data.mMOI                                     = MOI;
        Data.mDampingRateFullThrottle                 = DampingRateFullThrottle;
        Data.mDampingRateZeroThrottleClutchEngaged    = DampingRateZeroThrottleClutchEngaged;
        Data.mDampingRateZeroThrottleClutchDisengaged = DampingRateZeroThrottleClutchDisengaged;
        
        // 토크 커브 (하드 코딩)
        Data.mTorqueCurve.addPair(0.0f, 0.8f);   
        Data.mTorqueCurve.addPair(0.33f, 1.0f);  
        Data.mTorqueCurve.addPair(1.0f, 0.8f);   
        
        return Data;
    }
};

struct FVehicleTransmissionData
{
    float FinalRatio       = 4.0f;
    float ReverseGearRatio = 4.0f;
    float GearSwitchTime   = 0.1f;  // 변속 시간 (짧아야 자동변속이 잘 됨)

    // 기어비 예시 (일반적인 승용차 비율)
    float FirstGearRatio  = 2.66f;
    float SecondGearRatio = 1.78f;
    float ThirdGearRatio  = 1.30f;
    float FourthGearRatio = 1.00f;
    float FifthGearRatio  = 0.74f;

    physx::PxVehicleGearsData GetPxGearsData() const
    {
        physx::PxVehicleGearsData Data;
        Data.mSwitchTime                                  = GearSwitchTime;
        Data.mFinalRatio                                  = FinalRatio;
        Data.mNbRatios                                    = 7; // Reverse + Neutral + 5 forward gears
        Data.mRatios[physx::PxVehicleGearsData::eREVERSE] = ReverseGearRatio;
        Data.mRatios[physx::PxVehicleGearsData::eNEUTRAL] = 0.0f;
        Data.mRatios[physx::PxVehicleGearsData::eFIRST]   = FirstGearRatio;
        Data.mRatios[physx::PxVehicleGearsData::eSECOND]  = SecondGearRatio;
        Data.mRatios[physx::PxVehicleGearsData::eTHIRD]   = ThirdGearRatio;
        Data.mRatios[physx::PxVehicleGearsData::eFOURTH]  = FourthGearRatio;
        Data.mRatios[physx::PxVehicleGearsData::eFIFTH]   = FifthGearRatio;
        return Data;
    }
};