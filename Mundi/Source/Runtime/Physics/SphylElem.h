#pragma once
#include "ShapeElem.h"
#include "PrimitiveDrawInterface.h"
#include "FKSphylElem.generated.h"

/** 충돌용 Capsule Shape (Z축이 캡슐 축) */
USTRUCT(DisplayName="Sphyl Element", Description="충돌용 캡슐 Shape")
struct FKSphylElem : public FKShapeElem
{
    GENERATED_REFLECTION_BODY()

public:
    // 정적 Shape 타입
    static constexpr EAggCollisionShape StaticShapeType = EAggCollisionShape::Sphyl;

    // 데이터 멤버
    UPROPERTY(EditAnywhere, Category="Shape")
    FVector Center;     // 중심 위치

    UPROPERTY(EditAnywhere, Category="Shape")
    FQuat Rotation;     // 회전

    UPROPERTY(EditAnywhere, Category="Shape")
    float Radius;       // 반지름

    UPROPERTY(EditAnywhere, Category="Shape")
    float Length;       // 원통 부분 길이 (반구 제외, 전체 높이 = Length + 2*Radius)

    // 생성자
    FKSphylElem()
        : FKShapeElem(EAggCollisionShape::Sphyl)
        , Center(FVector::Zero())
        , Rotation(FQuat::Identity())
        , Radius(1.0f)
        , Length(1.0f)
    {
    }

    FKSphylElem(float InRadius, float InLength)
        : FKShapeElem(EAggCollisionShape::Sphyl)
        , Center(FVector::Zero())
        , Rotation(FQuat::Identity())
        , Radius(InRadius)
        , Length(InLength)
    {
    }

    FKSphylElem(const FVector& InCenter, const FQuat& InRotation, float InRadius, float InLength)
        : FKShapeElem(EAggCollisionShape::Sphyl)
        , Center(InCenter)
        , Rotation(InRotation)
        , Radius(InRadius)
        , Length(InLength)
    {
    }

    // 복사 생성자
    FKSphylElem(const FKSphylElem& Other)
        : FKShapeElem(Other)
        , Center(Other.Center)
        , Rotation(Other.Rotation)
        , Radius(Other.Radius)
        , Length(Other.Length)
    {
    }

    virtual ~FKSphylElem() = default;

    // 대입 연산자
    FKSphylElem& operator=(const FKSphylElem& Other)
    {
        if (this != &Other)
        {
            FKShapeElem::operator=(Other);
            Center = Other.Center;
            Rotation = Other.Rotation;
            Radius = Other.Radius;
            Length = Other.Length;
        }
        return *this;
    }

    // 비교 연산자
    friend bool operator==(const FKSphylElem& LHS, const FKSphylElem& RHS)
    {
        return LHS.Center == RHS.Center &&
               LHS.Rotation == RHS.Rotation &&
               FMath::Abs(LHS.Radius - RHS.Radius) < KINDA_SMALL_NUMBER &&
               FMath::Abs(LHS.Length - RHS.Length) < KINDA_SMALL_NUMBER;
    }

    friend bool operator!=(const FKSphylElem& LHS, const FKSphylElem& RHS)
    {
        return !(LHS == RHS);
    }

    // FKShapeElem 가상 함수 구현
    virtual FTransform GetTransform() const override
    {
        return FTransform(Center, Rotation, FVector::One());
    }

    void SetTransform(const FTransform& InTransform)
    {
        Center = InTransform.Translation;
        Rotation = InTransform.Rotation;
    }

    // 전체 높이 (원통 + 양쪽 반구)
    float GetTotalHeight() const
    {
        return Length + 2.0f * Radius;
    }

    // 원통 부분의 Half Length
    float GetHalfLength() const
    {
        return Length * 0.5f;
    }

    // Scale3D에서 가장 작은 절대값 반환 (XY 평면용)
    static float GetAbsMinXY(const FVector& V)
    {
        return FMath::Min(FMath::Abs(V.X), FMath::Abs(V.Y));
    }

    // 스케일된 반지름 (XY 평면 스케일 사용)
    float GetScaledRadius(const FVector& Scale3D) const
    {
        return Radius * GetAbsMinXY(Scale3D);
    }

    // 스케일된 원통 길이 (Z축 스케일 사용)
    float GetScaledCylinderLength(const FVector& Scale3D) const
    {
        return Length * FMath::Abs(Scale3D.Z);
    }

    // 스케일된 Half Length
    float GetScaledHalfLength(const FVector& Scale3D) const
    {
        return GetScaledCylinderLength(Scale3D) * 0.5f;
    }

    // 유틸리티 함수
    float GetVolume() const
    {
        // V = PI * r^2 * (4/3 * r + Length)
        // = 구 볼륨 + 원통 볼륨
        return PI * Radius * Radius * ((4.0f / 3.0f) * Radius + Length);
    }

    float GetScaledVolume(const FVector& Scale3D) const
    {
        float ScaledRadius = GetScaledRadius(Scale3D);
        float ScaledLength = GetScaledCylinderLength(Scale3D);
        return PI * ScaledRadius * ScaledRadius * ((4.0f / 3.0f) * ScaledRadius + ScaledLength);
    }

