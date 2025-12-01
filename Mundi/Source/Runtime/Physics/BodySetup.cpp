#include "pch.h"
#include "BodySetup.h"
#include "BodyInstance.h"
#include "PhysicalMaterial.h"
#include "PhysicsSystem.h"

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
}

// --- PhysX Shape 생성 ---

void UBodySetup::CreatePhysicsShapes(FBodyInstance* BodyInstance, const FVector& Scale3D,
                                      UPhysicalMaterial* InMaterial)
{
    if (!BodyInstance || !BodyInstance->RigidActor) return;

    FPhysicsSystem* System = GEngine.GetPhysicsSystem();
    PxPhysics* Physics = System->GetPhysics();
    if (!Physics) return;

    // 사용할 재질 결정
    PxMaterial* Material = System->GetDefaultMaterial();
    UPhysicalMaterial* UsedMat = InMaterial ? InMaterial : PhysMaterial;
    if (UsedMat && UsedMat->MatHandle)
    {
        Material = UsedMat->MatHandle;
    }

    // Sphere Shape 생성
    for (int32 i = 0; i < AggGeom.SphereElems.Num(); ++i)
    {
        const FKSphereElem& Elem = AggGeom.SphereElems[i];
        if (Elem.GetCollisionEnabled() == ECollisionEnabled::NoCollision) continue;

        PxShape* Shape = CreateSphereShape(Elem, Scale3D, Material);
        if (Shape)
        {
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
            ConfigureShapeFlags(Shape, Elem.GetCollisionEnabled(), BodyInstance->IsTrigger());
            BodyInstance->RigidActor->attachShape(*Shape);
            BodyInstance->Shapes.Add(Shape);
            Shape->release();
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

        // 3. 캡슐 축 회전 적용 (프로젝트 Z축 → PhysX X축)
        PxQuat AxisRotation = PhysXConvert::GetCapsuleAxisRotation();

        // 최종 회전 = 요소 회전 * 축 변환 회전
        PxQuat FinalRotation = PxElemRotation * AxisRotation;

        PxTransform LocalPose(PxCenter, FinalRotation);
        Shape->setLocalPose(LocalPose);
    }

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

void UBodySetup::ClearAllShapes()
{
    AggGeom.EmptyElements();
}

// --- 직렬화 ---

void UBodySetup::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
    Super::Serialize(bInIsLoading, InOutHandle);

    //if (bInIsLoading)
    //{
    //    // 로드
    //    if (InOutHandle.hasKey("BoneName"))
    //    {
    //        BoneName = FName(InOutHandle["BoneName"].ToString().c_str());
    //    }

    //    if (InOutHandle.hasKey("DefaultCollisionEnabled"))
    //    {
    //        DefaultCollisionEnabled = static_cast<ECollisionEnabled>(InOutHandle["DefaultCollisionEnabled"].ToInt());
    //    }

    //    // TODO: AggGeom 직렬화 (Shape 배열)
    //}
    //else
    //{
    //    // 저장
    //    InOutHandle["BoneName"] = BoneName.ToString().c_str();
    //    InOutHandle["DefaultCollisionEnabled"] = static_cast<int>(DefaultCollisionEnabled);

    //    // TODO: AggGeom 직렬화 (Shape 배열)
    //}
}

