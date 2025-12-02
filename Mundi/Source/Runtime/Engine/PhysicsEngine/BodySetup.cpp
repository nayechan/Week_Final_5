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
            else if (BoxElem.GetCollisionEnabled() == ECollisionEnabled::QueryAndPhysics)
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

void UBodySetup::DuplicateSubObjects()
{
	UObject::DuplicateSubObjects();

	// PhysicalMaterial은 리소스 참조이므로 포인터 공유 (복제 안함)
	// AggGeom은 값 타입이므로 얕은 복사로 이미 복제됨
	// 따라서 추가 처리 없음
}

void UBodySetup::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	// UPROPERTY로 선언된 필드들 자동 직렬화 (BoneName, BoneIndex, PhysicalMaterial)
	UObject::Serialize(bInIsLoading, InOutHandle);

	if (bInIsLoading)
	{
		// AggGeom 로드 (수동 처리)
		AggGeom.BoxElems.Empty();
		AggGeom.SphereElems.Empty();
		AggGeom.SphylElems.Empty();

		JSON AggGeomJson;
		if (FJsonSerializer::ReadObject(InOutHandle, "AggGeom", AggGeomJson, JSON(), false))
		{
			// Sphere Elements
			JSON SphereElemsJson;
			if (FJsonSerializer::ReadArray(AggGeomJson, "SphereElems", SphereElemsJson, JSON(), false))
			{
				for (int32 i = 0; i < static_cast<int32>(SphereElemsJson.size()); ++i)
				{
					const JSON& SphereJson = SphereElemsJson.at(i);
					FKSphereElem Sphere;
					FJsonSerializer::ReadVector(SphereJson, "Center", Sphere.Center, FVector::Zero(), false);
					FJsonSerializer::ReadFloat(SphereJson, "Radius", Sphere.Radius, 0.5f, false);
					AggGeom.SphereElems.Add(Sphere);
				}
			}

			// Box Elements
			JSON BoxElemsJson;
			if (FJsonSerializer::ReadArray(AggGeomJson, "BoxElems", BoxElemsJson, JSON(), false))
			{
				for (int32 i = 0; i < static_cast<int32>(BoxElemsJson.size()); ++i)
				{
					const JSON& BoxJson = BoxElemsJson.at(i);
					FKBoxElem Box;
					FJsonSerializer::ReadVector(BoxJson, "Center", Box.Center, FVector::Zero(), false);
					FVector4 RotVec;
					FJsonSerializer::ReadVector4(BoxJson, "Rotation", RotVec, FVector4(0, 0, 0, 1), false);
					Box.Rotation = FQuat(RotVec.X, RotVec.Y, RotVec.Z, RotVec.W);
					FJsonSerializer::ReadFloat(BoxJson, "X", Box.X, 1.f, false);
					FJsonSerializer::ReadFloat(BoxJson, "Y", Box.Y, 1.f, false);
					FJsonSerializer::ReadFloat(BoxJson, "Z", Box.Z, 1.f, false);
					AggGeom.BoxElems.Add(Box);
				}
			}

			// Sphyl (Capsule) Elements
			JSON SphylElemsJson;
			if (FJsonSerializer::ReadArray(AggGeomJson, "SphylElems", SphylElemsJson, JSON(), false))
			{
				for (int32 i = 0; i < static_cast<int32>(SphylElemsJson.size()); ++i)
				{
					const JSON& SphylJson = SphylElemsJson.at(i);
					FKSphylElem Sphyl;
					FJsonSerializer::ReadVector(SphylJson, "Center", Sphyl.Center, FVector::Zero(), false);
					FVector4 RotVec;
					FJsonSerializer::ReadVector4(SphylJson, "Rotation", RotVec, FVector4(0, 0, 0, 1), false);
					Sphyl.Rotation = FQuat(RotVec.X, RotVec.Y, RotVec.Z, RotVec.W);
					FJsonSerializer::ReadFloat(SphylJson, "Radius", Sphyl.Radius, 0.5f, false);
					FJsonSerializer::ReadFloat(SphylJson, "Length", Sphyl.Length, 1.f, false);
					AggGeom.SphylElems.Add(Sphyl);
				}
			}
		}
	}
	else
	{
		// AggGeom 저장 (수동 처리)
		JSON AggGeomJson = JSON::Make(JSON::Class::Object);

		// Sphere Elements
		JSON SphereElemsJson = JSON::Make(JSON::Class::Array);
		for (const FKSphereElem& Sphere : AggGeom.SphereElems)
		{
			JSON SphereJson = JSON::Make(JSON::Class::Object);
			SphereJson["Center"] = FJsonSerializer::VectorToJson(Sphere.Center);
			SphereJson["Radius"] = Sphere.Radius;
			SphereElemsJson.append(SphereJson);
		}
		AggGeomJson["SphereElems"] = SphereElemsJson;

		// Box Elements
		JSON BoxElemsJson = JSON::Make(JSON::Class::Array);
		for (const FKBoxElem& Box : AggGeom.BoxElems)
		{
			JSON BoxJson = JSON::Make(JSON::Class::Object);
			BoxJson["Center"] = FJsonSerializer::VectorToJson(Box.Center);
			BoxJson["Rotation"] = FJsonSerializer::Vector4ToJson(FVector4(Box.Rotation.X, Box.Rotation.Y, Box.Rotation.Z, Box.Rotation.W));
			BoxJson["X"] = Box.X;
			BoxJson["Y"] = Box.Y;
			BoxJson["Z"] = Box.Z;
			BoxElemsJson.append(BoxJson);
		}
		AggGeomJson["BoxElems"] = BoxElemsJson;

		// Sphyl (Capsule) Elements
		JSON SphylElemsJson = JSON::Make(JSON::Class::Array);
		for (const FKSphylElem& Sphyl : AggGeom.SphylElems)
		{
			JSON SphylJson = JSON::Make(JSON::Class::Object);
			SphylJson["Center"] = FJsonSerializer::VectorToJson(Sphyl.Center);
			SphylJson["Rotation"] = FJsonSerializer::Vector4ToJson(FVector4(Sphyl.Rotation.X, Sphyl.Rotation.Y, Sphyl.Rotation.Z, Sphyl.Rotation.W));
			SphylJson["Radius"] = Sphyl.Radius;
			SphylJson["Length"] = Sphyl.Length;
			SphylElemsJson.append(SphylJson);
		}
		AggGeomJson["SphylElems"] = SphylElemsJson;

		InOutHandle["AggGeom"] = AggGeomJson;
	}
}

