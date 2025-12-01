#pragma once
#include "Object.h"
#include "UBodySetupCore.generated.h"

/**
 * EPhysicsType
 *
 * 물리 시뮬레이션 타입
 */
UENUM()
enum class EPhysicsType : uint8
{
    // 기본값 - OwnerComponent의 설정을 상속
    Default,
    // 물리 시뮬레이션 적용
    Simulated,
    // 키네마틱 - 물리 영향 안받지만 다른 물체와 상호작용
    Kinematic
};

/**
 * EBodyCollisionResponse
 *
 * 바디의 충돌 응답 타입
 */
UENUM()
namespace EBodyCollisionResponse
{
    enum Type : uint8
    {
        // 충돌 활성화
        BodyCollision_Enabled,
        // 충돌 비활성화
        BodyCollision_Disabled
    };
}

/**
 * UBodySetupCore
 *
 * UBodySetup의 기본 클래스.
 * 본 이름과 기본 물리/충돌 설정을 정의.
 */
UCLASS(DisplayName="바디 셋업 코어", Description="물리 바디의 기본 설정")
class UBodySetupCore : public UObject
{
    GENERATED_REFLECTION_BODY()

public:
    // --- 본 정보 ---

    // PhysicsAsset에서 사용. 스켈레탈 메시의 본과 연결.
    UPROPERTY(EditAnywhere, Category="BodySetup")
    FName BoneName = "None";

    // --- 물리 타입 ---

    // 시뮬레이션 타입 (Simulated, Kinematic, Default)
    UPROPERTY(EditAnywhere, Category="Physics")
    EPhysicsType PhysicsType = EPhysicsType::Default;

    // --- 충돌 설정 ---
    // 충돌 응답 타입
    UPROPERTY(EditAnywhere, Category="Collision")
    EBodyCollisionResponse::Type CollisionResponse = EBodyCollisionResponse::BodyCollision_Enabled;

    // --- 생성자/소멸자 ---
    UBodySetupCore();
    virtual ~UBodySetupCore();

    // --- 직렬화 ---
    virtual void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;
};
