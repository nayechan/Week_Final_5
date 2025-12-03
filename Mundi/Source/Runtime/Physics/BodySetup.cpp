#include "pch.h"
#include "BodySetup.h"
#include "BodyInstance.h"
#include "PhysicalMaterial.h"
#include "PhysicsSystem.h"
#include "JsonSerializer.h"

using namespace physx;

// --- 내부 헬퍼 함수 ---

namespace
{
    void ConfigureShapeFlags(PxShape* Shape, ECollisionEnabled CollisionEnabled, bool bIsTrigger)
    {
        if (!Shape) return;

        // 기본적으로 모든 플래그 끄기
        Shape->setFlag(PxShapeFlag::eSIMULATION_SHAPE, false);
        Shape->setFlag(PxShapeFlag::eSCENE_QUERY_SHAPE, false);
        Shape->setFlag(PxShapeFlag::eTRIGGER_SHAPE, false);

        if (bIsTrigger)
        {
            // Trigger는 충돌 없이 오버랩만 감지
            Shape->setFlag(PxShapeFlag::eTRIGGER_SHAPE, true);
            Shape->setFlag(PxShapeFlag::eSCENE_QUERY_SHAPE, true);
            return;
        }

        switch (CollisionEnabled)
        {
        case ECollisionEnabled::QueryAndPhysics:
            Shape->setFlag(PxShapeFlag::eSIMULATION_SHAPE, true);
            Shape->setFlag(PxShapeFlag::eSCENE_QUERY_SHAPE, true);
            break;

        case ECollisionEnabled::QueryOnly:
            Shape->setFlag(PxShapeFlag::eSCENE_QUERY_SHAPE, true);
            break;

        case ECollisionEnabled::PhysicsOnly:
            Shape->setFlag(PxShapeFlag::eSIMULATION_SHAPE, true);
            break;

        case ECollisionEnabled::NoCollision:
        default:
            // 모든 플래그 비활성화 상태 유지
            break;
        }
    }
}

// --- 생성자/소멸자 ---

UBodySetup::UBodySetup()
    : DefaultCollisionEnabled(ECollisionEnabled::QueryAndPhysics)
    , PhysMaterial(nullptr)
{
}

UBodySetup::~UBodySetup()
{
    ClearAllShapes();
}

void UBodySetup::CreatePhysicsMeshes()
{
    ClearPhysicsMeshes();

    FPhysicsSystem* System = GEngine.GetPhysicsSystem();
    if (!System) { return; }
    
    auto* Cooking = System->GetCooking();
    auto* Physics = System->GetPhysics();

    if (!Cooking || !Physics) return;

    for (FKConvexElem& Elem : AggGeom.ConvexElems)
    {
        if (Elem.CookedData.Num() > 0)
        {
            PxDefaultMemoryInputData Input(Elem.CookedData.GetData(), Elem.CookedData.Num());
            Elem.ConvexMesh = Physics->createConvexMesh(Input);
            continue;
        }
        
        if (Elem.VertexData.Num() < 4) 
        {
            UE_LOG("[Physics] Skipping convex mesh with insufficient vertices.");
            continue; 
        }

        TArray<PxVec3> PxVerts;
        PxVerts.Reserve(Elem.VertexData.Num());
        for (const FVector& V : Elem.VertexData)
        {
            PxVerts.Add(PhysXConvert::ToPx(V));
        }

        PxConvexMeshDesc Desc;
        Desc.points.data = PxVerts.GetData();
        Desc.points.count = PxVerts.Num();
        Desc.points.stride = sizeof(PxVec3);
        Desc.flags = PxConvexFlag::eCOMPUTE_CONVEX; // 자동 Convex 계산

        PxDefaultMemoryOutputStream Output;
        if (Cooking->cookConvexMesh(Desc, Output))
        {
            Elem.CookedData.SetNum(Output.getSize());
            memcpy(Elem.CookedData.GetData(), Output.getData(), Output.getSize());
            PxDefaultMemoryInputData Input(Elem.CookedData.GetData(), Elem.CookedData.Num());
            Elem.ConvexMesh = Physics->createConvexMesh(Input);
        }
        else
        {
            UE_LOG("[Physics] Failed to cook convex mesh!");
        }
    }
}

