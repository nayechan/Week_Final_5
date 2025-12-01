// ────────────────────────────────────────────────────────────────────────────
// SphereComponent.cpp
// Sphere 형태의 충돌 컴포넌트 구현 (Week09 기반, Week12 적응)
// ────────────────────────────────────────────────────────────────────────────
#include "pch.h"
#include "SphereComponent.h"
#include "BoxComponent.h"
#include "Actor.h"
#include "AABB.h"
#include "BodySetup.h"
#include "World.h"
#include "CollisionManager.h"
#include "WorldPartitionManager.h"

// ────────────────────────────────────────────────────────────────────────────
// 생성자 / 소멸자
// ────────────────────────────────────────────────────────────────────────────

USphereComponent::USphereComponent()
{
	SphereRadius = 50.0f;
	UpdateBounds();
}

USphereComponent::~USphereComponent()
{
}

void USphereComponent::DuplicateSubObjects()
{
	Super::DuplicateSubObjects();
}

// ────────────────────────────────────────────────────────────────────────────
// Sphere 속성 함수
// ────────────────────────────────────────────────────────────────────────────

void USphereComponent::SetSphereRadius(float InRadius, bool bUpdateBoundsNow)
{
	if (SphereRadius - InRadius <= 0.0001f)
	{
		return; 
	}
	SphereRadius = InRadius;

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

float USphereComponent::GetScaledSphereRadius() const
{
	FVector Scale = GetWorldScale();

	// 최대 스케일 값을 사용 (구는 균등 스케일 가정)
	float MaxScale = FMath::Max(FMath::Abs(Scale.X), FMath::Max(FMath::Abs(Scale.Y), FMath::Abs(Scale.Z)));

	return SphereRadius * MaxScale;
}

FVector USphereComponent::GetSphereCenter() const
{
	return GetWorldLocation();
}

// ────────────────────────────────────────────────────────────────────────────
// UShapeComponent 인터페이스 구현
// ────────────────────────────────────────────────────────────────────────────

void USphereComponent::OnRegister(UWorld* InWorld)
{
	Super::OnRegister(InWorld);
	UpdateBounds();
}

void USphereComponent::GetShape(FShape& Out) const
{
	Out.Kind = EShapeKind::Sphere;
	Out.Sphere.SphereRadius = SphereRadius;
}

FAABB USphereComponent::GetWorldAABB() const
{
	return CachedBounds.GetBox();
}

void USphereComponent::UpdateBounds()
{
	float ScaledRadius = GetScaledSphereRadius();
	FVector Center = GetSphereCenter();

	// Sphere의 Bounds는 반지름을 Extent로 하는 Box
	FVector Extent(ScaledRadius, ScaledRadius, ScaledRadius);

	CachedBounds = FBoxSphereBounds(Center, Extent);
}

FBoxSphereBounds USphereComponent::GetScaledBounds() const
{
	return CachedBounds;
}

void USphereComponent::RenderDebugVolume(URenderer* Renderer) const
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

	FVector Center = GetSphereCenter();
	float Radius = GetScaledSphereRadius();

	// 라인 데이터 준비
	TArray<FVector> StartPoints;
	TArray<FVector> EndPoints;
	TArray<FVector4> Colors;

	// 충돌 중이면 빨간색, 아니면 원래 색상 (Week09 방식)
	const FVector4 LineColor = bIsOverlapping ?
		FVector4(1.0f, 0.0f, 0.0f, 1.0f) :
		FVector4(ShapeColor.X, ShapeColor.Y, ShapeColor.Z, 1.0f);
	const int32 NumSegments = 32;

	// XY 평면 원 (Z축 중심)
	for (int32 i = 0; i < NumSegments; ++i)
	{
		float Angle1 = (static_cast<float>(i) / NumSegments) * 2.0f * PI;
		float Angle2 = (static_cast<float>((i + 1) % NumSegments) / NumSegments) * 2.0f * PI;

		FVector Point1 = Center + FVector(cos(Angle1) * Radius, sin(Angle1) * Radius, 0.0f);
		FVector Point2 = Center + FVector(cos(Angle2) * Radius, sin(Angle2) * Radius, 0.0f);

		StartPoints.push_back(Point1);
		EndPoints.push_back(Point2);
		Colors.push_back(LineColor);
	}

	// XZ 평면 원 (Y축 중심)
	for (int32 i = 0; i < NumSegments; ++i)
	{
		float Angle1 = (static_cast<float>(i) / NumSegments) * 2.0f * PI;
		float Angle2 = (static_cast<float>((i + 1) % NumSegments) / NumSegments) * 2.0f * PI;

		FVector Point1 = Center + FVector(cos(Angle1) * Radius, 0.0f, sin(Angle1) * Radius);
		FVector Point2 = Center + FVector(cos(Angle2) * Radius, 0.0f, sin(Angle2) * Radius);

		StartPoints.push_back(Point1);
		EndPoints.push_back(Point2);
		Colors.push_back(LineColor);
	}

	// YZ 평면 원 (X축 중심)
	for (int32 i = 0; i < NumSegments; ++i)
	{
		float Angle1 = (static_cast<float>(i) / NumSegments) * 2.0f * PI;
		float Angle2 = (static_cast<float>((i + 1) % NumSegments) / NumSegments) * 2.0f * PI;

		FVector Point1 = Center + FVector(0.0f, cos(Angle1) * Radius, sin(Angle1) * Radius);
		FVector Point2 = Center + FVector(0.0f, cos(Angle2) * Radius, sin(Angle2) * Radius);

		StartPoints.push_back(Point1);
		EndPoints.push_back(Point2);
		Colors.push_back(LineColor);
	}

	Renderer->AddLines(StartPoints, EndPoints, Colors);
}

