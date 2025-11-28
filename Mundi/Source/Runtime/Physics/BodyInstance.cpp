#include "pch.h"
#include "BodyInstance.h"
#include "PhysicalMaterial.h"
using namespace physx;

void FBodyInstance::InitBody(const FTransform& Transform, const PxGeometry& Geometry, UPhysicalMaterial* PhysMat)
{
    // 이미 있으면 삭제하고 다시 생성
    TermBody();

    FPhysicsSystem& System = FPhysicsSystem::Get();
    PxPhysics* Physics = System.GetPhysics();
    PxScene* Scene = System.GetScene();
    PxTransform PTransform = Transform;

    // Actor 생성 (Static vs Dynamic)
    if (bSimulatePhysics)
    {
        PxRigidDynamic* DynamicActor = Physics->createRigidDynamic(PTransform);
        DynamicActor->setLinearDamping(LinearDamping);
        DynamicActor->setAngularDamping(AngularDamping);
        DynamicActor->setActorFlag(PxActorFlag::eDISABLE_GRAVITY, !bUseGravity);
        
        RigidActor = DynamicActor;
    }
    else
    {
        RigidActor = Physics->createRigidStatic(PTransform);
    }

    if (!RigidActor) return;

    // 재질 가져오기 (없으면 기본 재질)
    PxMaterial* Material = System.GetDefaultMaterial();
    if (PhysMat && PhysMat->MatHandle)
    {
        Material = PhysMat->MatHandle;
    }

    // Shape 생성 및 부착
    PxShape* Shape = Physics->createShape(Geometry, *Material);
    if (Shape)
    {
        if (bIsTrigger)
        {
            // 물리적 충돌 끔
            Shape->setFlag(PxShapeFlag::eSIMULATION_SHAPE, false); 
            // Overlap 감지 켬
            Shape->setFlag(PxShapeFlag::eTRIGGER_SHAPE, true);  
        }
        else
        {
            Shape->setFlag(PxShapeFlag::eSIMULATION_SHAPE, true);
            Shape->setFlag(PxShapeFlag::eTRIGGER_SHAPE, false);
        }
        
        RigidActor->attachShape(*Shape);
        Shape->release();
    }

    // 질량 계산
    if (bSimulatePhysics)
    {
        if (PxRigidDynamic* DynamicActor = RigidActor->is<PxRigidDynamic>())
        {
            if (bOverrideMass)
            {
                PxRigidBodyExt::setMassAndUpdateInertia(*DynamicActor, MassInKg);
            }
            else
            {
                float Density = 10.0f; // 기본 밀도
                if (PhysMat) { Density = PhysMat->Density; }
                PxRigidBodyExt::updateMassAndInertia(*DynamicActor, Density);
            }
        }
    }

    // UserData 연결
    RigidActor->userData = static_cast<void*>(this);

    // 씬에 추가
    Scene->addActor(*RigidActor);
}

void FBodyInstance::TermBody()
{
    if (RigidActor)
    {
        FPhysicsSystem::Get().GetScene()->removeActor(*RigidActor);
        RigidActor->release();
        RigidActor = nullptr;
    }
}

void FBodyInstance::AddForce(const FVector& Force)
{
    if (RigidActor)
    {
        PxVec3 PForce = Force;
        
        if (PxRigidDynamic* RigidDynamic = RigidActor->is<PxRigidDynamic>())
        {
            RigidDynamic->addForce(PForce);
        }
    }
}

void FBodyInstance::SetLinearVelocity(const FVector& Velocity)
{
    if (RigidActor)
    {
        PxVec3 PVel = Velocity;
        if (PxRigidDynamic* RigidDynamic = RigidActor->is<PxRigidDynamic>())
        {
            RigidDynamic->setLinearVelocity(PVel);
        }
    }
}

FTransform FBodyInstance::GetWorldTransform() const
{
    if (RigidActor)
    {
        return RigidActor->getGlobalPose();
    }
    return {};
}