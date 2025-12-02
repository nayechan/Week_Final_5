#pragma once
#include "ShapeElem.h"
#include "PrimitiveDrawInterface.h"
#include "FKBoxElem.generated.h"

/** 충돌용 Box Shape */
USTRUCT(DisplayName="Box Element", Description="충돌용 박스 Shape")
struct FKBoxElem : public FKShapeElem
{
    GENERATED_REFLECTION_BODY()

public:
    // 정적 Shape 타입
    static constexpr EAggCollisionShape StaticShapeType = EAggCollisionShape::Box;

    // 데이터 멤버
    UPROPERTY(EditAnywhere, Category="Shape")
    FVector Center;     // 중심 위치

    UPROPERTY(EditAnywhere, Category="Shape")
    FQuat Rotation;     // 회전

    UPROPERTY(EditAnywhere, Category="Shape")
    float X;            // X축 Half Extent

    UPROPERTY(EditAnywhere, Category="Shape")
    float Y;            // Y축 Half Extent

    UPROPERTY(EditAnywhere, Category="Shape")
    float Z;            // Z축 Half Extent

    // 생성자
    FKBoxElem()
        : FKShapeElem(EAggCollisionShape::Box)
        , Center(FVector::Zero())
        , Rotation(FQuat::Identity())
        , X(1.0f)
        , Y(1.0f)
        , Z(1.0f)
    {
    }

    // 정육면체 생성자
    explicit FKBoxElem(float InSize)
        : FKShapeElem(EAggCollisionShape::Box)
        , Center(FVector::Zero())
        , Rotation(FQuat::Identity())
        , X(InSize)
        , Y(InSize)
        , Z(InSize)
    {
    }

    // 직육면체 생성자
    FKBoxElem(float InX, float InY, float InZ)
        : FKShapeElem(EAggCollisionShape::Box)
        , Center(FVector::Zero())
        , Rotation(FQuat::Identity())
        , X(InX)
        , Y(InY)
        , Z(InZ)
    {
    }

    // 전체 파라미터 생성자
    FKBoxElem(const FVector& InCenter, const FQuat& InRotation, float InX, float InY, float InZ)
        : FKShapeElem(EAggCollisionShape::Box)
        , Center(InCenter)
        , Rotation(InRotation)
        , X(InX)
        , Y(InY)
        , Z(InZ)
    {
    }

    // 복사 생성자
    FKBoxElem(const FKBoxElem& Other)
        : FKShapeElem(Other)
        , Center(Other.Center)
        , Rotation(Other.Rotation)
        , X(Other.X)
        , Y(Other.Y)
        , Z(Other.Z)
    {
    }

    virtual ~FKBoxElem() = default;

    // 대입 연산자
    FKBoxElem& operator=(const FKBoxElem& Other)
    {
        if (this != &Other)
        {
            FKShapeElem::operator=(Other);
            Center = Other.Center;
            Rotation = Other.Rotation;
            X = Other.X;
            Y = Other.Y;
            Z = Other.Z;
        }
        return *this;
    }

    // 비교 연산자
    friend bool operator==(const FKBoxElem& LHS, const FKBoxElem& RHS)
    {
        return LHS.Center == RHS.Center &&
               LHS.Rotation == RHS.Rotation &&
               FMath::Abs(LHS.X - RHS.X) < KINDA_SMALL_NUMBER &&
               FMath::Abs(LHS.Y - RHS.Y) < KINDA_SMALL_NUMBER &&
               FMath::Abs(LHS.Z - RHS.Z) < KINDA_SMALL_NUMBER;
    }

    friend bool operator!=(const FKBoxElem& LHS, const FKBoxElem& RHS)
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

    // Half Extent 반환
    FVector GetHalfExtent() const
    {
        return FVector(X, Y, Z);
    }

    void SetHalfExtent(const FVector& HalfExtent)
    {
        X = HalfExtent.X;
        Y = HalfExtent.Y;
        Z = HalfExtent.Z;
    }

    // 유틸리티 함수
    float GetVolume() const
    {
        // V = 8 * X * Y * Z (Half Extent이므로 8배)
        return 8.0f * X * Y * Z;
    }

    float GetScaledVolume(const FVector& Scale3D) const
    {
        return FMath::Abs(Scale3D.X * Scale3D.Y * Scale3D.Z) * GetVolume();
    }

    // 스케일 적용된 Box 반환
    FKBoxElem GetFinalScaled(const FVector& Scale3D, const FTransform& RelativeTM) const
    {
        FKBoxElem Result(*this);

        // 중심 위치 스케일 및 변환
        FVector ScaledCenter = Center * Scale3D;
        Result.Center = RelativeTM.TransformPosition(ScaledCenter);

        // 회전 결합
        Result.Rotation = RelativeTM.Rotation * Rotation;

        // Half Extent 스케일
        Result.X = X * FMath::Abs(Scale3D.X);
        Result.Y = Y * FMath::Abs(Scale3D.Y);
        Result.Z = Z * FMath::Abs(Scale3D.Z);

        return Result;
    }

