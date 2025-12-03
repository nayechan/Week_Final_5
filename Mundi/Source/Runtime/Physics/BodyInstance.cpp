#include "pch.h"
#include "BodyInstance.h"
#include "PhysicalMaterial.h"
#include "PhysXConversion.h"
#include "BodySetup.h"
#include "PhysicsScene.h"
#include "PrimitiveComponent.h"
using namespace physx;

FBodyInstance::FBodyInstance() = default;
FBodyInstance::~FBodyInstance()
{
    TermBody();
}

FBodyInstance FBodyInstance::DuplicateBodyInstance() const
{
    // --- 1. 기본 식별 및 재질 ---
    FBodyInstance Other;
    Other.BoneName = BoneName;
    Other.PhysMaterialOverride = PhysMaterialOverride;
    Other.Scale3D = Scale3D;

    // --- 2. 시뮬레이션 설정 ---
    Other.bSimulatePhysics = bSimulatePhysics;
    Other.bEnableGravity = bEnableGravity;
    Other.bStartAwake = bStartAwake;

    // --- 3. 질량 설정 ---
    Other.bOverrideMass = bOverrideMass;
    Other.MassInKg = MassInKg;

    // --- 4. 감쇠(Damping) ---
    Other.LinearDamping = LinearDamping;
    Other.AngularDamping = AngularDamping;

    // --- 5. 충돌 설정 ---
    Other.bCollisionEnabled = bCollisionEnabled;
    Other.bIsTrigger = bIsTrigger;
    Other.bNotifyRigidBodyCollision = bNotifyRigidBodyCollision;
    Other.bUseCCD = bUseCCD;

    // --- 6. 자유도 잠금(DOF Lock) ---
    Other.bLockXLinear = bLockXLinear;
    Other.bLockYLinear = bLockYLinear;
    Other.bLockZLinear = bLockZLinear;
    
    Other.bLockXAngular = bLockXAngular;
    Other.bLockYAngular = bLockYAngular;
    Other.bLockZAngular = bLockZAngular;
    
    // --- 7. 기타 설정 ---
    Other.SleepThresholdMultiplier = SleepThresholdMultiplier;
    return Other;
}

void FBodyInstance::InitBody(UBodySetup* Setup, const FTransform& Transform, UPrimitiveComponent* PrimComp, FPhysicsScene* InRBScene, ECollisionShapeMode ShapeMode)
{
    if (!Setup || !Setup->HasValidShapes())
    {
        UE_LOG("[FBodyInstance] InitBody failed: Invalid BodySetup");
        return;
    }

    OwnerComponent = PrimComp;
    CurrentScene = InRBScene;
    Scale3D = Transform.Scale3D;
    
    RigidActor = CreateInternalActor(Transform);
    if (!RigidActor) return;

    UPhysicalMaterial* MatToUse = PhysMaterialOverride ? PhysMaterialOverride : Setup->PhysMaterial;
    Setup->CreatePhysicsShapes(this, Scale3D, MatToUse, ShapeMode);
    ApplyBodySetupSettings(Setup);
    FinalizeInternalActor(InRBScene);
}

void FBodyInstance::TermBody()
{
    if (RigidActor)
    {
        PxScene* PScene = RigidActor->getScene();
        if (PScene)
        {
            PScene->removeActor(*RigidActor);
        }
        RigidActor->release();
        RigidActor = nullptr;
    }
    Shapes.Empty();
}

PxRigidDynamic* FBodyInstance::GetDynamicActor() const
{
    if (RigidActor)
    {
        return RigidActor->is<PxRigidDynamic>();
    }
    return nullptr;
}

