// ────────────────────────────────────────────────────────────────────────────
// BoxComponent.cpp
// Box 형태의 충돌 컴포넌트 구현 (Week09 기반, Week12 적응)
// ────────────────────────────────────────────────────────────────────────────
#include "pch.h"
#include "BoxComponent.h"
#include "SphereComponent.h"
#include "Actor.h"
#include "BodySetup.h"
#include "BoxElem.h"
#include "Sphere.h"
#include "World.h"
#include "CollisionManager.h"
#include "WorldPartitionManager.h"

// ────────────────────────────────────────────────────────────────────────────
// 생성자 / 소멸자
// ────────────────────────────────────────────────────────────────────────────

UBoxComponent::UBoxComponent()
{
	BoxExtent = FVector(0.5f, 0.5f, 0.5f);
	UpdateBounds();
}

UBoxComponent::~UBoxComponent()
{
}

void UBoxComponent::DuplicateSubObjects()
{
	Super::DuplicateSubObjects();
}

// ────────────────────────────────────────────────────────────────────────────
// Box 속성 함수
// ────────────────────────────────────────────────────────────────────────────

void UBoxComponent::SetBoxExtent(const FVector& InExtent, bool bUpdateBoundsNow)
{
	if (BoxExtent.Equals(InExtent))
	{
		return; 
	}
	
	BoxExtent = InExtent;

	if (bUpdateBoundsNow)
	{
		UpdateBounds();

		// BVH 업데이트를 위해 dirty 마킹
		if (UWorld* World = GetWorld())
		{
			if (UCollisionManager* Manager = World->GetCollisionManager())
			{
				Manager->MarkComponentDirty(this);
			}
			if (UWorldPartitionManager* Partition = World->GetPartitionManager())
			{
				Partition->MarkDirty(this);
			}
		}
		RecreatePhysicsState();
	}
}

FVector UBoxComponent::GetScaledBoxExtent() const
{
	FVector Scale = GetWorldScale();
	return FVector(
		BoxExtent.X * FMath::Abs(Scale.X),
		BoxExtent.Y * FMath::Abs(Scale.Y),
		BoxExtent.Z * FMath::Abs(Scale.Z)
	);
}

FVector UBoxComponent::GetBoxCenter() const
{
	return GetWorldLocation();
}

// ────────────────────────────────────────────────────────────────────────────
// UShapeComponent 인터페이스 구현
// ────────────────────────────────────────────────────────────────────────────

void UBoxComponent::OnRegister(UWorld* InWorld)
{
	Super::OnRegister(InWorld);
	UpdateBounds();
}

void UBoxComponent::GetShape(FShape& Out) const
{
	Out.Kind = EShapeKind::Box;
	Out.Box.BoxExtent = BoxExtent;
}

FAABB UBoxComponent::GetWorldAABB() const
{
	return CachedBounds.GetBox();
}