    // 스케일 적용된 Capsule 반환
    FKSphylElem GetFinalScaled(const FVector& Scale3D, const FTransform& RelativeTM) const
    {
        FKSphylElem Result(*this);

        // 중심 위치 스케일 및 변환
        FVector ScaledCenter = Center * Scale3D;
        Result.Center = RelativeTM.TransformPosition(ScaledCenter);

        // 회전 결합
        Result.Rotation = RelativeTM.Rotation * Rotation;

        // 반지름은 XY 평면의 균일 스케일
        Result.Radius = GetScaledRadius(Scale3D);

        // 길이는 Z축 스케일
        Result.Length = GetScaledCylinderLength(Scale3D);

        return Result;
    }

    // 점과의 최단 거리 계산
    float GetShortestDistanceToPoint(const FVector& WorldPosition, const FTransform& BodyToWorldTM) const
    {
        // Capsule 로컬 공간으로 변환
        FTransform CapsuleWorldTM = BodyToWorldTM * GetTransform();
        FTransform InvCapsuleTM = CapsuleWorldTM.Inverse();
        FVector LocalPoint = InvCapsuleTM.TransformPosition(WorldPosition);

        // Capsule 중심선(Z축)상의 가장 가까운 점
        float HalfLen = GetHalfLength();
        float ClampedZ = FMath::Clamp(LocalPoint.Z, -HalfLen, HalfLen);
        FVector ClosestOnAxis(0.0f, 0.0f, ClampedZ);

        // 축에서 점까지의 거리에서 반지름을 뺌
        float Distance = (LocalPoint - ClosestOnAxis).Size() - Radius;
        return FMath::Max(0.0f, Distance);
    }

    // 가장 가까운 점과 노멀 계산
    float GetClosestPointAndNormal(const FVector& WorldPosition, const FTransform& BodyToWorldTM,
                                   FVector& OutClosestPosition, FVector& OutNormal) const
    {
        // Capsule 로컬 공간으로 변환
        FTransform CapsuleWorldTM = BodyToWorldTM * GetTransform();
        FTransform InvCapsuleTM = CapsuleWorldTM.Inverse();
        FVector LocalPoint = InvCapsuleTM.TransformPosition(WorldPosition);

        // Capsule 중심선(Z축)상의 가장 가까운 점
        float HalfLen = GetHalfLength();
        float ClampedZ = FMath::Clamp(LocalPoint.Z, -HalfLen, HalfLen);
        FVector ClosestOnAxis(0.0f, 0.0f, ClampedZ);

        // 축에서 점 방향으로의 벡터
        FVector ToPoint = LocalPoint - ClosestOnAxis;
        float DistToAxis = ToPoint.Size();

        FVector LocalNormal;
        FVector LocalClosest;

        if (DistToAxis < KINDA_SMALL_NUMBER)
        {
            // 점이 축 위에 있으면 임의의 방향 (X축)
            LocalNormal = FVector(1.0f, 0.0f, 0.0f);
            LocalClosest = ClosestOnAxis + LocalNormal * Radius;
        }
        else
        {
            LocalNormal = ToPoint / DistToAxis;
            LocalClosest = ClosestOnAxis + LocalNormal * Radius;
        }

        float Distance = FMath::Max(0.0f, DistToAxis - Radius);

        // 월드 공간으로 변환
        OutClosestPosition = CapsuleWorldTM.TransformPosition(LocalClosest);
        OutNormal = CapsuleWorldTM.TransformVector(LocalNormal).GetNormalized();

        return Distance;
    }

    // 디버그 렌더링
    virtual void DrawElemWire(FPrimitiveDrawInterface* PDI, const FTransform& ElemTM,
                              float Scale, const FLinearColor& Color) const override
    {
        if (!PDI) return;

        // 캡슐의 World Transform
        FVector WorldCenter = ElemTM.TransformPosition(Center);
        FQuat WorldRotation = ElemTM.Rotation * Rotation;

        // 스케일 적용
        float ScaledRadius = Radius * Scale;
        float ScaledHalfHeight = GetHalfLength() * Scale;  // Length/2

        PDI->DrawWireCapsule(WorldCenter, WorldRotation, ScaledRadius, ScaledHalfHeight,
                             FPrimitiveDrawInterface::DefaultSphereSegments, Color);
    }

    virtual void DrawElemSolid(FPrimitiveDrawInterface* PDI, const FTransform& ElemTM,
                               float Scale, const FLinearColor& Color) const override
    {
        if (!PDI) return;

        FTransform WorldTM;
        WorldTM.Translation = ElemTM.TransformPosition(Center);
        WorldTM.Rotation = ElemTM.Rotation * Rotation;
        WorldTM.Scale3D = FVector::One();

        float ScaledRadius = Radius * Scale;
        float ScaledHalfHeight = GetHalfLength() * Scale;

        PDI->DrawSolidCapsule(WorldTM, ScaledRadius, ScaledHalfHeight,
                              FPrimitiveDrawInterface::DefaultSphereSegments, Color);
    }
};