// ────────────────────────────────────────────────────────────────────────────
// Sphere 전용 충돌 감지 함수
// ────────────────────────────────────────────────────────────────────────────

bool USphereComponent::IsOverlappingSphere(const USphereComponent* OtherSphere) const
{
	if (!OtherSphere)
	{
		return false;
	}

	// 두 Sphere의 중심과 반지름을 가져옵니다
	FVector MyCenter = GetSphereCenter();
	float MyRadius = GetScaledSphereRadius();

	FVector OtherCenter = OtherSphere->GetSphereCenter();
	float OtherRadius = OtherSphere->GetScaledSphereRadius();

	// 중심 간 거리 제곱 계산 (제곱근 연산 회피)
	float DistanceSquared = (MyCenter - OtherCenter).SizeSquared();
	float RadiusSum = MyRadius + OtherRadius;

	// 거리가 반지름의 합보다 작거나 같으면 충돌
	return DistanceSquared <= (RadiusSum * RadiusSum);
}

bool USphereComponent::IsOverlappingBox(const UBoxComponent* OtherBox) const
{
	if (!OtherBox)
	{
		return false;
	}

	// Box 측에서 구현된 충돌 감지 사용 (중복 구현 방지)
	return OtherBox->IsOverlappingSphere(this);
}

bool USphereComponent::ContainsPoint(const FVector& Point) const
{
	FVector Center = GetSphereCenter();
	float Radius = GetScaledSphereRadius();

	// 점과 중심 사이의 거리 제곱 계산
	float DistanceSquared = (Point - Center).SizeSquared();

	// 거리가 반지름보다 작거나 같으면 포함
	return DistanceSquared <= (Radius * Radius);
}

UBodySetup* USphereComponent::GetBodySetup()
{
	if (BodySetup == nullptr)
	{
		BodySetup = NewObject<UBodySetup>();
	}
	BodySetup->ClearAllShapes();
	BodySetup->AddSphereElem(GetSphereRadius());

	return BodySetup;
}