void UBoxComponent::UpdateBounds()
{
	FVector ScaledExtent = GetScaledBoxExtent();
	FVector Center = GetBoxCenter();

	// 회전을 고려한 AABB 계산
	FQuat Rotation = GetWorldRotation();

	// 회전이 없으면 기존 방식 사용
	if (Rotation.IsIdentity())
	{
		CachedBounds = FBoxSphereBounds(Center, ScaledExtent);
		return;
	}

	// 회전된 Box의 8개 꼭짓점을 계산하여 AABB 구하기
	FVector Corners[8];
	Corners[0] = FVector(-ScaledExtent.X, -ScaledExtent.Y, -ScaledExtent.Z);
	Corners[1] = FVector(+ScaledExtent.X, -ScaledExtent.Y, -ScaledExtent.Z);
	Corners[2] = FVector(+ScaledExtent.X, +ScaledExtent.Y, -ScaledExtent.Z);
	Corners[3] = FVector(-ScaledExtent.X, +ScaledExtent.Y, -ScaledExtent.Z);
	Corners[4] = FVector(-ScaledExtent.X, -ScaledExtent.Y, +ScaledExtent.Z);
	Corners[5] = FVector(+ScaledExtent.X, -ScaledExtent.Y, +ScaledExtent.Z);
	Corners[6] = FVector(+ScaledExtent.X, +ScaledExtent.Y, +ScaledExtent.Z);
	Corners[7] = FVector(-ScaledExtent.X, +ScaledExtent.Y, +ScaledExtent.Z);

	// 회전 적용
	FVector Min = FVector(FLT_MAX, FLT_MAX, FLT_MAX);
	FVector Max = FVector(-FLT_MAX, -FLT_MAX, -FLT_MAX);

	for (int32 i = 0; i < 8; ++i)
	{
		FVector RotatedCorner = Rotation.RotateVector(Corners[i]) + Center;
		Min.X = FMath::Min(Min.X, RotatedCorner.X);
		Min.Y = FMath::Min(Min.Y, RotatedCorner.Y);
		Min.Z = FMath::Min(Min.Z, RotatedCorner.Z);
		Max.X = FMath::Max(Max.X, RotatedCorner.X);
		Max.Y = FMath::Max(Max.Y, RotatedCorner.Y);
		Max.Z = FMath::Max(Max.Z, RotatedCorner.Z);
	}

	// AABB의 중심과 Extent 계산
	FVector AABBCenter = (Min + Max) * 0.5f;
	FVector AABBExtent = (Max - Min) * 0.5f;

	CachedBounds = FBoxSphereBounds(AABBCenter, AABBExtent);
}

FBoxSphereBounds UBoxComponent::GetScaledBounds() const
{
	return CachedBounds;
}

void UBoxComponent::RenderDebugVolume(URenderer* Renderer) const
{
	if (!Renderer) return;
	if (!GetOwner()) return;

	UWorld* World = GetOwner()->GetWorld();
	if (!World) return;

	// Visibility 체크
	if (!World->bPie)
	{
		if (!bShapeIsVisible)
			return;
	}
	if (World->bPie)
	{
		if (bShapeHiddenInGame)
			return;
	}

	FVector Center = GetBoxCenter();
	FVector Extent = GetScaledBoxExtent();

	// Box의 8개 꼭짓점 계산
	FVector Corners[8];
	Corners[0] = Center + FVector(-Extent.X, -Extent.Y, -Extent.Z);
	Corners[1] = Center + FVector(+Extent.X, -Extent.Y, -Extent.Z);
	Corners[2] = Center + FVector(+Extent.X, +Extent.Y, -Extent.Z);
	Corners[3] = Center + FVector(-Extent.X, +Extent.Y, -Extent.Z);
	Corners[4] = Center + FVector(-Extent.X, -Extent.Y, +Extent.Z);
	Corners[5] = Center + FVector(+Extent.X, -Extent.Y, +Extent.Z);
	Corners[6] = Center + FVector(+Extent.X, +Extent.Y, +Extent.Z);
	Corners[7] = Center + FVector(-Extent.X, +Extent.Y, +Extent.Z);

	// 라인 데이터 준비
	TArray<FVector> StartPoints;
	TArray<FVector> EndPoints;
	TArray<FVector4> Colors;

	// 충돌 중이면 빨간색, 아니면 원래 색상 (Week09 방식)
	const FVector4 LineColor = bIsOverlapping ?
		FVector4(1.0f, 0.0f, 0.0f, 1.0f) :
		FVector4(ShapeColor.X, ShapeColor.Y, ShapeColor.Z, 1.0f);

	// 뒤쪽 면 (4개 선분)
	StartPoints.push_back(Corners[0]); EndPoints.push_back(Corners[1]); Colors.push_back(LineColor);
	StartPoints.push_back(Corners[1]); EndPoints.push_back(Corners[2]); Colors.push_back(LineColor);
	StartPoints.push_back(Corners[2]); EndPoints.push_back(Corners[3]); Colors.push_back(LineColor);
	StartPoints.push_back(Corners[3]); EndPoints.push_back(Corners[0]); Colors.push_back(LineColor);

	// 앞쪽 면 (4개 선분)
	StartPoints.push_back(Corners[4]); EndPoints.push_back(Corners[5]); Colors.push_back(LineColor);
	StartPoints.push_back(Corners[5]); EndPoints.push_back(Corners[6]); Colors.push_back(LineColor);
	StartPoints.push_back(Corners[6]); EndPoints.push_back(Corners[7]); Colors.push_back(LineColor);
	StartPoints.push_back(Corners[7]); EndPoints.push_back(Corners[4]); Colors.push_back(LineColor);

	// 앞뒤 연결 (4개 선분)
	StartPoints.push_back(Corners[0]); EndPoints.push_back(Corners[4]); Colors.push_back(LineColor);
	StartPoints.push_back(Corners[1]); EndPoints.push_back(Corners[5]); Colors.push_back(LineColor);
	StartPoints.push_back(Corners[2]); EndPoints.push_back(Corners[6]); Colors.push_back(LineColor);
	StartPoints.push_back(Corners[3]); EndPoints.push_back(Corners[7]); Colors.push_back(LineColor);

	Renderer->AddLines(StartPoints, EndPoints, Colors);
}