void UBodySetup::ClearPhysicsMeshes()
{
    for (FKConvexElem& Elem : AggGeom.ConvexElems)
    {
        if (Elem.ConvexMesh)
        {
            Elem.ConvexMesh->release();
            Elem.ConvexMesh = nullptr;
        }
    }
}

// --- PhysX Shape 생성 ---
void UBodySetup::CreatePhysicsShapes(FBodyInstance* BodyInstance, const FVector& Scale3D,
                                      UPhysicalMaterial* InMaterial, ECollisionShapeMode ShapeMode)
{
    if (!BodyInstance || !BodyInstance->RigidActor) return;

    FPhysicsSystem* System = GEngine.GetPhysicsSystem();
    PxPhysics* Physics = System->GetPhysics();
    
    if (!Physics) return;

    // 사용할 재질 결정
    PxMaterial* Material = System->GetDefaultMaterial();
    UPhysicalMaterial* UsedMat = InMaterial ? InMaterial : PhysMaterial;
    if (UsedMat)
    {
        if (!UsedMat->MatHandle) { UsedMat->CreateMaterial(); }
        Material = UsedMat->MatHandle;
    }
    void* BoneNameUserData = (void*)&this->BoneName;

    if (ShapeMode == ECollisionShapeMode::Simple)
    {
        // Sphere Shape 생성
        for (int32 i = 0; i < AggGeom.SphereElems.Num(); ++i)
        {
            const FKSphereElem& Elem = AggGeom.SphereElems[i];
            if (Elem.GetCollisionEnabled() == ECollisionEnabled::NoCollision) continue;

            PxShape* Shape = CreateSphereShape(Elem, Scale3D, Material);
            if (Shape)
            {
                Shape->userData = BoneNameUserData;
                ConfigureShapeFlags(Shape, Elem.GetCollisionEnabled(), BodyInstance->IsTrigger());
                BodyInstance->RigidActor->attachShape(*Shape);
                BodyInstance->Shapes.Add(Shape);
                Shape->release(); // Actor가 소유권을 가짐
            }
        }

        // Box Shape 생성
        for (int32 i = 0; i < AggGeom.BoxElems.Num(); ++i)
        {
            const FKBoxElem& Elem = AggGeom.BoxElems[i];
            if (Elem.GetCollisionEnabled() == ECollisionEnabled::NoCollision) continue;

            PxShape* Shape = CreateBoxShape(Elem, Scale3D, Material);
            if (Shape)
            {
                Shape->userData = BoneNameUserData;
                ConfigureShapeFlags(Shape, Elem.GetCollisionEnabled(), BodyInstance->IsTrigger());
                BodyInstance->RigidActor->attachShape(*Shape);
                BodyInstance->Shapes.Add(Shape);
                Shape->release();
            }
        }

        // Capsule Shape 생성
        for (int32 i = 0; i < AggGeom.SphylElems.Num(); ++i)
        {
            const FKSphylElem& Elem = AggGeom.SphylElems[i];
            if (Elem.GetCollisionEnabled() == ECollisionEnabled::NoCollision) continue;

            PxShape* Shape = CreateCapsuleShape(Elem, Scale3D, Material);
            if (Shape)
            {
                Shape->userData = BoneNameUserData;
                ConfigureShapeFlags(Shape, Elem.GetCollisionEnabled(), BodyInstance->IsTrigger());
                BodyInstance->RigidActor->attachShape(*Shape);
                BodyInstance->Shapes.Add(Shape);
                Shape->release();
            }
        }
    }

    if (ShapeMode == ECollisionShapeMode::Convex)
    {
        for (const auto& Elem : AggGeom.ConvexElems)
        {
            if (Elem.ConvexMesh == nullptr) { continue; }
            PxShape* Shape = CreateConvexShape(Elem, Scale3D, Material);
            
            if (Shape)
            {
                Shape->userData = BoneNameUserData;
                ConfigureShapeFlags(Shape, Elem.GetCollisionEnabled(), BodyInstance->IsTrigger());
                BodyInstance->RigidActor->attachShape(*Shape);
                BodyInstance->Shapes.Add(Shape);
                Shape->release();
            }
        }
    }
}