physx::PxRigidDynamic* FBodyInstance::CreateInternalActor(const FTransform& Transform)
{
    TermBody();

    FPhysicsSystem* System = GEngine.GetPhysicsSystem();
    PxPhysics* Physics = System->GetPhysics();
    
    PxTransform PTransform = PhysXConvert::ToPx(Transform); // 좌표 변환
    PxRigidDynamic* NewActor = Physics->createRigidDynamic(PTransform); // 일단 무조건 Movable 가정

    if (NewActor)
    {
        NewActor->setRigidBodyFlag(PxRigidBodyFlag::eKINEMATIC, !bSimulatePhysics);
        NewActor->setLinearDamping(LinearDamping);
        NewActor->setAngularDamping(AngularDamping);
        NewActor->setActorFlag(PxActorFlag::eDISABLE_GRAVITY, !bEnableGravity);

        if (bUseCCD && bSimulatePhysics)
        {
            NewActor->setRigidBodyFlag(PxRigidBodyFlag::eENABLE_CCD, true);
        }

        if (bSimulatePhysics && !bStartAwake)
        {
            NewActor->putToSleep();
        }
    }

    return NewActor;
}

void FBodyInstance::FinalizeInternalActor(FPhysicsScene* InRBScene)
{
    if (!RigidActor) { return; }

    UpdateMassProperties();
    UpdateDOFLock();
    InRBScene->AddActor(this);
}


FTransform FBodyInstance::GetWorldTransform() const
{
    if (RigidActor)
    {
        physx::PxTransform PxT = RigidActor->getGlobalPose();
        FTransform WorldTransform = PhysXConvert::FromPx(PxT); 
        WorldTransform.Scale3D = Scale3D; 
        
        return WorldTransform;
    }
    return {};
}

void FBodyInstance::SetWorldTransform(const FTransform& NewTransform, bool bTeleport)
{
    FVector NewScale = NewTransform.Scale3D;
    
    if (!Scale3D.Equals(NewScale)) 
    {
        Scale3D = NewScale;
        if (OwnerComponent)
        {
            // (내부적으로 TermBody() -> InitBody() 호출)
            OwnerComponent->RecreatePhysicsState();
        }
        
        return; 
    }

    if (!CurrentScene || !RigidActor) return;
    CurrentScene->EnqueueCommand([this, NewTransform, bTeleport]()
    {
        // 람다 내부에서 Actor 유효성 재확인 (큐 처리될 때 삭제됐을 수도 있음)
        if (!RigidActor) return;

        PxTransform PTransform = PhysXConvert::ToPx(NewTransform);

        if (PxRigidDynamic* DynamicActor = GetDynamicActor())
        {
            bool bIsKinematic = DynamicActor->getRigidBodyFlags().isSet(PxRigidBodyFlag::eKINEMATIC);
            if (bIsKinematic && !bTeleport)
            {
                DynamicActor->setKinematicTarget(PTransform);
            }
            else
            {
                DynamicActor->setGlobalPose(PTransform);
            }
        }
        else
        {
            RigidActor->setGlobalPose(PTransform);
        }
    });
}

FVector FBodyInstance::GetWorldLocation() const
{
    if (RigidActor)
    {
        return PhysXConvert::FromPx(RigidActor->getGlobalPose().p);
    }
    return FVector::Zero();
}

FQuat FBodyInstance::GetWorldRotation() const
{
    if (RigidActor)
    {
        return PhysXConvert::FromPx(RigidActor->getGlobalPose().q);
    }
    return FQuat::Identity();
}

void FBodyInstance::SetSimulatePhysics(bool bInSimulatePhysics)
{
    if (bSimulatePhysics == bInSimulatePhysics) { return; }
    bSimulatePhysics = bInSimulatePhysics;

    if (!CurrentScene || !RigidActor) { return; }

    CurrentScene->EnqueueCommand([this, bInSimulatePhysics]()
    {
        if (PxRigidDynamic* DynamicActor = GetDynamicActor())
        {
            if (bInSimulatePhysics)
            {
                DynamicActor->setRigidBodyFlag(PxRigidBodyFlag::eKINEMATIC, false);
                DynamicActor->wakeUp();
            }
            else
            {
                DynamicActor->setLinearVelocity(PxVec3(0));
                DynamicActor->setAngularVelocity(PxVec3(0));
                DynamicActor->setRigidBodyFlag(PxRigidBodyFlag::eKINEMATIC, true);
            }
        }
    });
}

