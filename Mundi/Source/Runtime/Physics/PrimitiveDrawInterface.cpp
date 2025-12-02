#include "pch.h"
#include "PrimitiveDrawInterface.h"
#include "LineComponent.h"
#include "Math.h"

FPrimitiveDrawInterface::FPrimitiveDrawInterface()
    : LineComp(nullptr)
{
}

FPrimitiveDrawInterface::~FPrimitiveDrawInterface()
{
}

void FPrimitiveDrawInterface::Initialize(ULineComponent* InLineComp)
{
    LineComp = InLineComp;
}

void FPrimitiveDrawInterface::Clear()
{
    if (LineComp)
    {
        LineComp->ClearLines();
    }
}

// =====================================================
// Wire 프리미티브
// =====================================================

void FPrimitiveDrawInterface::DrawLine(const FVector& Start, const FVector& End, const FLinearColor& Color)
{
    if (!LineComp) return;
    LineComp->AddLine(Start, End, FVector4(Color.R, Color.G, Color.B, Color.A));
}

void FPrimitiveDrawInterface::DrawCircle(const FVector& Center, const FVector& XAxis, const FVector& YAxis,
                                          float Radius, int32 Segments, const FLinearColor& Color)
{
    if (!LineComp || Segments < 3) return;

    const float AngleStep = 2.0f * PI / static_cast<float>(Segments);

    FVector PrevPoint = Center + XAxis * Radius;
    for (int32 i = 1; i <= Segments; ++i)
    {
        float Angle = AngleStep * i;
        FVector CurrPoint = Center + XAxis * (Radius * std::cos(Angle)) + YAxis * (Radius * std::sin(Angle));
        DrawLine(PrevPoint, CurrPoint, Color);
        PrevPoint = CurrPoint;
    }
}

void FPrimitiveDrawInterface::DrawCircleXY(const FVector& Center, float Radius, int32 Segments, const FLinearColor& Color)
{
    DrawCircle(Center, FVector(1, 0, 0), FVector(0, 1, 0), Radius, Segments, Color);
}

void FPrimitiveDrawInterface::DrawCircleXZ(const FVector& Center, float Radius, int32 Segments, const FLinearColor& Color)
{
    DrawCircle(Center, FVector(1, 0, 0), FVector(0, 0, 1), Radius, Segments, Color);
}

void FPrimitiveDrawInterface::DrawCircleYZ(const FVector& Center, float Radius, int32 Segments, const FLinearColor& Color)
{
    DrawCircle(Center, FVector(0, 1, 0), FVector(0, 0, 1), Radius, Segments, Color);
}

void FPrimitiveDrawInterface::DrawWireSphere(const FVector& Center, float Radius, int32 Segments, const FLinearColor& Color)
{
    // 3개의 원으로 구를 표현
    DrawCircleXY(Center, Radius, Segments, Color);
    DrawCircleXZ(Center, Radius, Segments, Color);
    DrawCircleYZ(Center, Radius, Segments, Color);
}

void FPrimitiveDrawInterface::DrawWireSphere(const FTransform& Transform, float Radius, int32 Segments, const FLinearColor& Color)
{
    FVector Center = Transform.Translation;
    FVector XAxis = Transform.Rotation.RotateVector(FVector(1, 0, 0));
    FVector YAxis = Transform.Rotation.RotateVector(FVector(0, 1, 0));
    FVector ZAxis = Transform.Rotation.RotateVector(FVector(0, 0, 1));

    // 스케일 적용된 반지름
    float ScaledRadius = Radius * Transform.Scale3D.X;  // 균일 스케일 가정

    DrawCircle(Center, XAxis, YAxis, ScaledRadius, Segments, Color);  // XY
    DrawCircle(Center, XAxis, ZAxis, ScaledRadius, Segments, Color);  // XZ
    DrawCircle(Center, YAxis, ZAxis, ScaledRadius, Segments, Color);  // YZ
}

