#include "pch.h"
#include "DepthOfFieldComponent.h"

#include "BillboardComponent.h"
#include "World.h"
#include "Renderer.h"
#include "CameraComponent.h"

UDepthOfFieldComponent::UDepthOfFieldComponent()
{
}

void UDepthOfFieldComponent::OnRegister(UWorld* InWorld)
{
    Super::OnRegister(InWorld);

    if (!SpriteComponent && InWorld && !InWorld->bPie)
    {
        CREATE_EDITOR_COMPONENT(SpriteComponent, UBillboardComponent);
        SpriteComponent->SetTexture(GDataDir + "/UI/Icons/EmptyActor.dds");
    }
}

float UDepthOfFieldComponent::GetBlendWeight() const
{
    return FMath::Clamp(BlendWeight, 0.0f, 1.0f);
}

FDepthOfFieldSettings UDepthOfFieldComponent::GetDepthOfFieldSettings() const
{
    FDepthOfFieldSettings Settings;
    Settings.FocalDistance = ClampPositive(FocalDistance, 10.0f);
    Settings.FocalLength = ClampPositive(FocalLength, 1.0f);
    Settings.Aperture = ClampPositive(Aperture, 0.1f);
    Settings.MaxCocRadius = ClampPositive(MaxCocRadius, 0.5f);

    Settings.NearTransitionRange = FMath::Max(0.0f, NearTransitionRange);
    Settings.FarTransitionRange = FMath::Max(0.0f, FarTransitionRange);
    Settings.NearBlurScale = ClampPositive(NearBlurScale, 0.1f);
    Settings.FarBlurScale = ClampPositive(FarBlurScale, 0.1f);

    Settings.BlurSampleCount = FMath::Clamp(BlurSampleCount, 3, 128);
    Settings.BokehRotationRadians = DegreesToRadians(BokehRotationDegrees);

    return Settings;
}

// RenderDebugVolume에 사용되는 디버그 렌더용 헬퍼 함수
static void DrawDebugCircle(URenderer* Renderer, const FVector& Center, const FVector& Axis1, const FVector& Axis2, float Radius, const FVector4& Color, int32 Segments)
{
    if (!Renderer) return;

    for (int32 i = 0; i < Segments; ++i)
    {
        float Angle1 = (i / (float)Segments) * PI * 2.0f;
        float Angle2 = ((i + 1) / (float)Segments) * PI * 2.0f;
        FVector Point1 = Center + (Axis1 * cos(Angle1) + Axis2 * sin(Angle1)) * Radius;
        FVector Point2 = Center + (Axis1 * cos(Angle2) + Axis2 * sin(Angle2)) * Radius;
        Renderer->AddLine(Point1, Point2, Color);
    }
}