    // 점과의 최단 거리 계산
    float GetShortestDistanceToPoint(const FVector& WorldPosition, const FTransform& BodyToWorldTM) const
    {
        // Box 로컬 공간으로 변환
        FTransform BoxWorldTM = BodyToWorldTM * GetTransform();
        FTransform InvBoxTM = BoxWorldTM.Inverse();
        FVector LocalPoint = InvBoxTM.TransformPosition(WorldPosition);

        // Box 표면까지의 거리 계산
        FVector ClosestPoint;
        ClosestPoint.X = FMath::Clamp(LocalPoint.X, -X, X);
        ClosestPoint.Y = FMath::Clamp(LocalPoint.Y, -Y, Y);
        ClosestPoint.Z = FMath::Clamp(LocalPoint.Z, -Z, Z);

        return (LocalPoint - ClosestPoint).Size();
    }

    // 가장 가까운 점과 노멀 계산
    float GetClosestPointAndNormal(const FVector& WorldPosition, const FTransform& BodyToWorldTM,
                                   FVector& OutClosestPosition, FVector& OutNormal) const
    {
        // Box 로컬 공간으로 변환
        FTransform BoxWorldTM = BodyToWorldTM * GetTransform();
        FTransform InvBoxTM = BoxWorldTM.Inverse();
        FVector LocalPoint = InvBoxTM.TransformPosition(WorldPosition);

        // Box 표면의 가장 가까운 점 계산
        FVector LocalClosest;
        LocalClosest.X = FMath::Clamp(LocalPoint.X, -X, X);
        LocalClosest.Y = FMath::Clamp(LocalPoint.Y, -Y, Y);
        LocalClosest.Z = FMath::Clamp(LocalPoint.Z, -Z, Z);

        FVector LocalDiff = LocalPoint - LocalClosest;
        float Distance = LocalDiff.Size();

        // 노멀 계산
        FVector LocalNormal;
        if (Distance < KINDA_SMALL_NUMBER)
        {
            // 점이 Box 내부에 있으면 가장 가까운 면의 노멀 사용
            float MinDist = X - FMath::Abs(LocalPoint.X);
            LocalNormal = FVector(LocalPoint.X > 0 ? 1.0f : -1.0f, 0.0f, 0.0f);

            float DistY = Y - FMath::Abs(LocalPoint.Y);
            if (DistY < MinDist)
            {
                MinDist = DistY;
                LocalNormal = FVector(0.0f, LocalPoint.Y > 0 ? 1.0f : -1.0f, 0.0f);
            }

            float DistZ = Z - FMath::Abs(LocalPoint.Z);
            if (DistZ < MinDist)
            {
                LocalNormal = FVector(0.0f, 0.0f, LocalPoint.Z > 0 ? 1.0f : -1.0f);
            }
        }
        else
        {
            LocalNormal = LocalDiff / Distance;
        }

        // 월드 공간으로 변환
        OutClosestPosition = BoxWorldTM.TransformPosition(LocalClosest);
        OutNormal = BoxWorldTM.TransformVector(LocalNormal).GetNormalized();

        return Distance;
    }

    // 디버그 렌더링
    virtual void DrawElemWire(FPrimitiveDrawInterface* PDI, const FTransform& ElemTM,
                              float Scale, const FLinearColor& Color) const override
    {
        if (!PDI) return;

        // Box의 로컬 Transform (Center + Rotation)
        FTransform LocalTM(Center, Rotation, FVector::One());

        // ElemTM (Body Transform) * LocalTM (Shape Transform)
        FTransform WorldTM;
        WorldTM.Translation = ElemTM.TransformPosition(Center);
        WorldTM.Rotation = ElemTM.Rotation * Rotation;
        WorldTM.Scale3D = ElemTM.Scale3D;

        // 스케일 적용된 Half Extent
        FVector ScaledExtent(X * Scale, Y * Scale, Z * Scale);

        PDI->DrawWireBox(WorldTM, ScaledExtent, Color);
    }

    virtual void DrawElemSolid(FPrimitiveDrawInterface* PDI, const FTransform& ElemTM,
                               float Scale, const FLinearColor& Color) const override
    {
        if (!PDI) return;

        FTransform WorldTM;
        WorldTM.Translation = ElemTM.TransformPosition(Center);
        WorldTM.Rotation = ElemTM.Rotation * Rotation;
        WorldTM.Scale3D = ElemTM.Scale3D;

        FVector ScaledExtent(X * Scale, Y * Scale, Z * Scale);

        PDI->DrawSolidBox(WorldTM, ScaledExtent, Color);
    }
};