void FPrimitiveDrawInterface::DrawWireBox(const FVector& Center, const FVector& Extent, const FLinearColor& Color)
{
    // 8개의 꼭지점
    FVector Min = Center - Extent;
    FVector Max = Center + Extent;

    FVector V[8] = {
        FVector(Min.X, Min.Y, Min.Z),  // 0: ---
        FVector(Max.X, Min.Y, Min.Z),  // 1: +--
        FVector(Max.X, Max.Y, Min.Z),  // 2: ++-
        FVector(Min.X, Max.Y, Min.Z),  // 3: -+-
        FVector(Min.X, Min.Y, Max.Z),  // 4: --+
        FVector(Max.X, Min.Y, Max.Z),  // 5: +-+
        FVector(Max.X, Max.Y, Max.Z),  // 6: +++
        FVector(Min.X, Max.Y, Max.Z),  // 7: -++
    };

    // 12개의 에지
    // 하단 면
    DrawLine(V[0], V[1], Color);
    DrawLine(V[1], V[2], Color);
    DrawLine(V[2], V[3], Color);
    DrawLine(V[3], V[0], Color);

    // 상단 면
    DrawLine(V[4], V[5], Color);
    DrawLine(V[5], V[6], Color);
    DrawLine(V[6], V[7], Color);
    DrawLine(V[7], V[4], Color);

    // 세로 에지
    DrawLine(V[0], V[4], Color);
    DrawLine(V[1], V[5], Color);
    DrawLine(V[2], V[6], Color);
    DrawLine(V[3], V[7], Color);
}

void FPrimitiveDrawInterface::DrawWireBox(const FTransform& Transform, const FVector& Extent, const FLinearColor& Color)
{
    // 스케일 적용된 Extent
    FVector ScaledExtent = Extent * Transform.Scale3D;

    // 로컬 꼭지점
    FVector LocalV[8] = {
        FVector(-ScaledExtent.X, -ScaledExtent.Y, -ScaledExtent.Z),
        FVector(+ScaledExtent.X, -ScaledExtent.Y, -ScaledExtent.Z),
        FVector(+ScaledExtent.X, +ScaledExtent.Y, -ScaledExtent.Z),
        FVector(-ScaledExtent.X, +ScaledExtent.Y, -ScaledExtent.Z),
        FVector(-ScaledExtent.X, -ScaledExtent.Y, +ScaledExtent.Z),
        FVector(+ScaledExtent.X, -ScaledExtent.Y, +ScaledExtent.Z),
        FVector(+ScaledExtent.X, +ScaledExtent.Y, +ScaledExtent.Z),
        FVector(-ScaledExtent.X, +ScaledExtent.Y, +ScaledExtent.Z),
    };

    // 월드 변환
    FVector V[8];
    for (int32 i = 0; i < 8; ++i)
    {
        V[i] = Transform.Translation + Transform.Rotation.RotateVector(LocalV[i]);
    }

    // 12개의 에지
    DrawLine(V[0], V[1], Color);
    DrawLine(V[1], V[2], Color);
    DrawLine(V[2], V[3], Color);
    DrawLine(V[3], V[0], Color);

    DrawLine(V[4], V[5], Color);
    DrawLine(V[5], V[6], Color);
    DrawLine(V[6], V[7], Color);
    DrawLine(V[7], V[4], Color);

    DrawLine(V[0], V[4], Color);
    DrawLine(V[1], V[5], Color);
    DrawLine(V[2], V[6], Color);
    DrawLine(V[3], V[7], Color);
}