void UDepthOfFieldComponent::RenderDebugVolume(URenderer* Renderer) const
{
    if (!bEnableDepthOfField || !Renderer) return;

    const FVector ComponentLocation = GetWorldLocation();
    const FQuat ComponentRotation = GetWorldRotation();

    Renderer->BeginLineBatch();

    // 1. DoF Volume 박스 (bUnbounded = false일 때만)
    if (!bUnbounded)
    {
        // Volume 색상 (밝은 노란색)
        const FVector4 VolumeColor(1.0f, 1.0f, 0.0f, 1.0f);

        // 박스의 8개 모서리 계산
        FVector Corners[8];
        for (int32 i = 0; i < 8; ++i)
        {
            FVector LocalCorner(
                (i & 1) ? VolumeExtent.X : -VolumeExtent.X,
                (i & 2) ? VolumeExtent.Y : -VolumeExtent.Y,
                (i & 4) ? VolumeExtent.Z : -VolumeExtent.Z
            );
            Corners[i] = ComponentLocation + ComponentRotation.RotateVector(LocalCorner);
        }

        // 박스의 12개 엣지 그리기
        // 하단 4개 엣지
        Renderer->AddLine(Corners[0], Corners[1], VolumeColor);
        Renderer->AddLine(Corners[1], Corners[3], VolumeColor);
        Renderer->AddLine(Corners[3], Corners[2], VolumeColor);
        Renderer->AddLine(Corners[2], Corners[0], VolumeColor);

        // 상단 4개 엣지
        Renderer->AddLine(Corners[4], Corners[5], VolumeColor);
        Renderer->AddLine(Corners[5], Corners[7], VolumeColor);
        Renderer->AddLine(Corners[7], Corners[6], VolumeColor);
        Renderer->AddLine(Corners[6], Corners[4], VolumeColor);

        // 수직 4개 엣지
        Renderer->AddLine(Corners[0], Corners[4], VolumeColor);
        Renderer->AddLine(Corners[1], Corners[5], VolumeColor);
        Renderer->AddLine(Corners[2], Corners[6], VolumeColor);
        Renderer->AddLine(Corners[3], Corners[7], VolumeColor);
    }

    // 2. 초점 거리 구체들
    const FVector SphereCenter = ComponentLocation;
    const int32 Segments = 32;

    // 2-1. 초점면 (Focal Distance) - 녹색, 가장 선명
    {
        const FVector4 FocalSphereColor(0.0f, 1.0f, 0.0f, 1.0f); // 진한 녹색
        DrawDebugCircle(Renderer, SphereCenter, FVector(1.0f, 0.0f, 0.0f), FVector(0.0f, 1.0f, 0.0f), FocalDistance, FocalSphereColor, Segments);
        DrawDebugCircle(Renderer, SphereCenter, FVector(0.0f, 1.0f, 0.0f), FVector(0.0f, 0.0f, 1.0f), FocalDistance, FocalSphereColor, Segments);
        DrawDebugCircle(Renderer, SphereCenter, FVector(1.0f, 0.0f, 0.0f), FVector(0.0f, 0.0f, 1.0f), FocalDistance, FocalSphereColor, Segments);
    }

    // 2-2. Near Transition 시작 (반투명 파란색)
    if (NearTransitionRange > 0.0f)
    {
        const FVector4 NearColor(0.3f, 0.5f, 1.0f, 0.5f); // 연한 파란색
        const float NearRadius = FMath::Max(0.0f, FocalDistance - NearTransitionRange);

        if (NearRadius > 0.0f)
        {
            DrawDebugCircle(Renderer, SphereCenter, FVector(1.0f, 0.0f, 0.0f), FVector(0.0f, 1.0f, 0.0f), NearRadius, NearColor, Segments);
            DrawDebugCircle(Renderer, SphereCenter, FVector(0.0f, 1.0f, 0.0f), FVector(0.0f, 0.0f, 1.0f), NearRadius, NearColor, Segments);
            DrawDebugCircle(Renderer, SphereCenter, FVector(1.0f, 0.0f, 0.0f), FVector(0.0f, 0.0f, 1.0f), NearRadius, NearColor, Segments);
        }
    }

    // 2-3. Far Transition 끝 (반투명 주황색)
    if (FarTransitionRange > 0.0f)
    {
        const FVector4 FarColor(1.0f, 0.5f, 0.0f, 0.5f); // 연한 주황색
        const float FarRadius = FocalDistance + FarTransitionRange;

        DrawDebugCircle(Renderer, SphereCenter, FVector(1.0f, 0.0f, 0.0f), FVector(0.0f, 1.0f, 0.0f), FarRadius, FarColor, Segments);
        DrawDebugCircle(Renderer, SphereCenter, FVector(0.0f, 1.0f, 0.0f), FVector(0.0f, 0.0f, 1.0f), FarRadius, FarColor, Segments);
        DrawDebugCircle(Renderer, SphereCenter, FVector(1.0f, 0.0f, 0.0f), FVector(0.0f, 0.0f, 1.0f), FarRadius, FarColor, Segments);
    }

    FMatrix IdentityMatrix = FMatrix::Identity();
    Renderer->EndLineBatch(IdentityMatrix);
}

bool UDepthOfFieldComponent::IsInsideVolume(const FVector& TestLocation) const
{
    // bUnbounded가 true면 항상 적용
    if (bUnbounded) { return true; }

    // AABB 체크: 테스트 위치가 박스 볼륨 안에 있는지 확인
    FVector ComponentLocation = GetWorldLocation();
    FVector Delta = TestLocation - ComponentLocation;

    return (abs(Delta.X) <= VolumeExtent.X) &&
           (abs(Delta.Y) <= VolumeExtent.Y) &&
           (abs(Delta.Z) <= VolumeExtent.Z);
}

float UDepthOfFieldComponent::ClampPositive(float Value, float DefaultValue) const
{
    return (Value > 0.0f) ? Value : DefaultValue;
}