void FBodyInstance::SetEnableGravity(bool bInEnableGravity)
{
    bEnableGravity = bInEnableGravity;

    if (!CurrentScene || !RigidActor) { return; }

    CurrentScene->EnqueueCommand([this, bInEnableGravity]()
    {
        if (RigidActor)
        {
            RigidActor->setActorFlag(PxActorFlag::eDISABLE_GRAVITY, !bInEnableGravity);
        }
    });
}

void FBodyInstance::SetStartAwake(bool bInStartAwake)
{
    bStartAwake = bInStartAwake;
}

void FBodyInstance::SetOverrideMass(bool bInOverrideMass)
{
    if (bOverrideMass == bInOverrideMass) { return; }
    bOverrideMass = bInOverrideMass;
    UpdateMassProperties();
}

void FBodyInstance::SetMassInKg(float InMassInKg)
{
    if (MassInKg == InMassInKg) { return; }
    MassInKg = InMassInKg;
    UpdateMassProperties();
}

float FBodyInstance::GetBodyMass() const
{
    if (PxRigidDynamic* DynamicActor = GetDynamicActor())
    {
        return DynamicActor->getMass();
    }
    return 0.0f;
}

FVector FBodyInstance::GetBodyInertiaTensor() const
{
    if (PxRigidDynamic* DynamicActor = GetDynamicActor())
    {
        // 관성 텐서도 좌표계 변환 필요
        return PhysXConvert::FromPx(DynamicActor->getMassSpaceInertiaTensor());
    }
    return FVector::Zero();
}

void FBodyInstance::UpdateMassProperties()
{
    if (!CurrentScene || !RigidActor) return;

    CurrentScene->EnqueueCommand([this]()
    {
        PxRigidDynamic* DynamicActor = GetDynamicActor();
        if (!DynamicActor) { return; }

        if (bOverrideMass)
        {
            PxRigidBodyExt::setMassAndUpdateInertia(*DynamicActor, MassInKg);
        }
        else
        {
            float Density = 1000.0f; 
            if (UPhysicalMaterial* UsedMat = PhysMaterialOverride)
            {
                Density = UsedMat->Density;
            }
            PxRigidBodyExt::updateMassAndInertia(*DynamicActor, Density);
        }
    });
}

void FBodyInstance::SetLinearDamping(float InLinearDamping)
{
    if (PxRigidDynamic* DynamicActor = GetDynamicActor())
    {
        LinearDamping = InLinearDamping;
        DynamicActor->setLinearDamping(InLinearDamping);
    }
}

void FBodyInstance::SetAngularDamping(float InAngularDamping)
{
    if (PxRigidDynamic* DynamicActor = GetDynamicActor())
    {
        AngularDamping = InAngularDamping;
        DynamicActor->setAngularDamping(InAngularDamping);
    }
}

void FBodyInstance::SetCollisionEnabled(bool bEnabled)
{
    if (bEnabled == bCollisionEnabled) { return; }
    bCollisionEnabled = bEnabled;

    if (!RigidActor || !CurrentScene) return;

    CurrentScene->EnqueueCommand([this]()
    {
        for (int32 i = 0; i < Shapes.Num(); ++i)
        {
            if (Shapes[i])
            {
                Shapes[i]->setFlag(PxShapeFlag::eSIMULATION_SHAPE, bCollisionEnabled && !bIsTrigger);
                Shapes[i]->setFlag(PxShapeFlag::eSCENE_QUERY_SHAPE, bCollisionEnabled);
            }
        }
    });
}

void FBodyInstance::SetTrigger(bool bInIsTrigger)
{
    if (bInIsTrigger == bIsTrigger) { return; }

    bIsTrigger = bInIsTrigger;
    for (int32 i = 0; i < Shapes.Num(); ++i)
    {
        if (Shapes[i])
        {
            Shapes[i]->setFlag(PxShapeFlag::eSIMULATION_SHAPE, bCollisionEnabled && !bIsTrigger);
            Shapes[i]->setFlag(PxShapeFlag::eSCENE_QUERY_SHAPE, bCollisionEnabled);
        }
    }
}

void FBodyInstance::SetNotifyRigidBodyCollision(bool bInNotifyRigidBodyCollision)
{
    if (bInNotifyRigidBodyCollision == bNotifyRigidBodyCollision) { return; }
    bNotifyRigidBodyCollision = bInNotifyRigidBodyCollision;
    // TODO - 충돌 이벤트 발생 안시키게
}

