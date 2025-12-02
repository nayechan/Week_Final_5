#pragma once
#include "ShapeElem.h"
#include "PrimitiveDrawInterface.h"
#include "FKSphereElem.generated.h"

/** 충돌용 Sphere Shape */
USTRUCT(DisplayName="Sphere Element", Description="충돌용 구체 Shape")
struct FKSphereElem : public FKShapeElem
{
    GENERATED_REFLECTION_BODY()

public:
    // 정적 Shape 타입
    static constexpr EAggCollisionShape StaticShapeType = EAggCollisionShape::Sphere;

    // 데이터 멤버
    UPROPERTY(EditAnywhere, Category="Shape")
    FVector Center;     // 중심 위치

    UPROPERTY(EditAnywhere, Category="Shape")
    float Radius;       // 반지름

    // 생성자
    FKSphereElem()
        : FKShapeElem(EAggCollisionShape::Sphere)
        , Center(FVector::Zero())
        , Radius(1.0f)
    {
    }

    explicit FKSphereElem(float InRadius)
        : FKShapeElem(EAggCollisionShape::Sphere)
        , Center(FVector::Zero())
        , Radius(InRadius)
    {
    }

    FKSphereElem(const FVector& InCenter, float InRadius)
        : FKShapeElem(EAggCollisionShape::Sphere)
        , Center(InCenter)
        , Radius(InRadius)
    {
    }

    // 복사 생성자
    FKSphereElem(const FKSphereElem& Other)
        : FKShapeElem(Other)
        , Center(Other.Center)
        , Radius(Other.Radius)
    {
    }

    virtual ~FKSphereElem() = default;

    // 대입 연산자
    FKSphereElem& operator=(const FKSphereElem& Other)
    {
        if (this != &Other)
        {
            FKShapeElem::operator=(Other);
            Center = Other.Center;
            Radius = Other.Radius;
        }
        return *this;
    }

    // 비교 연산자
    friend bool operator==(const FKSphereElem& LHS, const FKSphereElem& RHS)
    {
        return LHS.Center == RHS.Center && LHS.Radius == RHS.Radius;
    }

    friend bool operator!=(const FKSphereElem& LHS, const FKSphereElem& RHS)
    {
        return !(LHS == RHS);
    }

    // FKShapeElem 가상 함수 구현
    virtual FTransform GetTransform() const override
    {
        return FTransform(Center, FQuat::Identity(), FVector::One());
    }

    void SetTransform(const FTransform& InTransform)
    {
        Center = InTransform.Translation;
    }

    // 유틸리티 함수
    float GetVolume() const
    {
        // V = (4/3) * PI * r^3
        return (4.0f / 3.0f) * PI * Radius * Radius * Radius;
    }

    // Scale3D에서 가장 작은 절대값 반환
    static float GetAbsMin(const FVector& V)
    {
        return FMath::Min(FMath::Min(FMath::Abs(V.X), FMath::Abs(V.Y)), FMath::Abs(V.Z));
    }

    float GetScaledVolume(const FVector& Scale3D) const
    {
        float ScaledRadius = Radius * GetAbsMin(Scale3D);
        return (4.0f / 3.0f) * PI * ScaledRadius * ScaledRadius * ScaledRadius;
    }

    // 스케일 적용된 Sphere 반환
    FKSphereElem GetFinalScaled(const FVector& Scale3D, const FTransform& RelativeTM) const
    {
        FKSphereElem Result(*this);

        // 중심 위치 스케일 및 변환
        FVector ScaledCenter = Center * Scale3D;
        Result.Center = RelativeTM.TransformPosition(ScaledCenter);

        // 반지름은 균일 스케일만 적용
        Result.Radius = Radius * GetAbsMin(Scale3D);

        return Result;
    }

    // 점과의 최단 거리 계산
    float GetShortestDistanceToPoint(const FVector& WorldPosition, const FTransform& BodyToWorldTM) const
    {
        FVector WorldCenter = BodyToWorldTM.TransformPosition(Center);
        float Distance = (WorldPosition - WorldCenter).Size() - Radius;
        return FMath::Max(0.0f, Distance);
    }

    // 가장 가까운 점과 노멀 계산
    float GetClosestPointAndNormal(const FVector& WorldPosition, const FTransform& BodyToWorldTM,
                                   FVector& OutClosestPosition, FVector& OutNormal) const
    {
        FVector WorldCenter = BodyToWorldTM.TransformPosition(Center);
        FVector Direction = WorldPosition - WorldCenter;
        float Distance = Direction.Size();

        if (Distance < KINDA_SMALL_NUMBER)
        {
            // 점이 중심에 있으면 임의의 방향
            OutNormal = FVector(0.0f, 0.0f, 1.0f);
            OutClosestPosition = WorldCenter + OutNormal * Radius;
            return Radius;
        }

        OutNormal = Direction / Distance;
        OutClosestPosition = WorldCenter + OutNormal * Radius;

        return FMath::Max(0.0f, Distance - Radius);
    }

    // 디버그 렌더링
    virtual void DrawElemWire(FPrimitiveDrawInterface* PDI, const FTransform& ElemTM,
                              float Scale, const FLinearColor& Color) const override
    {
        if (!PDI) return;

        // ElemTM은 Body의 Transform, Center는 로컬 오프셋
        FVector WorldCenter = ElemTM.TransformPosition(Center);
        float ScaledRadius = Radius * Scale;

        PDI->DrawWireSphere(WorldCenter, ScaledRadius, FPrimitiveDrawInterface::DefaultSphereSegments, Color);
    }

    virtual void DrawElemSolid(FPrimitiveDrawInterface* PDI, const FTransform& ElemTM,
                               float Scale, const FLinearColor& Color) const override
    {
        if (!PDI) return;

        FVector WorldCenter = ElemTM.TransformPosition(Center);
        float ScaledRadius = Radius * Scale;

        PDI->DrawSolidSphere(WorldCenter, ScaledRadius, FPrimitiveDrawInterface::DefaultSphereSegments, Color);
    }
};
