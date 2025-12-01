// ────────────────────────────────────────────────────────────────────────────
// CapsuleComponent.cpp
// Capsule 형태의 충돌 컴포넌트 구현 (Week09 기반, Week12 적응)
// ────────────────────────────────────────────────────────────────────────────
#include "pch.h"
#include "CapsuleComponent.h"
#include "SphereComponent.h"
#include "BoxComponent.h"
#include "Actor.h"
#include "BodySetup.h"
#include "World.h"
#include "CollisionManager.h"
#include "WorldPartitionManager.h"

// ────────────────────────────────────────────────────────────────────────────
// 생성자 / 소멸자
// ────────────────────────────────────────────────────────────────────────────

UCapsuleComponent::UCapsuleComponent()
{
	UpdateBounds();
}

UCapsuleComponent::~UCapsuleComponent()
{
}

void UCapsuleComponent::DuplicateSubObjects()
{
	Super::DuplicateSubObjects();
}

// ────────────────────────────────────────────────────────────────────────────
// Capsule 속성 함수
// ────────────────────────────────────────────────────────────────────────────

void UCapsuleComponent::SetCapsuleSize(float InRadius, float InHalfHeight, bool bUpdateBoundsNow)
{
	CapsuleRadius = InRadius;
	CapsuleHalfHeight = InHalfHeight;

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

float UCapsuleComponent::GetScaledCapsuleRadius() const
{
	FVector Scale = GetWorldScale();

	// XY 평면 스케일 사용 (Capsule은 Z축 방향)
	float MaxRadialScale = FMath::Max(FMath::Abs(Scale.X), FMath::Abs(Scale.Y));

	return CapsuleRadius * MaxRadialScale;
}

float UCapsuleComponent::GetScaledCapsuleHalfHeight() const
{
	FVector Scale = GetWorldScale();

	// Z축 스케일 사용
	return CapsuleHalfHeight * FMath::Abs(Scale.Z);
}

FVector UCapsuleComponent::GetCapsuleCenter() const
{
	return GetWorldLocation();
}

// ────────────────────────────────────────────────────────────────────────────
// UShapeComponent 인터페이스 구현
// ────────────────────────────────────────────────────────────────────────────

void UCapsuleComponent::OnRegister(UWorld* InWorld)
{
	Super::OnRegister(InWorld);
	UpdateBounds();
}

void UCapsuleComponent::GetShape(FShape& Out) const
{
	Out.Kind = EShapeKind::Capsule;
	Out.Capsule.CapsuleRadius = CapsuleRadius;
	Out.Capsule.CapsuleHalfHeight = CapsuleHalfHeight;
}

FAABB UCapsuleComponent::GetWorldAABB() const
{
	return CachedBounds.GetBox();
}

void UCapsuleComponent::UpdateBounds()
{
	float ScaledRadius = GetScaledCapsuleRadius();
	float ScaledHalfHeight = GetScaledCapsuleHalfHeight();
	FVector Center = GetCapsuleCenter();

	// 회전 정보 가져오기
	FQuat Rotation = GetWorldRotation();

	// 회전이 없으면 기존 방식 사용 (Z축 정렬 가정)
	// 언리얼 방식: HalfHeight는 반구 끝까지 포함
	if (Rotation.IsIdentity())
	{
		FVector Extent(ScaledRadius, ScaledRadius, ScaledHalfHeight);
		CachedBounds = FBoxSphereBounds(Center, Extent);
		return;
	}

	// 회전된 Capsule의 끝점 계산
	FVector SegmentStart, SegmentEnd;
	GetCapsuleSegment(SegmentStart, SegmentEnd);

	// Capsule을 감싸는 AABB 계산
	FVector Min = FVector(
		FMath::Min(SegmentStart.X, SegmentEnd.X) - ScaledRadius,
		FMath::Min(SegmentStart.Y, SegmentEnd.Y) - ScaledRadius,
		FMath::Min(SegmentStart.Z, SegmentEnd.Z) - ScaledRadius
	);

	FVector Max = FVector(
		FMath::Max(SegmentStart.X, SegmentEnd.X) + ScaledRadius,
		FMath::Max(SegmentStart.Y, SegmentEnd.Y) + ScaledRadius,
		FMath::Max(SegmentStart.Z, SegmentEnd.Z) + ScaledRadius
	);

	// AABB의 중심과 Extent 계산
	FVector AABBCenter = (Min + Max) * 0.5f;
	FVector AABBExtent = (Max - Min) * 0.5f;

	CachedBounds = FBoxSphereBounds(AABBCenter, AABBExtent);
}

FBoxSphereBounds UCapsuleComponent::GetScaledBounds() const
{
	return CachedBounds;
}

void UCapsuleComponent::RenderDebugVolume(URenderer* Renderer) const
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

	FVector Center = GetCapsuleCenter();
	float Radius = GetScaledCapsuleRadius();
	float HalfHeight = GetScaledCapsuleHalfHeight();

	// Capsule의 선분 끝점 계산
	FVector SegmentStart, SegmentEnd;
	GetCapsuleSegment(SegmentStart, SegmentEnd);

	// 회전 정보 가져오기
	FQuat Rotation = GetWorldRotation();
	FVector UpVector = Rotation.GetUpVector();
	FVector RightVector = Rotation.GetRightVector();
	FVector ForwardVector = Rotation.GetForwardVector();

	// 라인 데이터 준비
	TArray<FVector> StartPoints;
	TArray<FVector> EndPoints;
	TArray<FVector4> Colors;

	// 충돌 중이면 빨간색, 아니면 원래 색상 (Week09 방식)
	const FVector4 LineColor = bIsOverlapping ?
		FVector4(1.0f, 0.0f, 0.0f, 1.0f) :
		FVector4(ShapeColor.X, ShapeColor.Y, ShapeColor.Z, 1.0f);
	const int32 NumSegments = 16;

	// 상단 원 (회전이 적용된 원 - Z축에 수직)
	for (int32 i = 0; i < NumSegments; ++i)
	{
		float Angle1 = (static_cast<float>(i) / NumSegments) * 2.0f * PI;
		float Angle2 = (static_cast<float>((i + 1) % NumSegments) / NumSegments) * 2.0f * PI;

		FVector Offset1 = (RightVector * cos(Angle1) + ForwardVector * sin(Angle1)) * Radius;
		FVector Offset2 = (RightVector * cos(Angle2) + ForwardVector * sin(Angle2)) * Radius;

		StartPoints.push_back(SegmentEnd + Offset1);
		EndPoints.push_back(SegmentEnd + Offset2);
		Colors.push_back(LineColor);
	}

	// 하단 원 (회전이 적용된 원 - Z축에 수직)
	for (int32 i = 0; i < NumSegments; ++i)
	{
		float Angle1 = (static_cast<float>(i) / NumSegments) * 2.0f * PI;
		float Angle2 = (static_cast<float>((i + 1) % NumSegments) / NumSegments) * 2.0f * PI;

		FVector Offset1 = (RightVector * cos(Angle1) + ForwardVector * sin(Angle1)) * Radius;
		FVector Offset2 = (RightVector * cos(Angle2) + ForwardVector * sin(Angle2)) * Radius;

		StartPoints.push_back(SegmentStart + Offset1);
		EndPoints.push_back(SegmentStart + Offset2);
		Colors.push_back(LineColor);
	}

	// 중앙 원 (회전이 적용된 원 - Z축에 수직)
	for (int32 i = 0; i < NumSegments; ++i)
	{
		float Angle1 = (static_cast<float>(i) / NumSegments) * 2.0f * PI;
		float Angle2 = (static_cast<float>((i + 1) % NumSegments) / NumSegments) * 2.0f * PI;

		FVector Offset1 = (RightVector * cos(Angle1) + ForwardVector * sin(Angle1)) * Radius;
		FVector Offset2 = (RightVector * cos(Angle2) + ForwardVector * sin(Angle2)) * Radius;

		StartPoints.push_back(Center + Offset1);
		EndPoints.push_back(Center + Offset2);
		Colors.push_back(LineColor);
	}

	// 상단 반원 호 (Right 방향)
	for (int32 i = 0; i < NumSegments / 2; ++i)
	{
		float Angle1 = (static_cast<float>(i) / NumSegments) * 2.0f * PI;
		float Angle2 = (static_cast<float>(i + 1) / NumSegments) * 2.0f * PI;

		FVector Offset1 = (RightVector * cos(Angle1) + UpVector * sin(Angle1)) * Radius;
		FVector Offset2 = (RightVector * cos(Angle2) + UpVector * sin(Angle2)) * Radius;

		StartPoints.push_back(SegmentEnd + Offset1);
		EndPoints.push_back(SegmentEnd + Offset2);
		Colors.push_back(LineColor);
	}

	// 하단 반원 호 (Right 방향)
	for (int32 i = NumSegments / 2; i < NumSegments; ++i)
	{
		float Angle1 = (static_cast<float>(i) / NumSegments) * 2.0f * PI;
		float Angle2 = (static_cast<float>(i + 1) / NumSegments) * 2.0f * PI;

		FVector Offset1 = (RightVector * cos(Angle1) + UpVector * sin(Angle1)) * Radius;
		FVector Offset2 = (RightVector * cos(Angle2) + UpVector * sin(Angle2)) * Radius;

		StartPoints.push_back(SegmentStart + Offset1);
		EndPoints.push_back(SegmentStart + Offset2);
		Colors.push_back(LineColor);
	}

	// 상단 반원 호 (Forward 방향)
	for (int32 i = 0; i < NumSegments / 2; ++i)
	{
		float Angle1 = (static_cast<float>(i) / NumSegments) * 2.0f * PI;
		float Angle2 = (static_cast<float>(i + 1) / NumSegments) * 2.0f * PI;

		FVector Offset1 = (ForwardVector * cos(Angle1) + UpVector * sin(Angle1)) * Radius;
		FVector Offset2 = (ForwardVector * cos(Angle2) + UpVector * sin(Angle2)) * Radius;

		StartPoints.push_back(SegmentEnd + Offset1);
		EndPoints.push_back(SegmentEnd + Offset2);
		Colors.push_back(LineColor);
	}

	// 하단 반원 호 (Forward 방향)
	for (int32 i = NumSegments / 2; i < NumSegments; ++i)
	{
		float Angle1 = (static_cast<float>(i) / NumSegments) * 2.0f * PI;
		float Angle2 = (static_cast<float>(i + 1) / NumSegments) * 2.0f * PI;

		FVector Offset1 = (ForwardVector * cos(Angle1) + UpVector * sin(Angle1)) * Radius;
		FVector Offset2 = (ForwardVector * cos(Angle2) + UpVector * sin(Angle2)) * Radius;

		StartPoints.push_back(SegmentStart + Offset1);
		EndPoints.push_back(SegmentStart + Offset2);
		Colors.push_back(LineColor);
	}

	// 수직 연결 라인 (4개 방향 - 회전 적용)
	FVector Offsets[4] = {
		RightVector * Radius,
		RightVector * -Radius,
		ForwardVector * Radius,
		ForwardVector * -Radius
	};

	for (int32 i = 0; i < 4; ++i)
	{
		StartPoints.push_back(SegmentStart + Offsets[i]);
		EndPoints.push_back(SegmentEnd + Offsets[i]);
		Colors.push_back(LineColor);
	}

	Renderer->AddLines(StartPoints, EndPoints, Colors);
}

