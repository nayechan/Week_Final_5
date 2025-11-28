#pragma once
#include <PxPhysicsAPI.h>

class UPhysicalMaterial : public UResourceBase
{
public:
    // 실제 PhysX 객체
    physx::PxMaterial* MatHandle = nullptr;

    // --- 설정값 ---
    float StaticFriction = 0.5f;   // 정지 마찰력 (안 미끄러지려는 힘)
    float DynamicFriction = 0.5f;  // 운동 마찰력 (미끄러질 때 받는 힘)
    float Restitution = 0.5f;      // 반발 계수 (클수록 많이 튐)
    float Density = 1000.0f; // 밀도 (kg/m^3)

    // 마찰/반발력 합산 방식 (Average, Max, Multiply 등)
    // 기본값: 마찰은 평균(Average), 반발은 평균(Average)
    
    UPhysicalMaterial() = default;
    ~UPhysicalMaterial() override { Release(); }

    // 생성 함수
    void CreateMaterial();
    
    // 해제 함수
    void Release();
};
