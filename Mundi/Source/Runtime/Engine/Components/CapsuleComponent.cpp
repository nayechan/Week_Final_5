#include "pch.h"
#include "CapsuleComponent.h"
// IMPLEMENT_CLASS is now auto-generated in .generated.cpp
//BEGIN_PROPERTIES(UCapsuleComponent)
//MARK_AS_COMPONENT("캡슐 충돌 컴포넌트", "캡슐 모양의 충돌체를 생성하는 컴포넌트입니다.")
//ADD_PROPERTY(float , CapsuleHalfHeight, "CapsuleHalfHeight", true, "박스 충돌체의 크기입니다.")
//ADD_PROPERTY(float , CapsuleRadius, "CapsuleHalfHeight", true, "박스 충돌체의 크기입니다.")
//END_PROPERTIES()

UCapsuleComponent::UCapsuleComponent()
{
    CapsuleHalfHeight = 0.5f;
    CapsuleRadius = 0.5f;

	//CapsuleHalfHeight = WorldAABB.GetHalfExtent().Z;
	//CapsuleRadius = FMath::Max(WorldAABB.GetHalfExtent().X, WorldAABB.GetHalfExtent().Y);
}

void UCapsuleComponent::OnRegister(UWorld* World)
{
	//Super::OnRegister(World);
    //
	//CapsuleHalfHeight = WorldAABB.GetHalfExtent().Z ;
	//CapsuleRadius = WorldAABB.GetHalfExtent().X < WorldAABB.GetHalfExtent().Y ? WorldAABB.GetHalfExtent().Y : WorldAABB.GetHalfExtent().X;
    //CapsuleRadius *= 2;


    Super::OnRegister(World);

    if (AActor* Owner = GetOwner())
    {
        FAABB ActorBounds = Owner->GetBounds();
        FVector WorldHalfExtent = ActorBounds.GetHalfExtent();

        // World scale로 나눠서 local 값 계산
        const FTransform WorldTransform = GetWorldTransform();
        const FVector S = FVector(
            std::fabs(WorldTransform.Scale3D.X),
            std::fabs(WorldTransform.Scale3D.Y),
            std::fabs(WorldTransform.Scale3D.Z)
        );

        constexpr float Eps = 1e-6f;

        // Z축 = 높이, XY축 = 반지름
        float LocalHalfHeight = S.Z > Eps ? WorldHalfExtent.Z / S.Z : WorldHalfExtent.Z;
        float LocalRadiusX = S.X > Eps ? WorldHalfExtent.X / S.X : WorldHalfExtent.X;
        float LocalRadiusY = S.Y > Eps ? WorldHalfExtent.Y / S.Y : WorldHalfExtent.Y;

        CapsuleHalfHeight = LocalHalfHeight;
        CapsuleRadius = FMath::Max(LocalRadiusX, LocalRadiusY); 
    }

}

void UCapsuleComponent::DuplicateSubObjects()
{
    Super::DuplicateSubObjects();
}

void UCapsuleComponent::GetShape(FShape& Out) const
{
	Out.Kind = EShapeKind::Capsule;
	Out.Capsule.CapsuleHalfHeight = CapsuleHalfHeight;
	Out.Capsule.CapsuleRadius = CapsuleRadius;
}

void UCapsuleComponent::RenderDebugVolume(URenderer* Renderer) const
{
    if (!bShapeIsVisible) return;
    if (GetOwner() && (GetOwner()->GetWorld()->bPie))
    {
        if (bShapeHiddenInGame)
            return;
    }

    // Compute cylinder half-length (halfHeight in AABB includes radius)
    const float Radius = CapsuleRadius;
    const float HalfHeightAABB = CapsuleHalfHeight;
    const float HalfHeightCylinder = FMath::Max(0.0f, HalfHeightAABB - Radius *0.5f);
     

    // 위치, 회전 변환만 가져와서 사용, Scale은 사용자가 조정 
    const FMatrix WorldNoScale = FMatrix::FromTRS(GetWorldLocation(), 
        GetWorldRotation(), FVector(1.0f, 1.0f, 1.0f));
     
    const int NumOfSphereSlice = 4;
    const int NumHemisphereSegments = 8; 

    TArray<FVector> StartPoints;
    TArray<FVector> EndPoints;
    TArray<FVector4> Colors;

    TArray<FVector> TopRingLocal;
    TArray<FVector> BottomRingLocal;
    TopRingLocal.Reserve(NumOfSphereSlice);
    BottomRingLocal.Reserve(NumOfSphereSlice);

     
    //윗면 아랫면 
    for (int i = 0; i < NumOfSphereSlice; ++i)
    {
        const float a0 = (static_cast<float>(i) / NumOfSphereSlice) * TWO_PI;
        const float x = Radius * std::sin(a0);
        const float y = Radius * std::cos(a0);
        TopRingLocal.Add(FVector(x, y, +HalfHeightCylinder));
        BottomRingLocal.Add(FVector(x, y, -HalfHeightCylinder));
    }
     
    for (int i = 0; i < NumOfSphereSlice; ++i)
    {
        const int j = (i + 1) % NumOfSphereSlice;

        //윗면
        StartPoints.Add(TopRingLocal[i] * WorldNoScale);
        EndPoints.Add(TopRingLocal[j] * WorldNoScale);
        Colors.Add(ShapeColor);

        // 아랫면
        StartPoints.Add(BottomRingLocal[i] * WorldNoScale);
        EndPoints.Add(BottomRingLocal[j] * WorldNoScale);
        Colors.Add(ShapeColor);
    }
     
    //윗면 아랫면 잇는 선분
    for (int i = 0; i < NumOfSphereSlice; ++i)
    {
        StartPoints.Add(TopRingLocal[i] * WorldNoScale);
        EndPoints.Add(BottomRingLocal[i] * WorldNoScale);
        Colors.Add(ShapeColor);
    }
    
    // 반구 위아래 
    auto AddHemisphereArcs = [&](float CenterZSign)
    {
        const float CenterZ = CenterZSign * HalfHeightCylinder;

        for (int i = 0; i < NumHemisphereSegments; ++i)
        {
            const float t0 = (static_cast<float>(i) / NumHemisphereSegments) * PI;
            const float t1 = (static_cast<float>(i + 1) / NumHemisphereSegments) * PI;
    
            FVector PlaneXZ0(Radius * std::cos(t0), 0.0f, CenterZ + CenterZSign* Radius * std::sin(t0));
            FVector PlaneXZ1(Radius * std::cos(t1), 0.0f, CenterZ + CenterZSign* Radius * std::sin(t1));
            
            StartPoints.Add(PlaneXZ0 * WorldNoScale);
            EndPoints.Add(PlaneXZ1 * WorldNoScale);
            Colors.Add(ShapeColor);
            
            FVector PlaneYZ0(0.0f, Radius * std::cos(t0), CenterZ + CenterZSign * Radius * std::sin(t0));
            FVector PlaneYZ1(0.0f, Radius * std::cos(t1), CenterZ + CenterZSign * Radius * std::sin(t1));

            StartPoints.Add(PlaneYZ0 * WorldNoScale);
            EndPoints.Add(PlaneYZ1 * WorldNoScale);
            Colors.Add(ShapeColor);
        }
    };
     
    AddHemisphereArcs(+1.0f);
    AddHemisphereArcs(-1.0f);

    Renderer->AddLines(StartPoints, EndPoints, Colors);
}
