#include "pch.h"
#include "SphereComponent.h"
#include "Renderer.h"
#include "Actor.h"

IMPLEMENT_CLASS(USphereComponent)

BEGIN_PROPERTIES(USphereComponent)
	MARK_AS_COMPONENT("구 충돌 컴포넌트", "구 크기의 충돌체를 생성하는 컴포넌트입니다.")
	ADD_PROPERTY(float, SphereRadius, "SphereRaidus", true, "구 충돌체의 반지름입니다.")
END_PROPERTIES()

USphereComponent::USphereComponent()
{

}

void USphereComponent::OnRegister(UWorld* InWorld)
{
    Super::OnRegister(InWorld);

    if (SphereRadius == 0)
    {
        SphereRadius = FMath::Max(WorldAABB.GetHalfExtent().X, WorldAABB.GetHalfExtent().Y, WorldAABB.GetHalfExtent().Z);
    }
}

void USphereComponent::DuplicateSubObjects()
{
    Super::DuplicateSubObjects();
}

void USphereComponent::GetShape(FShape& Out) const
{
    Out.Kind = EShapeKind::Sphere;
    Out.Sphere.SphereRadius = SphereRadius;	
}

void USphereComponent::RenderDebugVolume(URenderer* Renderer) const
{
    if (!bShapeIsVisible) return;
    if (GetOwner() && (GetOwner()->GetWorld()->bPie))
    {
        if (bShapeHiddenInGame)
            return;
    }
    // Draw three great circles to visualize the sphere (XY, XZ, YZ planes) 
    const FVector Center = GetWorldLocation();
    const float Radius = SphereRadius;
    const int NumSegments = 16;

    TArray<FVector> StartPoints;
    TArray<FVector> EndPoints;
    TArray<FVector4> Colors;

    // XY circle (Z fixed)
    for (int i = 0; i < NumSegments; ++i)
    {
        const float a0 = (static_cast<float>(i) / NumSegments) * TWO_PI;
        const float a1 = (static_cast<float>((i + 1) % NumSegments) / NumSegments) * TWO_PI;

        const FVector p0 = Center + FVector(Radius * std::cos(a0), Radius * std::sin(a0), 0.0f);
        const FVector p1 = Center + FVector(Radius * std::cos(a1), Radius * std::sin(a1), 0.0f);

        StartPoints.Add(p0);
        EndPoints.Add(p1);
        Colors.Add(ShapeColor);
    }

    // XZ circle (Y fixed)
    for (int i = 0; i < NumSegments; ++i)
    {
        const float a0 = (static_cast<float>(i) / NumSegments) * TWO_PI;
        const float a1 = (static_cast<float>((i + 1) % NumSegments) / NumSegments) * TWO_PI;

        const FVector p0 = Center + FVector(Radius * std::cos(a0), 0.0f, Radius * std::sin(a0));
        const FVector p1 = Center + FVector(Radius * std::cos(a1), 0.0f, Radius * std::sin(a1));

        StartPoints.Add(p0);
        EndPoints.Add(p1);
        Colors.Add(ShapeColor);
    }

    // YZ circle (X fixed)
    for (int i = 0; i < NumSegments; ++i)
    {
        const float a0 = (static_cast<float>(i) / NumSegments) * TWO_PI;
        const float a1 = (static_cast<float>((i + 1) % NumSegments) / NumSegments) * TWO_PI;

        const FVector p0 = Center + FVector(0.0f, Radius * std::cos(a0), Radius * std::sin(a0));
        const FVector p1 = Center + FVector(0.0f, Radius * std::cos(a1), Radius * std::sin(a1));

        StartPoints.Add(p0);
        EndPoints.Add(p1);
        Colors.Add(ShapeColor);
    }

    Renderer->AddLines(StartPoints, EndPoints, Colors);
}