PxShape* UBodySetup::CreateSphereShape(const FKSphereElem& Elem, const FVector& Scale3D,
                                        PxMaterial* Material)
{
    FPhysicsSystem* System = GEngine.GetPhysicsSystem();
    PxPhysics* Physics = System->GetPhysics();
    if (!Physics || !Material) return nullptr;

    // 스케일 적용 (균일 스케일 사용 - 최소값)
    float UniformScale = FMath::Min(FMath::Min(FMath::Abs(Scale3D.X), FMath::Abs(Scale3D.Y)), FMath::Abs(Scale3D.Z));
    float ScaledRadius = Elem.Radius * UniformScale;

    if (ScaledRadius <= 0.0f) return nullptr;

    // Geometry 생성
    PxSphereGeometry Geometry(ScaledRadius);
    PxShape* Shape = Physics->createShape(Geometry, *Material, true);

    if (Shape)
    {
        // Local Pose 설정 (중심 위치)
        // 프로젝트 좌표계 → PhysX 좌표계 변환
        FVector ScaledCenter = Elem.Center * Scale3D;
        PxTransform LocalPose(PhysXConvert::ToPx(ScaledCenter));
        Shape->setLocalPose(LocalPose);
    }

    return Shape;
}

PxShape* UBodySetup::CreateBoxShape(const FKBoxElem& Elem, const FVector& Scale3D,
                                     PxMaterial* Material)
{
    FPhysicsSystem* System = GEngine.GetPhysicsSystem();
    PxPhysics* Physics = System->GetPhysics();
    if (!Physics || !Material) return nullptr;

    // Half Extent에 스케일 적용
    FVector ScaledHalfExtent(Elem.X * Scale3D.X, Elem.Y * Scale3D.Y, Elem.Z * Scale3D.Z);

    if (ScaledHalfExtent.X <= 0.0f || ScaledHalfExtent.Y <= 0.0f || ScaledHalfExtent.Z <= 0.0f)
        return nullptr;

    // Geometry 생성 (PhysX 좌표계로 변환)
    PxBoxGeometry Geometry(PhysXConvert::BoxHalfExtentToPx(ScaledHalfExtent));
    PxShape* Shape = Physics->createShape(Geometry, *Material, true);

    if (Shape)
    {
        // Local Pose 설정 (위치 + 회전)
        FVector ScaledCenter = Elem.Center * Scale3D;
        FQuat Rotation = Elem.Rotation.GetNormalized();

        PxTransform LocalPose(PhysXConvert::ToPx(ScaledCenter), PhysXConvert::ToPx(Rotation));
        Shape->setLocalPose(LocalPose);
    }

    return Shape;
}

PxShape* UBodySetup::CreateCapsuleShape(const FKSphylElem& Elem, const FVector& Scale3D,
                                         PxMaterial* Material)
{
    FPhysicsSystem* System = GEngine.GetPhysicsSystem();
    PxPhysics* Physics = System->GetPhysics();
    if (!Physics || !Material) return nullptr;

    // 캡슐 스케일 적용
    // XY 평면에서의 반지름은 X, Y 스케일 중 최소값 사용
    // 길이는 Z 스케일 사용
    float RadiusScale = FMath::Min(Scale3D.X, Scale3D.Y);
    float LengthScale = Scale3D.Z;

    float ScaledRadius = Elem.Radius * RadiusScale;
    float ScaledHalfHeight = (Elem.Length * 0.5f) * LengthScale;

    if (ScaledRadius <= 0.0f || ScaledHalfHeight < 0.0f) return nullptr;

    // PhysX Capsule은 X축이 캡슐 축
    // Geometry의 halfHeight는 원통 부분의 절반 길이
    PxCapsuleGeometry Geometry(ScaledRadius, ScaledHalfHeight);
    PxShape* Shape = Physics->createShape(Geometry, *Material, true);

    if (Shape)
    {
        // Local Pose 설정
        // 1. 위치 변환
        FVector ScaledCenter = Elem.Center * Scale3D;
        PxVec3 PxCenter = PhysXConvert::ToPx(ScaledCenter);

        // 2. 회전 변환
        FQuat ElemRotation = Elem.Rotation.GetNormalized();
        PxQuat PxElemRotation = PhysXConvert::ToPx(ElemRotation);

        // 3. 캡슐 축 회전 적용 (누워 있는 캡슐을 로컬 공간에서 Y축 방향으로 세움)
        PxQuat AxisRotation = PhysXConvert::GetCapsuleAxisRotation();

        // 최종 회전 = 요소 회전 * 축 변환 회전
        PxQuat FinalRotation = PxElemRotation * AxisRotation;

        PxTransform LocalPose(PxCenter, FinalRotation);
        Shape->setLocalPose(LocalPose);
    }

    return Shape;
}