// ────────────────────────────────────────────────────────────────────────────
// 내부 함수
// ────────────────────────────────────────────────────────────────────────────

void UCapsuleComponent::GetCapsuleSegment(FVector& OutStart, FVector& OutEnd) const
{
	FVector Center = GetCapsuleCenter();
	float Radius = GetScaledCapsuleRadius();
	float HalfHeight = GetScaledCapsuleHalfHeight();

	// 실린더 부분의 높이 (언리얼 방식: HalfHeight는 반구 포함, 선분은 반구 중심까지)
	float CylinderHalfHeight = FMath::Max(0.0f, HalfHeight - Radius);

	// Z축 방향 (로컬)
	FVector UpDirection = GetWorldRotation().GetUpVector();

	OutStart = Center - UpDirection * CylinderHalfHeight;
	OutEnd = Center + UpDirection * CylinderHalfHeight;
}

float UCapsuleComponent::PointToSegmentDistanceSquared(
	const FVector& Point,
	const FVector& SegmentStart,
	const FVector& SegmentEnd)
{
	FVector Segment = SegmentEnd - SegmentStart;
	FVector PointToStart = Point - SegmentStart;

	float SegmentLengthSquared = Segment.SizeSquared();

	// 선분이 점인 경우 (길이가 0)
	if (SegmentLengthSquared < KINDA_SMALL_NUMBER)
	{
		return PointToStart.SizeSquared();
	}

	// 투영 비율 계산 (0~1 사이로 클램프)
	float t = FVector::Dot(PointToStart, Segment) / SegmentLengthSquared;
	t = FMath::Clamp(t, 0.0f, 1.0f);

	// 선분 위의 가장 가까운 점
	FVector ClosestPoint = SegmentStart + Segment * t;

	return (Point - ClosestPoint).SizeSquared();
}

