#pragma once
#include "PhysicsSystem.h"
#include "BodySetupCore.h"

// 전방 선언
class UPrimitiveComponent;
class UPhysicalMaterial;
class UBodySetup;

// NOTE: FBodyInstance는 PhysX 리소스를 직접 관리하며 복사/이동이 금지된 런타임 객체임
// USTRUCT 리플렉션을 적용하지 않는다. (TArray 조작 불가)

struct FBodyInstance
{
public:
    // --- 생성자/소멸자 ---
    FBodyInstance();
    ~FBodyInstance();

    // 복사/이동 방지 (PhysX 리소스 관리)
    // FBodyInstance(const FBodyInstance&) = delete;
    // FBodyInstance& operator=(const FBodyInstance&) = delete;

    // 다른 바디 인스턴스의 설정값만 복사 (PhysX 리소스는 건드리지 않음)
    FBodyInstance DuplicateBodyInstance() const;
    
// --- 초기화/해제 ---
public:
    void InitBody(UBodySetup* Setup, const FTransform& Transform, UPrimitiveComponent* PrimComp, class FPhysicsScene* InRBScene);
    void TermBody();

private:
    physx::PxRigidDynamic* GetDynamicActor() const;
    physx::PxRigidDynamic* CreateInternalActor(const FTransform& Transform);
    void FinalizeInternalActor(FPhysicsScene* InRBScene);

public:
    bool IsValidBodyInstance() const { return RigidActor != nullptr; }
    bool IsDynamic() const { return RigidActor && RigidActor->is<PxRigidDynamic>() != nullptr; }

// --- 소유자 정보 ---
    UPrimitiveComponent* OwnerComponent = nullptr;
    FName BoneName = "None"; // 랙돌용 뼈 이름
    int32 BoneIndex = -1;
// --- PhysX 객체 ---
    physx::PxRigidActor* RigidActor = nullptr;
    TArray<physx::PxShape*> Shapes; // 다중 Shape 지원
// --- 물리 재질 ---
    UPhysicalMaterial* PhysMaterialOverride = nullptr;
// --- 스케일 ---
    FVector Scale3D = FVector::One();

    
// --- Transform ---
public:
    FTransform GetWorldTransform() const;
    void SetWorldTransform(const FTransform& NewTransform, bool bTeleport = false);
    FVector GetWorldLocation() const;
    FQuat GetWorldRotation() const;

// --- Simulate Physics ---
public:
    bool IsSimulatePhysics() const { return bSimulatePhysics; }
    void SetSimulatePhysics(bool bInSimulatePhysics);
private:
    bool bSimulatePhysics = true;  // true=Dynamic, false=Static/Kinematic
    
// --- Gravity ---
public:
    bool IsEnabledGravity() const { return bEnableGravity; }
    void SetEnableGravity(bool bInEnableGravity);
private:
    bool bEnableGravity = true;

// --- Start Awake ---
public:
    bool IsStartAwake() const { return bStartAwake; }
    void SetStartAwake(bool bInStartAwake);
private:
    bool bStartAwake = true;        // 시작 시 활성 상태
    
// --- Mass ---
public:
    bool IsOverrideMass() const { return bOverrideMass; }
    void SetOverrideMass(bool bInOverrideMass);
private:
    bool bOverrideMass = false;

public:
    float GetMassInKg() const { return MassInKg; }
    void SetMassInKg(float InMassInKg);
private:
    float MassInKg = 10.0f;

public:
    float GetBodyMass() const;
    FVector GetBodyInertiaTensor() const;
private:
    void UpdateMassProperties();

// --- Damping ---
public:
    float GetLinearDamping() const { return LinearDamping; }
    void SetLinearDamping(float InLinearDamping);
private:
    float LinearDamping = 0.01f;

public:
    float GetAngularDamping() const { return AngularDamping; }
    void SetAngularDamping(float InAngularDamping);
private:
    float AngularDamping = 0.05f;

// --- Collision ---
public:
    bool GetCollisionEnabled() const { return bCollisionEnabled; }
    void SetCollisionEnabled(bool bEnabled);
private:
    bool bCollisionEnabled = true;
    
public:
    bool IsTrigger() const { return bIsTrigger; }
    void SetTrigger(bool bInIsTrigger);
private:
    bool bIsTrigger = false;

public:
    bool GetNotifyRigidBodyCollision() const { return bNotifyRigidBodyCollision; }
    void SetNotifyRigidBodyCollision(bool bInNotifyRigidBodyCollision);
private:
    bool bNotifyRigidBodyCollision = true;  // 충돌 이벤트 발생 여부

public:
    bool GetUseCCD() const { return bUseCCD; }
    void SetUseCCD(bool bInUseCCD);
private:
    bool bUseCCD = false;                   // Continuous Collision Detection
    
// --- DOF Lock ---
public:
    void UpdateDOFLock();
    void GetLinearLock(bool& bX, bool& bY, bool& bZ) const { bX = bLockXLinear; bY = bLockYLinear; bZ = bLockZLinear; }
    void SetLinearLock(bool bX, bool bY, bool bZ);
    void GetAngularLock(bool& bX, bool& bY, bool& bZ) const { bX = bLockXAngular; bY = bLockYAngular; bZ = bLockZAngular; }
    void SetAngularLock(bool bX, bool bY, bool bZ);
private:
    bool bLockXLinear = false;
    bool bLockYLinear = false;
    bool bLockZLinear = false;
    bool bLockXAngular = false;
    bool bLockYAngular = false;
    bool bLockZAngular = false;

// --- 슬립 설정 ---
public:
    float GetSleepThresholdMultiplier() const { return SleepThresholdMultiplier; }
    void SetSleepThresholdMultiplier(float InSleepThresholdMultiplier);
private:
    float SleepThresholdMultiplier = 1.0f;

public:
    bool IsAwake() const;
    void WakeUp();
    void PutToSleep();

// --- 힘/토크/속도 ---
public:
    void AddForce(const FVector& Force, bool bAccelChange = false);
    void AddForceAtLocation(const FVector& Force, const FVector& Location);
    void AddTorque(const FVector& Torque, bool bAccelChange = false);
    void AddImpulse(const FVector& Impulse, bool bVelChange = false);
    void AddImpulseAtLocation(const FVector& Impulse, const FVector& Location);
    void AddAngularImpulse(const FVector& AngularImpulse, bool bVelChange = false);

    FVector GetLinearVelocity() const;
    void SetLinearVelocity(const FVector& Velocity, bool bAddToCurrent = false);
    FVector GetAngularVelocity() const;
    void SetAngularVelocity(const FVector& AngularVelocity, bool bAddToCurrent = false);

// --- UBodySetupCore 설정 적용 ---
public:
    void ApplyBodySetupSettings(const UBodySetupCore* BodySetupCore);
    void ApplyPhysicsType(EPhysicsType PhysicsType);
    void ApplyCollisionResponse(EBodyCollisionResponse::Type Response);
};