void FBodyInstance::SetUseCCD(bool bInUseCCD)
{
    if (bUseCCD == bInUseCCD) { return; }

    bUseCCD = bInUseCCD;
    if (PxRigidDynamic* DynamicActor = GetDynamicActor())
    {
        DynamicActor->setRigidBodyFlag(PxRigidBodyFlag::eENABLE_CCD, bUseCCD);
    }
}

void FBodyInstance::UpdateDOFLock()
{
    if (!CurrentScene || !RigidActor) return;

    CurrentScene->EnqueueCommand([this]()
    {
        PxRigidDynamic* DynamicActor = GetDynamicActor();
        if (!DynamicActor) return;

        PxRigidDynamicLockFlags LockFlags;
        if (bLockXLinear) LockFlags |= PxRigidDynamicLockFlag::eLOCK_LINEAR_Z;
        if (bLockXAngular) LockFlags |= PxRigidDynamicLockFlag::eLOCK_ANGULAR_Z;
        if (bLockYLinear) LockFlags |= PxRigidDynamicLockFlag::eLOCK_LINEAR_X;
        if (bLockYAngular) LockFlags |= PxRigidDynamicLockFlag::eLOCK_ANGULAR_X;
        if (bLockZLinear) LockFlags |= PxRigidDynamicLockFlag::eLOCK_LINEAR_Y;
        if (bLockZAngular) LockFlags |= PxRigidDynamicLockFlag::eLOCK_ANGULAR_Y;

        DynamicActor->setRigidDynamicLockFlags(LockFlags);
    });
}

void FBodyInstance::SetLinearLock(bool bX, bool bY, bool bZ)
{
    bLockXLinear = bX; bLockYLinear = bY; bLockZLinear = bZ;
    UpdateDOFLock();
}

void FBodyInstance::SetAngularLock(bool bX, bool bY, bool bZ)
{
    bLockXAngular = bX; bLockYAngular = bY; bLockZAngular = bZ;
    UpdateDOFLock();
}

void FBodyInstance::SetSleepThresholdMultiplier(float InSleepThresholdMultiplier)
{
    if (InSleepThresholdMultiplier == SleepThresholdMultiplier) { return; }
    SleepThresholdMultiplier = InSleepThresholdMultiplier;
    CurrentScene->EnqueueCommand([this]()
    {
        if (PxRigidDynamic* DynamicActor = GetDynamicActor())
        {
            constexpr float SleepThreshold = 0.005f;
            DynamicActor->setSleepThreshold(SleepThreshold * SleepThresholdMultiplier);
        }
    });
}

bool FBodyInstance::IsAwake() const
{
    if (PxRigidDynamic* DynamicActor = GetDynamicActor())
    {
        return !DynamicActor->isSleeping();
    }
    return false;
}

void FBodyInstance::WakeUp()
{
    CurrentScene->EnqueueCommand([this]()
    {
        if (PxRigidDynamic* DynamicActor = GetDynamicActor())
        {
            DynamicActor->wakeUp();
        }
    });
}

void FBodyInstance::PutToSleep()
{
    CurrentScene->EnqueueCommand([this]()
    {
        if (PxRigidDynamic* DynamicActor = GetDynamicActor())
        {
            DynamicActor->putToSleep();
        }
    });
}

void FBodyInstance::AddForce(const FVector& Force, bool bAccelChange)
{
    if (!CurrentScene || !RigidActor) return;

    CurrentScene->EnqueueCommand([this, Force, bAccelChange]()
    {
        if (PxRigidDynamic* DynamicActor = GetDynamicActor())
        {
            if (DynamicActor->getRigidBodyFlags() & PxRigidBodyFlag::eKINEMATIC) { return; }
            PxVec3 PForce = PhysXConvert::ToPx(Force);
            PxForceMode::Enum Mode = bAccelChange ? PxForceMode::eACCELERATION : PxForceMode::eFORCE;
            DynamicActor->addForce(PForce, Mode);
        }
    });
}

