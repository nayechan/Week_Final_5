#include "pch.h"
#include "BodySetup.h"
#include "BodyInstance.h"


#include "PhysicalMaterial.h"

UBodySetup::UBodySetup()
{
} 

UBodySetup::~UBodySetup()
{
}

void UBodySetup::AddShapesToRigidActor_AssumesLocked(FBodyInstance* OwningInstance, const FVector& Scale3D, PxRigidActor* PDestActor, UPhysicalMaterial* InPhysicalMaterial)
{
    if (!PDestActor || !GPhysXSDK)
    {
        return;
    }

    PxMaterial* PhysMaterialToUse;
    if (!InPhysicalMaterial)
    {
        assert(GPhysicalMaterial);
        PhysMaterialToUse = GPhysicalMaterial->GetPxMaterial();
    }
    else
    {
        PhysMaterialToUse = InPhysicalMaterial->GetPxMaterial();
    }

    // ====================================================================
    // 1. Box Elements
    // ====================================================================
    for (const FKBoxElem& BoxElem : AggGeom.BoxElems)
    {
        if (BoxElem.GetCollisionEnabled() == ECollisionEnabled::NoCollision)
        {
            continue;
        }

        PxBoxGeometry BoxGeom = BoxElem.GetPxGeometry(Scale3D);

        PxShape* NewShape = GPhysXSDK->createShape(BoxGeom, *PhysMaterialToUse, true);

        if (NewShape)
        {
            FTransform ElemTM = BoxElem.GetTransform();
            
            FVector ScaledCenter;
            ScaledCenter.X = ElemTM.Translation.X * Scale3D.X;
            ScaledCenter.Y = ElemTM.Translation.Y * Scale3D.Y;
            ScaledCenter.Z = ElemTM.Translation.Z * Scale3D.Z;

            PxTransform PxLocalPose(U2PVector(ScaledCenter), U2PQuat(ElemTM.Rotation));
            NewShape->setLocalPose(PxLocalPose);

            if (BoxElem.GetCollisionEnabled() == ECollisionEnabled::QueryOnly)
            {
                NewShape->setFlag(PxShapeFlag::eSIMULATION_SHAPE, false);
                NewShape->setFlag(PxShapeFlag::eSCENE_QUERY_SHAPE, true);
            }
            else if (BoxElem.GetCollisionEnabled() == ECollisionEnabled::PhysicsOnly)
            {
                NewShape->setFlag(PxShapeFlag::eSIMULATION_SHAPE, true);
                NewShape->setFlag(PxShapeFlag::eSCENE_QUERY_SHAPE, false);
            }
            else
            {
                NewShape->setFlag(PxShapeFlag::eSIMULATION_SHAPE, true);
                NewShape->setFlag(PxShapeFlag::eSCENE_QUERY_SHAPE, true);
            }

            PDestActor->attachShape(*NewShape);

            NewShape->release();
        }
    }

    // ====================================================================
    // 2. Sphere Elements
    // ====================================================================
    for (const FKSphereElem& SphereElem : AggGeom.SphereElems)
    {
        if (SphereElem.CollisionEnabled == ECollisionEnabled::NoCollision) continue;

        PxSphereGeometry SphereGeom = SphereElem.GetPxGeometry(Scale3D);

        PxShape* NewShape = GPhysXSDK->createShape(SphereGeom, *PhysMaterialToUse, true);
        if (NewShape)
        {
            FTransform ElemTM = SphereElem.GetTransform();
            FVector ScaledCenter;
            ScaledCenter.X = ElemTM.Translation.X * Scale3D.X;
            ScaledCenter.Y = ElemTM.Translation.Y * Scale3D.Y;
            ScaledCenter.Z = ElemTM.Translation.Z * Scale3D.Z;

            NewShape->setLocalPose(PxTransform(U2PVector(ScaledCenter), U2PQuat(ElemTM.Rotation)));

            NewShape->setFlag(PxShapeFlag::eSIMULATION_SHAPE, true);
            NewShape->setFlag(PxShapeFlag::eSCENE_QUERY_SHAPE, true);

            PDestActor->attachShape(*NewShape);
            NewShape->release();
        }
    }

    // ====================================================================
    // 3. Sphyl (Capsule) Elements
    // ====================================================================
    for (const FKSphylElem& SphylElem : AggGeom.SphylElems)
    {
        if (SphylElem.CollisionEnabled == ECollisionEnabled::NoCollision) continue;

        PxCapsuleGeometry CapsuleGeom = SphylElem.GetPxGeometry(Scale3D);

        PxShape* NewShape = GPhysXSDK->createShape(CapsuleGeom, *PhysMaterialToUse, true);
        if (NewShape)
        {
            FTransform ElemTM = SphylElem.GetTransform();
            
            FVector ScaledCenter;
            ScaledCenter.X = ElemTM.Translation.X * Scale3D.X;
            ScaledCenter.Y = ElemTM.Translation.Y * Scale3D.Y;
            ScaledCenter.Z = ElemTM.Translation.Z * Scale3D.Z;

            // [중요] 회전 보정
            // 언리얼: 캡슐 길이 방향 = Z축
            // PhysX : 캡슐 길이 방향 = X축
            // 따라서 로컬 회전에 "Z->X 90도 회전"을 추가해야 합니다.
            
            PxQuat UERot = U2PQuat(ElemTM.Rotation);
            // Z축(0,0,1)을 X축(1,0,0)으로 보내는 회전: Y축 기준 -90도 or +90도
            // (Unreal 좌표계 기준 Y축 회전)
            PxQuat FixRot(PxHalfPi, PxVec3(0, 1, 0)); 
            
            PxTransform LocalPose(U2PVector(ScaledCenter), UERot * FixRot);
            NewShape->setLocalPose(LocalPose);

            NewShape->setFlag(PxShapeFlag::eSIMULATION_SHAPE, true);
            NewShape->setFlag(PxShapeFlag::eSCENE_QUERY_SHAPE, true);

            PDestActor->attachShape(*NewShape);
            NewShape->release();
        }
    }
}

void UBodySetup::AddBoxTest(const FVector& Extent)
{
    FKBoxElem Box;
    Box.X = Extent.X;
    Box.Y = Extent.Y;
    Box.Z = Extent.Z;
    AggGeom.BoxElems.Add(Box);
}

void UBodySetup::AddSphereTest(float Radius)
{
    FKSphereElem Sphere(Radius);
    AggGeom.SphereElems.Add(Sphere);
}

void UBodySetup::AddCapsuleTest(float Radius, float Length)
{
    FKSphylElem Capsule(Radius, Length);
    AggGeom.SphylElems.Add(Capsule);
}

