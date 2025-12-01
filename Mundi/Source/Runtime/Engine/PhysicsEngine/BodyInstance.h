#pragma once
#include "BodySetup.h"

#include "FBodyInstance.generated.h"

USTRUCT()
struct FBodyInstance
{
    GENERATED_REFLECTION_BODY()

    FBodyInstance();
    ~FBodyInstance();

    // 복사 시 런타임 포인터는 복사하지 않음 (PhysX 리소스 공유 방지)
    FBodyInstance(const FBodyInstance& Other);
    FBodyInstance& operator=(const FBodyInstance& Other);

    /**
     * 단일 강체를 초기화한다.
     * @param Setup         모양 정보 (UBodySetup)
     * @param Transform     월드 트랜스폼
     * @param Component     바디를 소유한 컴포넌트
     * @param InRBScene     바디가 속할 물리 씬
     * @param InAggregate   Aggregate (nullptr이면 Scene에 직접 추가, 아니면 Aggregate에 추가)
     */
    void InitBody(UBodySetup* Setup, const FTransform& Transform, UPrimitiveComponent* Component, FPhysScene* InRBScene, physx::PxAggregate* InAggregate = nullptr);

    /** 바디를 씬에서 제거하고 자원을 해제한다. */
    void TermBody();

    /** 물리 바디로부터 현재 월드 공간 트랜스폼을 가져온다. */
    FTransform GetUnrealWorldTransform() const;

    /** 바디 인스턴스에서 사용되는 물리 머티리얼을 가져온다. */
    UPhysicalMaterial* GetSimplePhysicalMaterial() const;

    /**
     * 물리 바디의 위치를 강제로 설정 (텔레포트)
     * @note 현재는 커맨드 큐가 없기 때문에 SCOPED_SCENE_WRITE_LOCK을 통해서 비동기 연산을 처리한다.
     * @todo 시뮬레이션 중인지 확인한 후에, 시뮬레이션 중에만 락을 걸도록 변경한다.
     *       bTeleport대신에 enum을 사용하도록 변경한다.
     * @param NewTransform      이동할 목표 위치
     * @param bTeleport         true면 속도를 초기화하고 순간이동, false면 kinematic 이동
     */
    void SetBodyTransform(const FTransform& NewTransform, bool bTeleport);

    /**
     * 바디의 선형 속도를 설정한다.
     * @todo 시뮬레이션 중인지 확인한 후에, 시뮬레이션 중에만 락을 걸도록 변경한다.
     */
    void SetLinearVelocity(const FVector& NewVel, bool bAddToCurrent = false);

    /**
     * 바디에 힘을 더한다.
     * @todo 시뮬레이션 중인지 확인한 후에, 시뮬레이션 중에만 락을 걸도록 변경한다.
     */
    void AddForce(const FVector& Force, bool bAccelChange = false);

    /**
     * 바디에 토크를 더한다.
     * @todo 시뮬레이션 중인지 확인한 후에, 시뮬레이션 중에만 락을 걸도록 변경한다.
     */
    void AddTorque(const FVector& Torque, bool bAccelChange = false);

    /** 동적 바디여부 확인*/
    bool IsDynamic() const;
    
    /** 유효성 검사 */
    bool IsValidBodyInstance() const { return RigidActor != nullptr;}

public:
    /** 바디를 소유하고 있는 컴포넌트 */
    UPrimitiveComponent* OwnerComponent;

    /** 충돌체 정보를 가지고있는 바디 설정 */
    UBodySetup* BodySetup;

    /** 바디의 물리 머티리얼 대신에 사용할 머티리얼 */
    UPhysicalMaterial* PhysicalMaterialOverride;

    /** 바디를 소유하고 있는 물리 씬 */
    FPhysScene* PhysScene;

    /** PhysX Actor */
    PxRigidActor* RigidActor;

    /** 바디를 생성하기 위한 3D 스케일 */
    FVector Scale3D;

    /** True일 경우, 물리 시뮬레이션을 수행한다 (False일 경우 kinematic/static). */
    bool bSimulatePhysics;

    /** 선형 댐핑 (이동에 대한 저항력) */
    float LinearDamping;

    /** 각형 댐핑 (회전에 대한 저항력) */
    float AngularDamping;

    /** 질량 오버라이드 */
    float MassInKgOverride;

    /** True일 경우, MassInKgOverride를 사용 */
    bool bOverrideMass;

    /** 이 바디가 속한 Aggregate (nullptr이면 Scene에 직접 추가됨) */
    physx::PxAggregate* OwningAggregate = nullptr;

    /** True일 경우, 랙돌 바디 (SyncComponentsToBodies에서 제외됨) */
    bool bIsRagdollBody = false;
};