void FBodyInstance::AddForceAtLocation(const FVector& Force, const FVector& Location)
{
    if (!CurrentScene || !RigidActor) return;

    CurrentScene->EnqueueCommand([this, Force, Location]()
    {
        if (PxRigidDynamic* DynamicActor = GetDynamicActor())
        {
            if (DynamicActor->getRigidBodyFlags() & PxRigidBodyFlag::eKINEMATIC) { return; }
            PxVec3 PForce = PhysXConvert::ToPx(Force);
            PxVec3 PLocation = PhysXConvert::ToPx(Location);
            PxRigidBodyExt::addForceAtPos(*DynamicActor, PForce, PLocation);
        }
    });
}

void FBodyInstance::AddTorque(const FVector& Torque, bool bAccelChange)
{
    if (!CurrentScene || !RigidActor) return;

    CurrentScene->EnqueueCommand([this, Torque, bAccelChange]()
    {
        if (PxRigidDynamic* DynamicActor = GetDynamicActor())
        {
            if (DynamicActor->getRigidBodyFlags() & PxRigidBodyFlag::eKINEMATIC) { return; }
            PxVec3 PTorque = PhysXConvert::AngularToPx(Torque);
            PxForceMode::Enum Mode = bAccelChange ? PxForceMode::eACCELERATION : PxForceMode::eFORCE;
            DynamicActor->addTorque(PTorque, Mode);
        }
    });
}

void FBodyInstance::AddImpulse(const FVector& Impulse, bool bVelChange)
{
    if (!CurrentScene || !RigidActor) return;

    CurrentScene->EnqueueCommand([this, Impulse, bVelChange]()
    {
        if (PxRigidDynamic* DynamicActor = GetDynamicActor())
        {
            if (DynamicActor->getRigidBodyFlags() & PxRigidBodyFlag::eKINEMATIC) { return; }
            PxVec3 PImpulse = PhysXConvert::ToPx(Impulse);
            PxForceMode::Enum Mode = bVelChange ? PxForceMode::eVELOCITY_CHANGE : PxForceMode::eIMPULSE;
            DynamicActor->addForce(PImpulse, Mode);
        }
    });
}

void FBodyInstance::AddImpulseAtLocation(const FVector& Impulse, const FVector& Location)
{
    if (!CurrentScene || !RigidActor) return;

    CurrentScene->EnqueueCommand([this, Impulse, Location]()
    {
        if (PxRigidDynamic* DynamicActor = GetDynamicActor())
        {
            if (DynamicActor->getRigidBodyFlags() & PxRigidBodyFlag::eKINEMATIC) { return; }
            PxVec3 PImpulse = PhysXConvert::ToPx(Impulse);
            PxVec3 PLocation = PhysXConvert::ToPx(Location);
            PxRigidBodyExt::addForceAtPos(*DynamicActor, PImpulse, PLocation, PxForceMode::eIMPULSE);
        }
    });
}

void FBodyInstance::AddAngularImpulse(const FVector& AngularImpulse, bool bVelChange)
{
    if (!CurrentScene || !RigidActor) return;

    CurrentScene->EnqueueCommand([this, AngularImpulse, bVelChange]()
    {
        if (PxRigidDynamic* DynamicActor = GetDynamicActor())
        {
            if (DynamicActor->getRigidBodyFlags() & PxRigidBodyFlag::eKINEMATIC) { return; }
            PxVec3 PImpulse = PhysXConvert::AngularToPx(AngularImpulse);
            PxForceMode::Enum Mode = bVelChange ? PxForceMode::eVELOCITY_CHANGE : PxForceMode::eIMPULSE;
            DynamicActor->addTorque(PImpulse, Mode);
        }
    });
}

FVector FBodyInstance::GetLinearVelocity() const
{
    if (PxRigidDynamic* DynamicActor = GetDynamicActor())
    {
        return PhysXConvert::FromPx(DynamicActor->getLinearVelocity());
    }
    return FVector::Zero();
}