float UCapsuleComponent::SegmentToSegmentDistanceSquared(
	const FVector& Seg1Start,
	const FVector& Seg1End,
	const FVector& Seg2Start,
	const FVector& Seg2End)
{
	// 간략 구현: 각 선분의 끝점과 중점을 샘플링하여 최단 거리 근사
	float MinDistSq = FLT_MAX;

	// Seg1의 샘플 점들과 Seg2 사이의 거리
	MinDistSq = FMath::Min(MinDistSq, PointToSegmentDistanceSquared(Seg1Start, Seg2Start, Seg2End));
	MinDistSq = FMath::Min(MinDistSq, PointToSegmentDistanceSquared(Seg1End, Seg2Start, Seg2End));
	MinDistSq = FMath::Min(MinDistSq, PointToSegmentDistanceSquared((Seg1Start + Seg1End) * 0.5f, Seg2Start, Seg2End));

	// Seg2의 샘플 점들과 Seg1 사이의 거리
	MinDistSq = FMath::Min(MinDistSq, PointToSegmentDistanceSquared(Seg2Start, Seg1Start, Seg1End));
	MinDistSq = FMath::Min(MinDistSq, PointToSegmentDistanceSquared(Seg2End, Seg1Start, Seg1End));
	MinDistSq = FMath::Min(MinDistSq, PointToSegmentDistanceSquared((Seg2Start + Seg2End) * 0.5f, Seg1Start, Seg1End));

	return MinDistSq;
}