void FPrimitiveDrawInterface::DrawWireCapsule(const FVector& Center, const FQuat& Rotation, float Radius, float HalfHeight,
                                               int32 Segments, const FLinearColor& Color)
{
    // 캡슐: Z축이 캡슐 축 (Length 방향)
    // HalfHeight = 원통 부분의 반 높이 (반구 제외)

    FVector XAxis = Rotation.RotateVector(FVector(1, 0, 0));
    FVector YAxis = Rotation.RotateVector(FVector(0, 1, 0));
    FVector ZAxis = Rotation.RotateVector(FVector(0, 0, 1));

    FVector TopCenter = Center + ZAxis * HalfHeight;
    FVector BottomCenter = Center - ZAxis * HalfHeight;

    // 원통 부분: 상단/하단 원 + 4개의 세로선
    DrawCircle(TopCenter, XAxis, YAxis, Radius, Segments, Color);
    DrawCircle(BottomCenter, XAxis, YAxis, Radius, Segments, Color);

    // 4개의 세로선 (캡슐 측면)
    DrawLine(TopCenter + XAxis * Radius, BottomCenter + XAxis * Radius, Color);
    DrawLine(TopCenter - XAxis * Radius, BottomCenter - XAxis * Radius, Color);
    DrawLine(TopCenter + YAxis * Radius, BottomCenter + YAxis * Radius, Color);
    DrawLine(TopCenter - YAxis * Radius, BottomCenter - YAxis * Radius, Color);

    // 반구 (상단)
    int32 HalfSegments = Segments / 2;
    float AngleStep = PI / static_cast<float>(HalfSegments);

    // 상단 반구 - XZ 평면 반원
    FVector PrevTop = TopCenter + XAxis * Radius;
    for (int32 i = 1; i <= HalfSegments; ++i)
    {
        float Angle = AngleStep * i;
        FVector CurrTop = TopCenter + XAxis * (Radius * std::cos(Angle)) + ZAxis * (Radius * std::sin(Angle));
        DrawLine(PrevTop, CurrTop, Color);
        PrevTop = CurrTop;
    }

    // 상단 반구 - YZ 평면 반원
    PrevTop = TopCenter + YAxis * Radius;
    for (int32 i = 1; i <= HalfSegments; ++i)
    {
        float Angle = AngleStep * i;
        FVector CurrTop = TopCenter + YAxis * (Radius * std::cos(Angle)) + ZAxis * (Radius * std::sin(Angle));
        DrawLine(PrevTop, CurrTop, Color);
        PrevTop = CurrTop;
    }

    // 하단 반구 - XZ 평면 반원
    FVector PrevBottom = BottomCenter + XAxis * Radius;
    for (int32 i = 1; i <= HalfSegments; ++i)
    {
        float Angle = AngleStep * i;
        FVector CurrBottom = BottomCenter + XAxis * (Radius * std::cos(Angle)) - ZAxis * (Radius * std::sin(Angle));
        DrawLine(PrevBottom, CurrBottom, Color);
        PrevBottom = CurrBottom;
    }

    // 하단 반구 - YZ 평면 반원
    PrevBottom = BottomCenter + YAxis * Radius;
    for (int32 i = 1; i <= HalfSegments; ++i)
    {
        float Angle = AngleStep * i;
        FVector CurrBottom = BottomCenter + YAxis * (Radius * std::cos(Angle)) - ZAxis * (Radius * std::sin(Angle));
        DrawLine(PrevBottom, CurrBottom, Color);
        PrevBottom = CurrBottom;
    }
}

void FPrimitiveDrawInterface::DrawWireCapsule(const FTransform& Transform, float Radius, float HalfHeight,
                                               int32 Segments, const FLinearColor& Color)
{
    // 스케일 적용 (XY: 반지름, Z: 높이)
    float ScaledRadius = Radius * FMath::Min(Transform.Scale3D.X, Transform.Scale3D.Y);
    float ScaledHalfHeight = HalfHeight * Transform.Scale3D.Z;

    DrawWireCapsule(Transform.Translation, Transform.Rotation, ScaledRadius, ScaledHalfHeight, Segments, Color);
}

// =====================================================
// Solid 프리미티브 (추후 구현)
// =====================================================

void FPrimitiveDrawInterface::DrawSolidSphere(const FVector& Center, float Radius, int32 Segments, const FLinearColor& Color)
{
    // TODO: DynamicMesh를 사용한 구체 메시 생성
}

void FPrimitiveDrawInterface::DrawSolidBox(const FTransform& Transform, const FVector& Extent, const FLinearColor& Color)
{
    // TODO: DynamicMesh를 사용한 박스 메시 생성
}

void FPrimitiveDrawInterface::DrawSolidCapsule(const FTransform& Transform, float Radius, float HalfHeight,
                                                int32 Segments, const FLinearColor& Color)
{
    // TODO: DynamicMesh를 사용한 캡슐 메시 생성
}