physx::PxShape* UBodySetup::CreateConvexShape(const FKConvexElem& Elem, const FVector& Scale3D, physx::PxMaterial* Material)
{
    if (!Elem.ConvexMesh || !Material) return nullptr;

    FPhysicsSystem* System = GEngine.GetPhysicsSystem();
    PxPhysics* Physics = System->GetPhysics();

    FVector TotalScale = Elem.Transform.Scale3D * Scale3D;
    PxMeshScale MeshScale(PhysXConvert::ScaleToPx(TotalScale), PxQuat(PxIdentity));
    PxConvexMeshGeometry ConvexGeom(Elem.ConvexMesh, MeshScale);

    // Shape 생성 (isExclusive=true로 설정하여 최적화 권장)
    PxShape* Shape = Physics->createShape(ConvexGeom, *Material, true);
    if (!Shape) return nullptr;

    FTransform T = Elem.Transform;
    T.Scale3D = FVector::One(); 
    PxTransform PxT = PhysXConvert::ToPx(T);
    Shape->setLocalPose(PxT);

    return Shape;
}

// --- 유틸리티 ---

float UBodySetup::GetVolume(const FVector& Scale3D) const
{
    return AggGeom.GetScaledVolume(Scale3D);
}

// --- Shape 추가 헬퍼 함수 ---

int32 UBodySetup::AddSphereElem(float Radius, const FVector& Center)
{
    FKSphereElem Elem(Radius);
    Elem.Center = Center;
    return AggGeom.AddSphereElem(Elem);
}

int32 UBodySetup::AddBoxElem(float HalfX, float HalfY, float HalfZ,
                              const FVector& Center, const FQuat& Rotation)
{
    FKBoxElem Elem(HalfX, HalfY, HalfZ);
    Elem.Center = Center;
    Elem.Rotation = Rotation;
    return AggGeom.AddBoxElem(Elem);
}

int32 UBodySetup::AddCapsuleElem(float Radius, float HalfHeight,
                                  const FVector& Center, const FQuat& Rotation)
{
    // FKSphylElem의 Length는 원통 부분의 전체 길이
    FKSphylElem Elem(Radius, HalfHeight * 2.0f);
    Elem.Center = Center;
    Elem.Rotation = Rotation;
    return AggGeom.AddSphylElem(Elem);
}

int32 UBodySetup::AddConvexElem(const TArray<FVector>& InVertices, const FTransform& InTransform)
{
    FKConvexElem NewElem;
    
    NewElem.VertexData = InVertices;
    NewElem.Transform = InTransform;

    NewElem.UpdateElemBox(); 

    return AggGeom.ConvexElems.Add(NewElem);
}

void UBodySetup::ClearAllShapes()
{
    ClearPhysicsMeshes();
    AggGeom.EmptyElements();
}