// ────────────────────────────────────────────────────────────────────────────
// Capsule 전용 충돌 감지 함수
// ────────────────────────────────────────────────────────────────────────────

bool UCapsuleComponent::IsOverlappingCapsule(const UCapsuleComponent* OtherCapsule) const
{
	if (!OtherCapsule)
	{
		return false;
	}

	// 두 Capsule의 선분 끝점 계산
	FVector MyStart, MyEnd;
	GetCapsuleSegment(MyStart, MyEnd);
	float MyRadius = GetScaledCapsuleRadius();

	FVector OtherStart, OtherEnd;
	OtherCapsule->GetCapsuleSegment(OtherStart, OtherEnd);
	float OtherRadius = OtherCapsule->GetScaledCapsuleRadius();

	// 선분 vs 선분 최단 거리 계산
	float DistanceSquared = SegmentToSegmentDistanceSquared(MyStart, MyEnd, OtherStart, OtherEnd);
	float RadiusSum = MyRadius + OtherRadius;

	// 거리가 반지름의 합보다 작거나 같으면 충돌
	return DistanceSquared <= (RadiusSum * RadiusSum);
}

bool UCapsuleComponent::IsOverlappingSphere(const USphereComponent* OtherSphere) const
{
	if (!OtherSphere)
	{
		return false;
	}

	FVector SphereCenter = OtherSphere->GetSphereCenter();
	float SphereRadius = OtherSphere->GetScaledSphereRadius();

	FVector SegmentStart, SegmentEnd;
	GetCapsuleSegment(SegmentStart, SegmentEnd);
	float CapsuleRad = GetScaledCapsuleRadius();

	// 점과 선분 사이의 거리 계산
	float DistanceSquared = PointToSegmentDistanceSquared(SphereCenter, SegmentStart, SegmentEnd);
	float RadiusSum = CapsuleRad + SphereRadius;

	// 거리가 반지름의 합보다 작거나 같으면 충돌
	return DistanceSquared <= (RadiusSum * RadiusSum);
}

bool UCapsuleComponent::IsOverlappingBox(const UBoxComponent* OtherBox) const
{
	if (!OtherBox)
	{
		return false;
	}

	// 간략 구현: Bounds 기반 체크
	return CachedBounds.Intersects(OtherBox->GetScaledBounds());
}

bool UCapsuleComponent::ContainsPoint(const FVector& Point) const
{
	FVector SegmentStart, SegmentEnd;
	GetCapsuleSegment(SegmentStart, SegmentEnd);
	float Radius = GetScaledCapsuleRadius();

	// 점과 선분 사이의 거리 계산
	float DistanceSquared = PointToSegmentDistanceSquared(Point, SegmentStart, SegmentEnd);

	// 거리가 반지름보다 작거나 같으면 포함
	return DistanceSquared <= (Radius * Radius);
}

UBodySetup* UCapsuleComponent::GetBodySetup()
{
	if (BodySetup == nullptr)
	{
		BodySetup = NewObject<UBodySetup>();
	}
	BodySetup->ClearAllShapes();
	BodySetup->AddCapsuleElem(CapsuleRadius, CapsuleHalfHeight);

	return BodySetup;
}