// ────────────────────────────────────────────────────────────────────────────
// Box 전용 충돌 감지 함수
// ────────────────────────────────────────────────────────────────────────────

bool UBoxComponent::IsOverlappingBox(const UBoxComponent* OtherBox) const
{
	if (!OtherBox)
	{
		return false;
	}

	// 두 Box의 중심과 크기를 가져옵니다
	FVector MyCenter = GetBoxCenter();
	FVector MyExtent = GetScaledBoxExtent();

	FVector OtherCenter = OtherBox->GetBoxCenter();
	FVector OtherExtent = OtherBox->GetScaledBoxExtent();

	// AABB 교차 테스트 (SAT - Separating Axis Theorem)
	bool bOverlapX = FMath::Abs(MyCenter.X - OtherCenter.X) <= (MyExtent.X + OtherExtent.X);
	bool bOverlapY = FMath::Abs(MyCenter.Y - OtherCenter.Y) <= (MyExtent.Y + OtherExtent.Y);
	bool bOverlapZ = FMath::Abs(MyCenter.Z - OtherCenter.Z) <= (MyExtent.Z + OtherExtent.Z);

	return bOverlapX && bOverlapY && bOverlapZ;
}

bool UBoxComponent::IsOverlappingSphere(const USphereComponent* OtherSphere) const
{
	if (!OtherSphere)
	{
		return false;
	}

	// Box의 중심과 크기
	FVector BoxCenter = GetBoxCenter();
	FVector BoxExt = GetScaledBoxExtent();

	// Sphere의 중심과 반지름
	FVector SphereCenter = OtherSphere->GetSphereCenter();
	float SphereRadius = OtherSphere->GetScaledSphereRadius();

	// AABB를 생성합니다
	FAABB Box(BoxCenter - BoxExt, BoxCenter + BoxExt);

	// FSphere를 생성합니다
	FSphere Sphere(SphereCenter, SphereRadius);

	// Box와 Sphere 교차 테스트
	return Sphere.IntersectsAABB(Box);
}

bool UBoxComponent::ContainsPoint(const FVector& Point) const
{
	FVector BoxCenter = GetBoxCenter();
	FVector BoxExt = GetScaledBoxExtent();

	// 각 축에서 점이 Box 범위 안에 있는지 확인
	bool bInsideX = FMath::Abs(Point.X - BoxCenter.X) <= BoxExt.X;
	bool bInsideY = FMath::Abs(Point.Y - BoxCenter.Y) <= BoxExt.Y;
	bool bInsideZ = FMath::Abs(Point.Z - BoxCenter.Z) <= BoxExt.Z;

	return bInsideX && bInsideY && bInsideZ;
}

UBodySetup* UBoxComponent::GetBodySetup()
{
	if (BodySetup == nullptr)
	{
		BodySetup = NewObject<UBodySetup>();
	}
	BodySetup->AggGeom.BoxElems.Empty();

	FKBoxElem BoxElem;
    
	BoxElem.X = BoxExtent.X;
	BoxElem.Y = BoxExtent.Y;
	BoxElem.Z = BoxExtent.Z;
    
	BoxElem.Center = FVector::Zero();
	BoxElem.Rotation = FQuat::Identity();
	BodySetup->AggGeom.BoxElems.Add(BoxElem);

	return BodySetup;
}