// --- 직렬화 ---
FArchive& operator<<(FArchive& Ar, UBodySetup& BodySetup)
{
    if (Ar.IsSaving())
    {
        // 1. DefaultCollisionEnabled 저장
        int32 CollisionEnabled = static_cast<int32>(BodySetup.DefaultCollisionEnabled);
        Ar << CollisionEnabled;

        // 2. Sphere Elements
        int32 SphereCount = BodySetup.AggGeom.SphereElems.Num();
        Ar << SphereCount;
        for (int32 i = 0; i < SphereCount; ++i)
        {
            FKSphereElem& Elem = BodySetup.AggGeom.SphereElems[i];
            Ar << Elem.Radius;
            Ar << Elem.Center;
        }

        // 3. Box Elements
        int32 BoxCount = BodySetup.AggGeom.BoxElems.Num();
        Ar << BoxCount;
        for (int32 i = 0; i < BoxCount; ++i)
        {
            FKBoxElem& Elem = BodySetup.AggGeom.BoxElems[i];
            Ar << Elem.X;
            Ar << Elem.Y;
            Ar << Elem.Z;
            Ar << Elem.Center;
            Ar << Elem.Rotation;
        }

        // 4. Capsule Elements
        int32 CapsuleCount = BodySetup.AggGeom.SphylElems.Num();
        Ar << CapsuleCount;
        for (int32 i = 0; i < CapsuleCount; ++i)
        {
            FKSphylElem& Elem = BodySetup.AggGeom.SphylElems[i];
            Ar << Elem.Radius;
            Ar << Elem.Length;
            Ar << Elem.Center;
            Ar << Elem.Rotation;
        }

        // 5. Convex Elements
        int32 ConvexCount = BodySetup.AggGeom.ConvexElems.Num();
        Ar << ConvexCount;

        for (int32 i = 0; i < ConvexCount; ++i)
        {
            FKConvexElem& Elem = BodySetup.AggGeom.ConvexElems[i];
            FVector Pos = Elem.Transform.Translation;
            FQuat Rot = Elem.Transform.Rotation;
            FVector Scale = Elem.Transform.Scale3D;

            Ar << Pos;
            Ar << Rot;
            Ar << Scale;

            Serialization::WriteArray(Ar, Elem.VertexData);
            Serialization::WriteArray(Ar, Elem.IndexData);
            Serialization::WriteArray(Ar, Elem.CookedData);
        }
    }
    else if (Ar.IsLoading())
    {
        // 1. DefaultCollisionEnabled 로드
        int32 CollisionEnabled = 0;
        Ar << CollisionEnabled;
        BodySetup.DefaultCollisionEnabled = static_cast<ECollisionEnabled>(CollisionEnabled);

        // 2. Sphere Elements 로드
        int32 SphereCount = 0;
        Ar << SphereCount;
        for (int32 i = 0; i < SphereCount; ++i)
        {
            float Radius;
            FVector Center;
            Ar << Radius;
            Ar << Center;
            BodySetup.AddSphereElem(Radius, Center);
        }

        // 3. Box Elements 로드
        int32 BoxCount = 0;
        Ar << BoxCount;
        for (int32 i = 0; i < BoxCount; ++i)
        {
            float X, Y, Z;
            FVector Center;
            FQuat Rotation;
            Ar << X << Y << Z;
            Ar << Center;
            Ar << Rotation;
            BodySetup.AddBoxElem(X, Y, Z, Center, Rotation);
        }

        // 4. Capsule Elements 로드
        int32 CapsuleCount = 0;
        Ar << CapsuleCount;
        for (int32 i = 0; i < CapsuleCount; ++i)
        {
            float Radius, Length;
            FVector Center;
            FQuat Rotation;
            Ar << Radius << Length;
            Ar << Center;
            Ar << Rotation;
            BodySetup.AddCapsuleElem(Radius, Length * 0.5f, Center, Rotation);
        }

        // 5. Convex Elements 로드
        int32 ConvexCount = 0;
        Ar << ConvexCount;

        for (int32 i = 0; i < ConvexCount; ++i)
        {
            // A. Transform 로드
            FVector Pos;
            FQuat Rot;
            FVector Scale;
            Ar << Pos;
            Ar << Rot;
            Ar << Scale;

            FTransform Transform(Pos, Rot, Scale);

            TArray<FVector> LoadedVertices;
            Serialization::ReadArray(Ar, LoadedVertices);

            // C. IndexData Load
            TArray<int32> LoadedIndices;
            Serialization::ReadArray(Ar, LoadedIndices);

            // D. BodySetup에 추가
            int32 NewIdx = BodySetup.AddConvexElem(LoadedVertices, Transform);

            // 인덱스 데이터 설정
            if (BodySetup.AggGeom.ConvexElems.Num() > NewIdx && NewIdx >= 0)
            {
                BodySetup.AggGeom.ConvexElems[NewIdx].IndexData = LoadedIndices;
                Serialization::ReadArray(Ar, BodySetup.AggGeom.ConvexElems[NewIdx].CookedData);
            }
            else
            {
                TArray<uint8> Dummy;
                Serialization::ReadArray(Ar, Dummy);
            }
        }
        BodySetup.CreatePhysicsMeshes();
    }
    return Ar;
}