void FBodyInstance::SetLinearVelocity(const FVector& Velocity, bool bAddToCurrent)
{
    if (!CurrentScene || !RigidActor) return;

    CurrentScene->EnqueueCommand([this, Velocity, bAddToCurrent]()
    {
        if (PxRigidDynamic* DynamicActor = GetDynamicActor())
        {
            if (DynamicActor->getRigidBodyFlags() & PxRigidBodyFlag::eKINEMATIC) { return; }

            PxVec3 PVel = PhysXConvert::ToPx(Velocity);
            if (bAddToCurrent)
            {
                PVel += DynamicActor->getLinearVelocity();
            }
            DynamicActor->setLinearVelocity(PVel);
        }
    });
}

FVector FBodyInstance::GetAngularVelocity() const
{
    if (PxRigidDynamic* DynamicActor = GetDynamicActor())
    {
        return PhysXConvert::AngularFromPx(DynamicActor->getAngularVelocity());
    }
    return FVector::Zero();
}

void FBodyInstance::SetAngularVelocity(const FVector& AngularVelocity, bool bAddToCurrent)
{
    if (!CurrentScene || !RigidActor) return;

    CurrentScene->EnqueueCommand([this, AngularVelocity, bAddToCurrent]()
    {
        if (PxRigidDynamic* DynamicActor = GetDynamicActor())
        {
            if (DynamicActor->getRigidBodyFlags() & PxRigidBodyFlag::eKINEMATIC) { return; }

            PxVec3 PVel = PhysXConvert::AngularToPx(AngularVelocity);
            if (bAddToCurrent)
            {
                PVel += DynamicActor->getAngularVelocity();
            }
            DynamicActor->setAngularVelocity(PVel);
            }
    });
}

// --- UBodySetupCore 설정 적용 ---

void FBodyInstance::ApplyBodySetupSettings(const UBodySetupCore* BodySetupCore)
{
    if (!BodySetupCore) return;

    // PhysicsType 적용 (Simulated/Kinematic/Default)
    ApplyPhysicsType(BodySetupCore->PhysicsType);

    // CollisionResponse 적용 (충돌 활성/비활성)
    ApplyCollisionResponse(BodySetupCore->CollisionResponse);
}

void FBodyInstance::ApplyPhysicsType(EPhysicsType PhysicsType)
{
    PxRigidDynamic* DynamicActor = GetDynamicActor();
    if (!DynamicActor) return;

    switch (PhysicsType)
    {
    case EPhysicsType::Simulated:
        // 물리 시뮬레이션 활성화 (Kinematic 플래그 해제)
        DynamicActor->setRigidBodyFlag(PxRigidBodyFlag::eKINEMATIC, false);
        bSimulatePhysics = true;
        break;

    case EPhysicsType::Kinematic:
        // 키네마틱 모드 (물리 시뮬레이션 비활성화, 직접 제어)
        DynamicActor->setRigidBodyFlag(PxRigidBodyFlag::eKINEMATIC, true);
        bSimulatePhysics = false;
        break;

    case EPhysicsType::Default:
    default:
        // 기존 bSimulatePhysics 값 유지
        DynamicActor->setRigidBodyFlag(PxRigidBodyFlag::eKINEMATIC, !bSimulatePhysics);
        break;
    }
}

void FBodyInstance::ApplyCollisionResponse(EBodyCollisionResponse::Type Response)
{
    if (!RigidActor) return;

    bool bEnableCollision = (Response == EBodyCollisionResponse::BodyCollision_Enabled);
    
    for (int32 i = 0; i < Shapes.Num(); ++i)
    {
        if (!Shapes[i]) continue;

        if (bCollisionEnabled)
        {
            // 충돌 활성화
            Shapes[i]->setFlag(PxShapeFlag::eSIMULATION_SHAPE, bEnableCollision && !bIsTrigger);
            Shapes[i]->setFlag(PxShapeFlag::eSCENE_QUERY_SHAPE, true);
        }
        else
        {
            // 충돌 비활성화 (오버랩만 감지)
            Shapes[i]->setFlag(PxShapeFlag::eSIMULATION_SHAPE, false);
            Shapes[i]->setFlag(PxShapeFlag::eSCENE_QUERY_SHAPE, false);
        }
    }
}
