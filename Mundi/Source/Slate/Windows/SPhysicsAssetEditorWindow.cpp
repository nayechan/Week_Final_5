#include "pch.h"
#include "SPhysicsAssetEditorWindow.h"
#include "SlateManager.h"
#include "ImGui/imgui.h"
#include "ImGui/imnodes.h"
#include "FViewport.h"
#include "FViewportClient.h"
#include <filesystem>
#include "Source/Runtime/Engine/Viewer/PhysicsAssetEditorBootstrap.h"
#include "Source/Runtime/Engine/Viewer/EditorAssetPreviewContext.h"
#include "Source/Runtime/Engine/PhysicsEngine/PhysicsAsset.h"
#include "Source/Runtime/Engine/PhysicsEngine/PhysicsTypes.h"
#include "Source/Runtime/Engine/PhysicsEngine/BodySetup.h"
#include "Source/Runtime/Engine/PhysicsEngine/FConstraintSetup.h"
#include "Source/Runtime/Core/Misc/VertexData.h"
#include "SkeletalMeshActor.h"
#include "SkeletalMesh.h"
#include "SkeletalMeshComponent.h"
#include "Source/Runtime/Engine/Components/LineComponent.h"
#include "Source/Runtime/AssetManagement/Line.h"
#include "Source/Runtime/AssetManagement/LinesBatch.h"
#include "Source/Editor/PlatformProcess.h"
#include "Source/Slate/Widgets/ModalDialog.h"
#include "ResourceManager.h"

// Sub Widgets
#include "Source/Slate/Widgets/PhysicsAssetEditor/SkeletonTreeWidget.h"
#include "Source/Slate/Widgets/PhysicsAssetEditor/BodyPropertiesWidget.h"
#include "Source/Slate/Widgets/PhysicsAssetEditor/ConstraintPropertiesWidget.h"
#include "Source/Slate/Widgets/PhysicsAssetEditor/ToolsWidget.h"

// Gizmo
#include "Source/Editor/Gizmo/GizmoActor.h"
#include "Source/Runtime/Engine/Components/BoneAnchorComponent.h"
#include "SelectionManager.h"
#include "World.h"

// Picking
#include "Source/Runtime/Engine/Collision/Picking.h"
#include "CameraActor.h"
#include <chrono>

// ────────────────────────────────────────────────────────────────
// Shape 라인 좌표 생성 헬퍼
// ────────────────────────────────────────────────────────────────

// TODO: M_PI를 Math/MathConstants.h 등에 전역 상수로 이동
namespace
{
	constexpr float M_PI = 3.14159265358979f;
	constexpr int32 CircleSegments = 16;
	constexpr int32 CapsuleVerticalLines = 8;
	constexpr int32 ConeSegments = 12;        // 원뿔 세그먼트 수
	constexpr int32 SphereRings = 3;          // 구체 링 수
	constexpr float ConstraintSphereRadius = 0.02f;  // 조인트 구체 크기
}

/**
 * Sphere Shape 와이어프레임 라인 좌표 생성
 */
static void GenerateSphereLineCoordinates(
	const FKSphereElem& Sphere,
	const FTransform& BoneTransform,
	TArray<FVector>& OutStartPoints,
	TArray<FVector>& OutEndPoints)
{
	FVector Center = BoneTransform.Translation + BoneTransform.Rotation.RotateVector(Sphere.Center);
	float Radius = Sphere.Radius;

	// 3개 평면 (XY, XZ, YZ)에 원 그리기
	for (int32 j = 0; j < CircleSegments; ++j)
	{
		float Angle1 = (j / static_cast<float>(CircleSegments)) * 2.0f * M_PI;
		float Angle2 = ((j + 1) / static_cast<float>(CircleSegments)) * 2.0f * M_PI;
		float Cos1 = cos(Angle1), Sin1 = sin(Angle1);
		float Cos2 = cos(Angle2), Sin2 = sin(Angle2);

		// XY 평면
		OutStartPoints.Add(Center + FVector(Cos1 * Radius, Sin1 * Radius, 0));
		OutEndPoints.Add(Center + FVector(Cos2 * Radius, Sin2 * Radius, 0));
		// XZ 평면
		OutStartPoints.Add(Center + FVector(Cos1 * Radius, 0, Sin1 * Radius));
		OutEndPoints.Add(Center + FVector(Cos2 * Radius, 0, Sin2 * Radius));
		// YZ 평면
		OutStartPoints.Add(Center + FVector(0, Cos1 * Radius, Sin1 * Radius));
		OutEndPoints.Add(Center + FVector(0, Cos2 * Radius, Sin2 * Radius));
	}
}

/**
 * Box Shape 와이어프레임 라인 좌표 생성
 */
static void GenerateBoxLineCoordinates(
	const FKBoxElem& Box,
	const FTransform& BoneTransform,
	TArray<FVector>& OutStartPoints,
	TArray<FVector>& OutEndPoints)
{
	FVector Center = BoneTransform.Translation + BoneTransform.Rotation.RotateVector(Box.Center);
	FQuat Rotation = BoneTransform.Rotation * Box.Rotation;

	// Box.X, Box.Y, Box.Z는 지름이므로 반으로 나눔
	FVector HalfExtent(Box.X * 0.5f, Box.Y * 0.5f, Box.Z * 0.5f);

	// 8개 코너 생성 (dx/dy/dz 패턴)
	constexpr int dx[] = {-1, 1, 1, -1, -1, 1, 1, -1};
	constexpr int dy[] = {-1, -1, 1, 1, -1, -1, 1, 1};
	constexpr int dz[] = {-1, -1, -1, -1, 1, 1, 1, 1};

	FVector Corners[8];
	for (int i = 0; i < 8; ++i)
	{
		Corners[i] = Center + Rotation.RotateVector(
			FVector(HalfExtent.X * dx[i], HalfExtent.Y * dy[i], HalfExtent.Z * dz[i]));
	}

	// 12개 엣지 (pair로 정의)
	constexpr std::pair<int, int> Edges[] = {
		{0,1}, {1,2}, {2,3}, {3,0},  // 하단 면
		{4,5}, {5,6}, {6,7}, {7,4},  // 상단 면
		{0,4}, {1,5}, {2,6}, {3,7}   // 수직 엣지
	};

	for (const auto& [a, b] : Edges)
	{
		OutStartPoints.Add(Corners[a]);
		OutEndPoints.Add(Corners[b]);
	}
}

/**
 * Capsule(Sphyl) Shape 와이어프레임 라인 좌표 생성
 */
static void GenerateCapsuleLineCoordinates(
	const FKSphylElem& Capsule,
	const FTransform& BoneTransform,
	TArray<FVector>& OutStartPoints,
	TArray<FVector>& OutEndPoints)
{
	FVector Center = BoneTransform.Translation + BoneTransform.Rotation.RotateVector(Capsule.Center);
	FQuat Rotation = BoneTransform.Rotation * Capsule.Rotation;

	float HalfLength = Capsule.Length * 0.5f;
	float Radius = Capsule.Radius;

	// 캡슐은 Z축 방향 (기존 엔진 규약)
	FVector Up = Rotation.RotateVector(FVector(0, 0, 1));  // Z축이 길이 방향
	FVector Top = Center + Up * HalfLength;
	FVector Bottom = Center - Up * HalfLength;

	// 수직 라인들
	for (int32 j = 0; j < CapsuleVerticalLines; ++j)
	{
		float Angle = (j / static_cast<float>(CapsuleVerticalLines)) * 2.0f * M_PI;
		FVector Dir = Rotation.RotateVector(FVector(cos(Angle), sin(Angle), 0));
		OutStartPoints.Add(Top + Dir * Radius);
		OutEndPoints.Add(Bottom + Dir * Radius);
	}

	// 상단/하단 원 (XY 평면, Z축이 길이 방향이므로)
	for (int32 j = 0; j < CircleSegments; ++j)
	{
		float Angle1 = (j / static_cast<float>(CircleSegments)) * 2.0f * M_PI;
		float Angle2 = ((j + 1) / static_cast<float>(CircleSegments)) * 2.0f * M_PI;
		FVector Dir1 = Rotation.RotateVector(FVector(cos(Angle1), sin(Angle1), 0));
		FVector Dir2 = Rotation.RotateVector(FVector(cos(Angle2), sin(Angle2), 0));

		OutStartPoints.Add(Top + Dir1 * Radius);
		OutEndPoints.Add(Top + Dir2 * Radius);
		OutStartPoints.Add(Bottom + Dir1 * Radius);
		OutEndPoints.Add(Bottom + Dir2 * Radius);
	}
}

// ────────────────────────────────────────────────────────────────
// Constraint 시각화 헬퍼 함수들
// ────────────────────────────────────────────────────────────────

/**
 * 조인트 위치에 작은 구체(Sphere) 와이어프레임 생성
 */
static void GenerateConstraintSphereLines(
	const FVector& Center,
	float Radius,
	const FVector4& Color,
	FLinesBatch& OutBatch)
{
	// 3개 평면에 원 그리기
	for (int32 j = 0; j < CircleSegments; ++j)
	{
		float Angle1 = (j / static_cast<float>(CircleSegments)) * 2.0f * M_PI;
		float Angle2 = ((j + 1) / static_cast<float>(CircleSegments)) * 2.0f * M_PI;
		float Cos1 = cos(Angle1), Sin1 = sin(Angle1);
		float Cos2 = cos(Angle2), Sin2 = sin(Angle2);

		// XY 평면
		OutBatch.Add(Center + FVector(Cos1 * Radius, Sin1 * Radius, 0),
		             Center + FVector(Cos2 * Radius, Sin2 * Radius, 0), Color);
		// XZ 평면
		OutBatch.Add(Center + FVector(Cos1 * Radius, 0, Sin1 * Radius),
		             Center + FVector(Cos2 * Radius, 0, Sin2 * Radius), Color);
		// YZ 평면
		OutBatch.Add(Center + FVector(0, Cos1 * Radius, Sin1 * Radius),
		             Center + FVector(0, Cos2 * Radius, Sin2 * Radius), Color);
	}
}

/**
 * Swing Limit 원뿔(Cone) 시각화 생성
 * @param JointPos 조인트 위치
 * @param JointRotation 조인트 회전 (부모→자식 방향)
 * @param Swing1Limit Y축 회전 제한 (degrees)
 * @param Swing2Limit Z축 회전 제한 (degrees)
 * @param ConeLength 원뿔 길이
 */
static void GenerateSwingConeLimitLines(
	const FVector& JointPos,
	const FQuat& JointRotation,
	float Swing1Limit,
	float Swing2Limit,
	float ConeLength,
	const FVector4& Color,
	FLinesBatch& OutBatch)
{
	// 각도를 라디안으로 변환
	float Swing1Rad = Swing1Limit * M_PI / 180.0f;
	float Swing2Rad = Swing2Limit * M_PI / 180.0f;

	// 원뿔 베이스 원의 점들 계산
	TArray<FVector> ConeBasePoints;
	ConeBasePoints.Reserve(ConeSegments);

	for (int32 i = 0; i < ConeSegments; ++i)
	{
		float Angle = (i / static_cast<float>(ConeSegments)) * 2.0f * M_PI;

		// 타원형 원뿔 (Swing1 = Y축, Swing2 = Z축)
		float RadiusY = ConeLength * tan(Swing1Rad);
		float RadiusZ = ConeLength * tan(Swing2Rad);

		// 로컬 좌표에서 원뿔 베이스 점
		FVector LocalPoint(
			ConeLength,  // X축 방향이 원뿔 축
			RadiusY * cos(Angle),
			RadiusZ * sin(Angle)
		);

		// 월드 좌표로 변환
		FVector WorldPoint = JointPos + JointRotation.RotateVector(LocalPoint);
		ConeBasePoints.Add(WorldPoint);
	}

	// 원뿔 베이스 원 그리기
	for (int32 i = 0; i < ConeSegments; ++i)
	{
		int32 NextIdx = (i + 1) % ConeSegments;
		OutBatch.Add(ConeBasePoints[i], ConeBasePoints[NextIdx], Color);
	}

	// 조인트에서 원뿔 베이스까지 선 그리기 (4개 주요 방향)
	for (int32 i = 0; i < ConeSegments; i += ConeSegments / 4)
	{
		OutBatch.Add(JointPos, ConeBasePoints[i], Color);
	}
}

/**
 * Twist Limit 원호(Arc) 시각화 생성
 * @param JointPos 조인트 위치
 * @param JointRotation 조인트 회전
 * @param TwistMin 최소 회전 (degrees)
 * @param TwistMax 최대 회전 (degrees)
 * @param ArcRadius 원호 반지름
 */
static void GenerateTwistLimitLines(
	const FVector& JointPos,
	const FQuat& JointRotation,
	float TwistMin,
	float TwistMax,
	float ArcRadius,
	const FVector4& Color,
	FLinesBatch& OutBatch)
{
	// 각도를 라디안으로 변환
	float TwistMinRad = TwistMin * M_PI / 180.0f;
	float TwistMaxRad = TwistMax * M_PI / 180.0f;

	// Twist 각도 범위
	float TwistRange = TwistMaxRad - TwistMinRad;
	int32 NumSegments = FMath::Max(4, static_cast<int32>(TwistRange / (M_PI / 8.0f)));

	// X축 기준으로 YZ 평면에 원호 그리기
	TArray<FVector> ArcPoints;
	ArcPoints.Reserve(NumSegments + 1);

	for (int32 i = 0; i <= NumSegments; ++i)
	{
		float T = static_cast<float>(i) / static_cast<float>(NumSegments);
		float Angle = TwistMinRad + T * TwistRange;

		// 로컬 좌표에서 원호 점 (X축이 Twist 축)
		FVector LocalPoint(
			0.0f,
			ArcRadius * cos(Angle),
			ArcRadius * sin(Angle)
		);

		// 월드 좌표로 변환
		FVector WorldPoint = JointPos + JointRotation.RotateVector(LocalPoint);
		ArcPoints.Add(WorldPoint);
	}

	// 원호 그리기
	for (int32 i = 0; i < ArcPoints.Num() - 1; ++i)
	{
		OutBatch.Add(ArcPoints[i], ArcPoints[i + 1], Color);
	}

	// 시작/끝 라인 (조인트 중심에서)
	if (ArcPoints.Num() >= 2)
	{
		OutBatch.Add(JointPos, ArcPoints[0], Color);
		OutBatch.Add(JointPos, ArcPoints.Last(), Color);
	}
}

/**
 * Constraint 전체 시각화 생성 (라인 기반 - 와이어프레임)
 */
static void GenerateConstraintVisualization(
	const FConstraintSetup& Constraint,
	const FVector& ParentPos,
	const FVector& ChildPos,
	const FQuat& JointRotation,
	bool bSelected,
	FLinesBatch& OutBatch)
{
	// 색상 설정
	FVector4 SphereColor = bSelected ? FVector4(1.0f, 1.0f, 0.0f, 1.0f) : FVector4(1.0f, 0.6f, 0.0f, 1.0f);  // 노란/주황
	FVector4 SwingColor = bSelected ? FVector4(0.0f, 1.0f, 0.5f, 1.0f) : FVector4(0.0f, 0.8f, 0.4f, 1.0f);   // 초록 (Swing)
	FVector4 TwistColor = bSelected ? FVector4(0.5f, 0.5f, 1.0f, 1.0f) : FVector4(0.3f, 0.3f, 0.8f, 1.0f);   // 파랑 (Twist)
	FVector4 LineColor = bSelected ? FVector4(1.0f, 1.0f, 0.0f, 1.0f) : FVector4(1.0f, 0.5f, 0.0f, 1.0f);    // 연결선

	// 조인트 위치 = 자식 본 위치
	FVector JointPos = ChildPos;

	// 1. 조인트 위치에 구체 그리기
	float SphereRadius = bSelected ? ConstraintSphereRadius * 1.5f : ConstraintSphereRadius;
	GenerateConstraintSphereLines(JointPos, SphereRadius, SphereColor, OutBatch);

	// 2. 부모-자식 연결선
	OutBatch.Add(ParentPos, ChildPos, LineColor);

	// 3. 원뿔 길이 계산 (부모-자식 거리의 30%)
	float Distance = (ChildPos - ParentPos).Size();
	float ConeLength = FMath::Max(0.05f, Distance * 0.3f);

	// 4. Swing Limit 원뿔 (각도가 유의미할 때만)
	if (Constraint.Swing1Limit > 1.0f || Constraint.Swing2Limit > 1.0f)
	{
		GenerateSwingConeLimitLines(JointPos, JointRotation,
			Constraint.Swing1Limit, Constraint.Swing2Limit, ConeLength, SwingColor, OutBatch);
	}

	// 5. Twist Limit 원호 (각도가 유의미할 때만)
	float TwistRange = Constraint.TwistLimitMax - Constraint.TwistLimitMin;
	if (TwistRange > 1.0f && TwistRange < 359.0f)
	{
		float ArcRadius = ConeLength * 0.5f;
		GenerateTwistLimitLines(JointPos, JointRotation,
			Constraint.TwistLimitMin, Constraint.TwistLimitMax, ArcRadius, TwistColor, OutBatch);
	}
}

// ────────────────────────────────────────────────────────────────
// Constraint 면(삼각형) 기반 시각화 헬퍼 함수들
// ────────────────────────────────────────────────────────────────

/**
 * Swing Limit 원뿔(Cone) 면 기반 시각화 생성
 */
static void GenerateSwingConeMesh(
	const FVector& JointPos,
	const FQuat& JointRotation,
	float Swing1Limit,
	float Swing2Limit,
	float ConeLength,
	const FVector4& Color,
	TArray<FVector>& OutVertices,
	TArray<uint32>& OutIndices,
	TArray<FVector4>& OutColors)
{
	// 각도를 라디안으로 변환 (89도로 클램프하여 tan 폭발 방지)
	float Swing1Rad = FMath::Min(Swing1Limit, 89.0f) * M_PI / 180.0f;
	float Swing2Rad = FMath::Min(Swing2Limit, 89.0f) * M_PI / 180.0f;

	// 원뿔 베이스 원의 점들 계산
	uint32 BaseVertexIndex = static_cast<uint32>(OutVertices.size());

	// 원뿔 꼭지점 (조인트 위치)
	OutVertices.push_back(JointPos);
	OutColors.push_back(Color);
	uint32 ApexIndex = BaseVertexIndex;

	// 원뿔 베이스 점들
	for (int32 i = 0; i <= ConeSegments; ++i)
	{
		float Angle = (i / static_cast<float>(ConeSegments)) * 2.0f * M_PI;

		// 타원형 원뿔 (Swing1 = Y축, Swing2 = Z축)
		float RadiusY = ConeLength * tan(Swing1Rad);
		float RadiusZ = ConeLength * tan(Swing2Rad);

		// 로컬 좌표에서 원뿔 베이스 점
		FVector LocalPoint(
			ConeLength,  // X축 방향이 원뿔 축
			RadiusY * cos(Angle),
			RadiusZ * sin(Angle)
		);

		// 월드 좌표로 변환
		FVector WorldPoint = JointPos + JointRotation.RotateVector(LocalPoint);
		OutVertices.push_back(WorldPoint);
		OutColors.push_back(Color);
	}

	// 원뿔 측면 삼각형 (꼭지점 → 베이스 원 순환)
	for (int32 i = 0; i < ConeSegments; ++i)
	{
		uint32 BaseI = BaseVertexIndex + 1 + i;
		uint32 BaseNext = BaseVertexIndex + 1 + ((i + 1) % (ConeSegments + 1));

		// 삼각형: Apex → BaseI → BaseNext
		OutIndices.push_back(ApexIndex);
		OutIndices.push_back(BaseI);
		OutIndices.push_back(BaseNext);
	}
}

/**
 * Twist Limit 부채꼴(Fan) 면 기반 시각화 생성
 */
static void GenerateTwistFanMesh(
	const FVector& JointPos,
	const FQuat& JointRotation,
	float TwistMin,
	float TwistMax,
	float ArcRadius,
	const FVector4& Color,
	TArray<FVector>& OutVertices,
	TArray<uint32>& OutIndices,
	TArray<FVector4>& OutColors)
{
	// 각도를 라디안으로 변환
	float TwistMinRad = TwistMin * M_PI / 180.0f;
	float TwistMaxRad = TwistMax * M_PI / 180.0f;

	// Twist 각도 범위
	float TwistRange = TwistMaxRad - TwistMinRad;
	int32 NumSegments = FMath::Max(4, static_cast<int32>(TwistRange / (M_PI / 8.0f)));

	uint32 BaseVertexIndex = static_cast<uint32>(OutVertices.size());

	// 중심점 (조인트 위치)
	OutVertices.push_back(JointPos);
	OutColors.push_back(Color);
	uint32 CenterIndex = BaseVertexIndex;

	// 원호 점들
	for (int32 i = 0; i <= NumSegments; ++i)
	{
		float T = static_cast<float>(i) / static_cast<float>(NumSegments);
		float Angle = TwistMinRad + T * TwistRange;

		// 로컬 좌표에서 원호 점 (X축이 Twist 축)
		FVector LocalPoint(
			0.0f,
			ArcRadius * cos(Angle),
			ArcRadius * sin(Angle)
		);

		// 월드 좌표로 변환
		FVector WorldPoint = JointPos + JointRotation.RotateVector(LocalPoint);
		OutVertices.push_back(WorldPoint);
		OutColors.push_back(Color);
	}

	// 부채꼴 삼각형들 (중심 → 원호 점들)
	for (int32 i = 0; i < NumSegments; ++i)
	{
		uint32 ArcI = BaseVertexIndex + 1 + i;
		uint32 ArcNext = BaseVertexIndex + 1 + i + 1;

		// 삼각형: Center → ArcI → ArcNext
		OutIndices.push_back(CenterIndex);
		OutIndices.push_back(ArcI);
		OutIndices.push_back(ArcNext);
	}
}

/**
 * Constraint 전체 면 기반 시각화 생성
 */
static void GenerateConstraintMeshVisualization(
	const FConstraintSetup& Constraint,
	const FVector& ParentPos,
	const FVector& ChildPos,
	const FQuat& JointRotation,
	bool bSelected,
	TArray<FVector>& OutVertices,
	TArray<uint32>& OutIndices,
	TArray<FVector4>& OutColors,
	FLinesBatch& OutLineBatch)
{
	// 색상 설정 (반투명)
	FVector4 SwingColor = bSelected
		? FVector4(0.0f, 1.0f, 0.5f, 0.4f)   // 선택: 밝은 초록
		: FVector4(0.0f, 0.8f, 0.4f, 0.25f);  // 기본: 초록 (25% 투명)
	FVector4 TwistColor = bSelected
		? FVector4(0.5f, 0.5f, 1.0f, 0.4f)   // 선택: 밝은 파랑
		: FVector4(0.3f, 0.3f, 0.8f, 0.25f);  // 기본: 파랑 (25% 투명)
	FVector4 LineColor = bSelected
		? FVector4(1.0f, 1.0f, 0.0f, 1.0f)   // 선택: 노랑
		: FVector4(1.0f, 0.5f, 0.0f, 1.0f);  // 기본: 주황

	// 조인트 위치 = 자식 본 위치
	FVector JointPos = ChildPos;

	// 1. 부모-자식 연결선 (라인으로 유지)
	OutLineBatch.Add(ParentPos, ChildPos, LineColor);

	// 2. 조인트 위치에 십자 마커 (라인)
	float MarkerSize = bSelected ? 0.03f : 0.02f;
	FVector XAxis = JointRotation.RotateVector(FVector(1, 0, 0)) * MarkerSize;
	FVector YAxis = JointRotation.RotateVector(FVector(0, 1, 0)) * MarkerSize;
	FVector ZAxis = JointRotation.RotateVector(FVector(0, 0, 1)) * MarkerSize;
	OutLineBatch.Add(JointPos - XAxis, JointPos + XAxis, FVector4(1, 0, 0, 1));  // X축: 빨강
	OutLineBatch.Add(JointPos - YAxis, JointPos + YAxis, FVector4(0, 1, 0, 1));  // Y축: 초록
	OutLineBatch.Add(JointPos - ZAxis, JointPos + ZAxis, FVector4(0, 0, 1, 1));  // Z축: 파랑

	// 3. 원뿔 길이 계산 (부모-자식 거리 기반)
	float Distance = (ChildPos - ParentPos).Size();
	float ConeLength = FMath::Max(0.02f, Distance * 0.3f);  // 본 거리의 30%

	// 4. Swing Limit 원뿔 면 (각도가 유의미할 때만)
	if (Constraint.Swing1Limit > 1.0f || Constraint.Swing2Limit > 1.0f)
	{
		GenerateSwingConeMesh(JointPos, JointRotation,
			Constraint.Swing1Limit, Constraint.Swing2Limit, ConeLength, SwingColor,
			OutVertices, OutIndices, OutColors);
	}

	// 5. Twist Limit 부채꼴 면 (각도가 유의미할 때만)
	float TwistRange = Constraint.TwistLimitMax - Constraint.TwistLimitMin;
	if (TwistRange > 1.0f && TwistRange < 359.0f)
	{
		float ArcRadius = ConeLength * 0.5f;
		GenerateTwistFanMesh(JointPos, JointRotation,
			Constraint.TwistLimitMin, Constraint.TwistLimitMax, ArcRadius, TwistColor,
			OutVertices, OutIndices, OutColors);
	}
}

/**
 * UBodySetup의 모든 Shape에 대해 와이어프레임 라인 좌표 생성
 */
static void GenerateBodyShapeLineCoordinates(
	const UBodySetup* BodySetup,
	const FTransform& BoneTransform,
	TArray<FVector>& OutStartPoints,
	TArray<FVector>& OutEndPoints)
{
	if (!BodySetup) return;

	// Sphere shapes
	for (const FKSphereElem& Sphere : BodySetup->AggGeom.SphereElems)
	{
		GenerateSphereLineCoordinates(Sphere, BoneTransform, OutStartPoints, OutEndPoints);
	}

	// Box shapes
	for (const FKBoxElem& Box : BodySetup->AggGeom.BoxElems)
	{
		GenerateBoxLineCoordinates(Box, BoneTransform, OutStartPoints, OutEndPoints);
	}

	// Capsule shapes
	for (const FKSphylElem& Capsule : BodySetup->AggGeom.SphylElems)
	{
		GenerateCapsuleLineCoordinates(Capsule, BoneTransform, OutStartPoints, OutEndPoints);
	}
}

SPhysicsAssetEditorWindow::SPhysicsAssetEditorWindow()
{
	WindowTitle = "Physics Asset Editor";
	bHasBottomPanel = false; // 기본적으로 하단 패널 숨김
	CreateSubWidgets();
}

SPhysicsAssetEditorWindow::~SPhysicsAssetEditorWindow()
{
	DestroySubWidgets();

	// Clean up tabs (ViewerState 정리)
	for (int i = 0; i < Tabs.Num(); ++i)
	{
		ViewerState* State = Tabs[i];
		PhysicsAssetEditorBootstrap::DestroyViewerState(State);
	}
	Tabs.Empty();
	ActiveState = nullptr;
}

ViewerState* SPhysicsAssetEditorWindow::CreateViewerState(const char* Name, UEditorAssetPreviewContext* Context)
{
	return PhysicsAssetEditorBootstrap::CreateViewerState(Name, World, Device, Context);
}

void SPhysicsAssetEditorWindow::DestroyViewerState(ViewerState*& State)
{
	PhysicsAssetEditorBootstrap::DestroyViewerState(State);
}

void SPhysicsAssetEditorWindow::OpenOrFocusTab(UEditorAssetPreviewContext* Context)
{
	// Context가 없거나 AssetPath가 비어있으면 새 탭 열기
	if (!Context || Context->AssetPath.empty())
	{
		char label[64];
		sprintf_s(label, "PhysicsAssetTab%d", Tabs.Num() + 1);
		OpenNewTab(label);
		return;
	}

	// 기존 탭 중 동일한 파일 경로를 가진 탭 검색
	for (int32 i = 0; i < Tabs.Num(); ++i)
	{
		PhysicsAssetEditorState* State = static_cast<PhysicsAssetEditorState*>(Tabs[i]);
		if (State && State->CurrentFilePath == Context->AssetPath)
		{
			// 기존 탭으로 전환
			ActiveTabIndex = i;
			ActiveState = Tabs[i];
			return;
		}
	}

	// 기존 탭이 없으면 새 탭 생성
	FString AssetName;
	const size_t lastSlash = Context->AssetPath.find_last_of("/\\");
	if (lastSlash != FString::npos)
		AssetName = Context->AssetPath.substr(lastSlash + 1);
	else
		AssetName = Context->AssetPath;

	// 확장자 제거
	const size_t dotPos = AssetName.find_last_of('.');
	if (dotPos != FString::npos)
		AssetName = AssetName.substr(0, dotPos);

	ViewerState* NewState = CreateViewerState(AssetName.c_str(), Context);
	if (NewState)
	{
		Tabs.Add(NewState);
		ActiveTabIndex = Tabs.Num() - 1;
		ActiveState = NewState;
	}
}

void SPhysicsAssetEditorWindow::OnRender()
{
	// If window is closed, request cleanup and don't render
	if (!bIsOpen)
	{
		USlateManager::GetInstance().RequestCloseDetachedWindow(this);
		return;
	}

	// Parent detachable window (movable, top-level) with solid background
	ImGuiWindowFlags flags = ImGuiWindowFlags_NoSavedSettings;

	if (!bInitialPlacementDone)
	{
		ImGui::SetNextWindowPos(ImVec2(Rect.Left, Rect.Top));
		ImGui::SetNextWindowSize(ImVec2(Rect.GetWidth(), Rect.GetHeight()));
		bInitialPlacementDone = true;
	}
	if (bRequestFocus)
	{
		ImGui::SetNextWindowFocus();
	}

	// Generate a unique window title to pass to ImGui
	char UniqueTitle[256];
	FString Title = GetWindowTitle();
	if (Tabs.Num() == 1 && ActiveState)
	{
		PhysicsAssetEditorState* PhysState = static_cast<PhysicsAssetEditorState*>(ActiveState);
		if (!PhysState->CurrentFilePath.empty())
		{
			std::filesystem::path fsPath(UTF8ToWide(PhysState->CurrentFilePath));
			FString AssetName = WideToUTF8(fsPath.filename().wstring());
			Title += " - " + AssetName;
		}
	}
	sprintf_s(UniqueTitle, sizeof(UniqueTitle), "%s###%p", Title.c_str(), this);

	bool bViewerVisible = false;
	if (ImGui::Begin(UniqueTitle, &bIsOpen, flags))
	{
		bViewerVisible = true;

		// 입력 라우팅을 위한 hover/focus 상태 캡처
		bIsWindowHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows);
		bIsWindowFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);

		// Render tab bar and switch active state
		RenderTabsAndToolbar(EViewerType::PhysicsAsset);

		// 마지막 탭을 닫은 경우 렌더링 중단
		if (!bIsOpen)
		{
			USlateManager::GetInstance().RequestCloseDetachedWindow(this);
			ImGui::End();
			return;
		}

		ImVec2 pos = ImGui::GetWindowPos();
		ImVec2 size = ImGui::GetWindowSize();
		Rect.Left = pos.x; Rect.Top = pos.y; Rect.Right = pos.x + size.x; Rect.Bottom = pos.y + size.y; Rect.UpdateMinMax();

		ImVec2 contentAvail = ImGui::GetContentRegionAvail();
		float totalWidth = contentAvail.x;
		float totalHeight = contentAvail.y;

		float splitterWidth = 4.0f; // 분할선 두께

		float leftWidth = totalWidth * LeftPanelRatio;
		float rightWidth = totalWidth * RightPanelRatio;
		float centerWidth = totalWidth - leftWidth - rightWidth - (splitterWidth * 2);

		// 중앙 패널이 음수가 되지 않도록 보정 (안전장치)
		if (centerWidth < 0.0f)
		{
			centerWidth = 0.0f;
		}

		// Remove spacing between panels
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));

		// Left panel - Skeleton Tree & Bodies
		ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
		ImGui::BeginChild("LeftPanel", ImVec2(leftWidth, totalHeight), true, ImGuiWindowFlags_NoScrollbar);
		ImGui::PopStyleVar();
		RenderLeftPanel(leftWidth);
		ImGui::EndChild();

		ImGui::SameLine(0, 0); // No spacing between panels

		// Left splitter (드래그 가능한 분할선)
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.3f, 0.3f, 0.5f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.4f, 0.4f, 0.4f, 0.7f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.5f, 0.5f, 0.5f, 0.9f));
		ImGui::Button("##LeftSplitter", ImVec2(splitterWidth, totalHeight));
		ImGui::PopStyleColor(3);

		if (ImGui::IsItemHovered())
			ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);

		if (ImGui::IsItemActive())
		{
			float delta = ImGui::GetIO().MouseDelta.x;
			if (delta != 0.0f)
			{
				float newLeftRatio = LeftPanelRatio + delta / totalWidth;
				float maxLeftRatio = 1.0f - RightPanelRatio - (splitterWidth * 2) / totalWidth;
				LeftPanelRatio = std::max(0.1f, std::min(newLeftRatio, maxLeftRatio));
			}
		}

		ImGui::SameLine(0, 0);

		// Center panel (viewport area)
		if (centerWidth > 0.0f)
		{
			ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
			ImGui::BeginChild("CenterPanel", ImVec2(centerWidth, totalHeight), false,
				ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoNavInputs | ImGuiWindowFlags_NoNavFocus);
			ImGui::PopStyleVar();

			// 뷰어 툴바 렌더링 (뷰포트 상단)
			RenderViewerToolbar();

			// 툴바 아래 뷰포트 영역
			ImVec2 viewportPos = ImGui::GetCursorScreenPos();
			float remainingWidth = ImGui::GetContentRegionAvail().x;
			float remainingHeight = ImGui::GetContentRegionAvail().y;

			// 뷰포트 영역 설정
			CenterRect.Left = viewportPos.x;
			CenterRect.Top = viewportPos.y;
			CenterRect.Right = viewportPos.x + remainingWidth;
			CenterRect.Bottom = viewportPos.y + remainingHeight;
			CenterRect.UpdateMinMax();

			// 뷰포트 렌더링 (텍스처에)
			OnRenderViewport();

			// ImGui::Image로 결과 텍스처 표시
			if (ActiveState && ActiveState->Viewport)
			{
				ID3D11ShaderResourceView* SRV = ActiveState->Viewport->GetSRV();
				if (SRV)
				{
					ImGui::Image((void*)SRV, ImVec2(remainingWidth, remainingHeight));
					ActiveState->Viewport->SetViewportHovered(ImGui::IsItemHovered());
				}
				else
				{
					ImGui::Dummy(ImVec2(remainingWidth, remainingHeight));
					ActiveState->Viewport->SetViewportHovered(false);
				}
			}
			else
			{
				ImGui::Dummy(ImVec2(remainingWidth, remainingHeight));
			}

			ImGui::EndChild(); // CenterPanel

			ImGui::SameLine(0, 0);
		}
		else
		{
			CenterRect = FRect(0, 0, 0, 0);
			CenterRect.UpdateMinMax();
		}

		// Right splitter (드래그 가능한 분할선)
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.3f, 0.3f, 0.5f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.4f, 0.4f, 0.4f, 0.7f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.5f, 0.5f, 0.5f, 0.9f));
		ImGui::Button("##RightSplitter", ImVec2(splitterWidth, totalHeight));
		ImGui::PopStyleColor(3);

		if (ImGui::IsItemHovered())
			ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);

		if (ImGui::IsItemActive())
		{
			float delta = ImGui::GetIO().MouseDelta.x;
			if (delta != 0.0f)
			{
				float newRightRatio = RightPanelRatio - delta / totalWidth;
				float maxRightRatio = 1.0f - LeftPanelRatio - (splitterWidth * 2) / totalWidth;
				RightPanelRatio = std::max(0.1f, std::min(newRightRatio, maxRightRatio));
			}
		}

		ImGui::SameLine(0, 0);

		// Right panel - Properties
		ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
		ImGui::BeginChild("RightPanel", ImVec2(rightWidth, totalHeight), true);
		ImGui::PopStyleVar();
		RenderRightPanel();
		ImGui::EndChild();

		// Pop the ItemSpacing style
		ImGui::PopStyleVar();
	}
	ImGui::End();

	// If collapsed or not visible, clear the center rect
	if (!bViewerVisible)
	{
		CenterRect = FRect(0, 0, 0, 0);
		CenterRect.UpdateMinMax();
		bIsWindowHovered = false;
		bIsWindowFocused = false;
	}

	// If window was closed via X button, notify the manager to clean up
	if (!bIsOpen)
	{
		USlateManager::GetInstance().RequestCloseDetachedWindow(this);
	}

	bRequestFocus = false;
}

void SPhysicsAssetEditorWindow::OnUpdate(float DeltaSeconds)
{
	SViewerWindow::OnUpdate(DeltaSeconds);

	// Sub Widget 에디터 상태 업데이트
	UpdateSubWidgetEditorState();

	// Sub Widget Update 호출
	if (SkeletonTreeWidget) SkeletonTreeWidget->Update();
	if (BodyPropertiesWidget) BodyPropertiesWidget->Update();
	if (ConstraintPropertiesWidget) ConstraintPropertiesWidget->Update();
	if (ToolsWidget) ToolsWidget->Update();

	// Delete 키로 선택 항목 삭제
	PhysicsAssetEditorState* State = GetActivePhysicsState();
	if (State && ImGui::IsKeyPressed(ImGuiKey_Delete) && !ImGui::GetIO().WantTextInput)
	{
		if (State->bBodySelectionMode && State->SelectedBodyIndex >= 0)
		{
			RemoveSelectedBody();
		}
		else if (!State->bBodySelectionMode && State->SelectedConstraintIndex >= 0)
		{
			RemoveSelectedConstraint();
		}
	}

	// Shape 프리뷰 업데이트
	if (State)
	{
		// ─────────────────────────────────────────────────
		// 기즈모 연동: 선택된 바디의 트랜스폼 편집
		// ─────────────────────────────────────────────────
		if (State->bBodySelectionMode && State->SelectedBodyIndex >= 0 && State->World)
		{
			AGizmoActor* Gizmo = State->World->GetGizmoActor();
			bool bCurrentlyDragging = Gizmo && Gizmo->GetbIsDragging();

			// 드래그 첫 프레임인지 확인 (World→Local 변환 오차 방지)
			bool bIsFirstDragFrame = bCurrentlyDragging && !State->bWasGizmoDragging;

			if (bCurrentlyDragging && !bIsFirstDragFrame)
			{
				// 첫 프레임이 아닐 때만 기즈모로부터 트랜스폼 업데이트
				UpdateBodyTransformFromGizmo();
			}
			else if (!bCurrentlyDragging)
			{
				// 드래그 중이 아니면 기즈모 앵커를 바디 위치로 이동
				RepositionAnchorToBody(State->SelectedBodyIndex);
			}

			// 드래그 상태 업데이트 (다음 프레임에서 첫 프레임 감지용)
			State->bWasGizmoDragging = bCurrentlyDragging;
		}
		else
		{
			State->bWasGizmoDragging = false;
		}

		// 라인 재구성이 필요한 경우 (바디/제약조건 추가/제거)
		if (State->bShapeLinesNeedRebuild)
		{
			RebuildShapeLines();
			State->bShapeLinesNeedRebuild = false;
			State->bSelectedBodyLinesNeedUpdate = false;  // 전체 재구성 시 개별 업데이트 불필요
			State->bSelectionColorDirty = false;  // 재구성 시 색상도 업데이트됨
			State->CachedSelectedBodyIndex = State->SelectedBodyIndex;
			State->CachedSelectedConstraintIndex = State->SelectedConstraintIndex;
		}
		// 선택된 바디의 속성만 변경된 경우 (좌표만 업데이트, new/delete 없음)
		else if (State->bSelectedBodyLinesNeedUpdate)
		{
			UpdateSelectedBodyLines();
			State->bSelectedBodyLinesNeedUpdate = false;
		}
		// 선택만 변경된 경우 (색상만 업데이트)
		else if (State->bSelectionColorDirty)
		{
			UpdateSelectionColors();
			State->bSelectionColorDirty = false;
			State->CachedSelectedBodyIndex = State->SelectedBodyIndex;
			State->CachedSelectedConstraintIndex = State->SelectedConstraintIndex;
		}
	}
}

void SPhysicsAssetEditorWindow::PreRenderViewportUpdate()
{
	// 삼각형 배치는 ULineComponent를 통해 처리됨 (RebuildShapeLines에서 설정)
	// 별도 처리 불필요
}

void SPhysicsAssetEditorWindow::OnSave()
{
	SavePhysicsAsset();
}

void SPhysicsAssetEditorWindow::RenderTabsAndToolbar(EViewerType CurrentViewerType)
{
	// 탭바만 렌더링 (Skeletal Mesh 뷰어 전환 버튼 제외 - SViewerWindow 베이스 호출 안 함)
	if (!ImGui::BeginTabBar("PhysicsAssetEditorTabs",
		ImGuiTabBarFlags_AutoSelectNewTabs | ImGuiTabBarFlags_Reorderable))
		return;

	// 탭 렌더링
	for (int i = 0; i < Tabs.Num(); ++i)
	{
		ViewerState* State = Tabs[i];
		PhysicsAssetEditorState* PState = static_cast<PhysicsAssetEditorState*>(State);
		bool open = true;

		// 동적 탭 이름 생성
		FString TabDisplayName;
		if (PState->CurrentFilePath.empty())
		{
			TabDisplayName = "Untitled";
		}
		else
		{
			// 파일 경로에서 파일명만 추출 (확장자 제외)
			size_t lastSlash = PState->CurrentFilePath.find_last_of("/\\");
			FString filename = (lastSlash != FString::npos)
				? PState->CurrentFilePath.substr(lastSlash + 1)
				: PState->CurrentFilePath;

			// 확장자 제거
			size_t dotPos = filename.find_last_of('.');
			if (dotPos != FString::npos)
				filename = filename.substr(0, dotPos);

			TabDisplayName = filename;
		}

		// 수정됨 표시 추가
		if (PState->bIsDirty)
		{
			TabDisplayName += "*";
		}

		// ImGui ID 충돌 방지
		TabDisplayName += "##";
		TabDisplayName += State->Name.ToString();

		if (ImGui::BeginTabItem(TabDisplayName.c_str(), &open))
		{
			if (ActiveState != State)
			{
				ActiveTabIndex = i;
				ActiveState = State;
				UpdateSubWidgetEditorState();  // 탭 전환 시 sub widget 상태 즉시 갱신
			}
			ImGui::EndTabItem();
		}
		if (!open)
		{
			CloseTab(i);
			UpdateSubWidgetEditorState();  // 탭 닫힌 직후 sub widget 상태 갱신 (dangling pointer 방지)
			ImGui::EndTabBar();
			return;
		}
	}

	// + 버튼 (새 탭 추가)
	if (ImGui::TabItemButton("+", ImGuiTabItemFlags_Trailing))
	{
		int maxViewerNum = 0;
		for (int i = 0; i < Tabs.Num(); ++i)
		{
			const FString& tabName = Tabs[i]->Name.ToString();
			const char* prefix = "PhysicsAssetTab";
			if (strncmp(tabName.c_str(), prefix, strlen(prefix)) == 0)
			{
				const char* numberPart = tabName.c_str() + strlen(prefix);
				int num = atoi(numberPart);
				if (num > maxViewerNum)
				{
					maxViewerNum = num;
				}
			}
		}

		char label[64];
		sprintf_s(label, "PhysicsAssetTab%d", maxViewerNum + 1);
		OpenNewTab(label);
	}
	ImGui::EndTabBar();

	// Physics Asset Editor 전용 툴바
	RenderToolbar();
}

void SPhysicsAssetEditorWindow::RenderLeftPanel(float PanelWidth)
{
	PhysicsAssetEditorState* State = GetActivePhysicsState();
	if (!State) return;

	ImGuiStyle& style = ImGui::GetStyle();

	// ─────────────────────────────────────────────────
	// SELECT MESH 섹션 (로드된 메시 목록에서 선택)
	// ─────────────────────────────────────────────────
	ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 6);
	ImGui::Text("SELECT MESH");
	ImGui::Dummy(ImVec2(0, 6));
	ImGui::Separator();
	ImGui::Dummy(ImVec2(0, 6));

	// 현재 선택된 메시 표시
	FString CurrentMeshName = State->LoadedMeshPath.empty() ? "(None)" : ExtractFileName(State->LoadedMeshPath);
	ImGui::Text("Current: %s", CurrentMeshName.c_str());
	ImGui::Dummy(ImVec2(0, 4));

	// 메시 목록 스크롤 영역
	float meshListHeight = 120.0f;
	ImGui::BeginChild("MeshList", ImVec2(0, meshListHeight), true);

	// SkeletalMesh 목록 (ResourceManager에서 가져오기)
	TArray<FString> MeshPaths = UResourceManager::GetInstance().GetAllFilePaths<USkeletalMesh>();

	if (MeshPaths.IsEmpty())
	{
		ImGui::TextDisabled("No skeletal meshes loaded");
		ImGui::TextDisabled("Load FBX files first");
	}
	else
	{
		for (const FString& Path : MeshPaths)
		{
			bool bSelected = (State->LoadedMeshPath == Path);

			// 파일명만 표시
			FString DisplayName = ExtractFileName(Path);

			if (ImGui::Selectable(DisplayName.c_str(), bSelected))
			{
				// 현재 메시와 다르면 변경
				if (!bSelected)
				{
					// 미저장 확인 후 메시 로드
					CheckUnsavedChangesAndExecute([this, Path]()
					{
						PhysicsAssetEditorState* S = GetActivePhysicsState();
						if (S)
						{
							LoadMeshAndResetPhysics(S, Path);
						}
					});
				}
			}

			// 툴팁으로 전체 경로 표시
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("%s", Path.c_str());
			}
		}
	}

	ImGui::EndChild();

	ImGui::Dummy(ImVec2(0, 8));
	ImGui::Separator();
	ImGui::Dummy(ImVec2(0, 8));

	// ─────────────────────────────────────────────────
	// 남은 높이 계산 (Skeleton 60%, Graph 40%)
	// ─────────────────────────────────────────────────
	float remainingHeight = ImGui::GetContentRegionAvail().y;
	float graphSectionHeight = remainingHeight * 0.4f;
	float skeletonHeight = remainingHeight * 0.6f - 40.0f;

	// ─────────────────────────────────────────────────
	// SKELETON 섹션 (본 계층 구조만 표시)
	// ─────────────────────────────────────────────────
	ImGui::Text("SKELETON");
	ImGui::Dummy(ImVec2(0, 4));
	ImGui::Separator();
	ImGui::Dummy(ImVec2(0, 4));

	if (skeletonHeight > 50.0f)
	{
		ImGui::BeginChild("SkeletonTreeScroll", ImVec2(0, skeletonHeight), false);
		if (SkeletonTreeWidget) SkeletonTreeWidget->RenderWidget();
		ImGui::EndChild();
	}

	ImGui::Dummy(ImVec2(0, 4));
	ImGui::Separator();
	ImGui::Dummy(ImVec2(0, 4));

	// ─────────────────────────────────────────────────
	// GRAPH 섹션 (GraphPivotBodyIndex 기준 그래프)
	// 더블클릭으로만 그래프 기준 변경, 일반 클릭은 선택만 변경
	// ─────────────────────────────────────────────────
	ImGui::Text("GRAPH");
	ImGui::Dummy(ImVec2(0, 4));
	ImGui::Separator();
	ImGui::Dummy(ImVec2(0, 4));

	float graphHeight = ImGui::GetContentRegionAvail().y;

	// 그래프의 기준 바디 (State에 저장된 값 사용)
	int32 pivotBodyIdx = State->GraphPivotBodyIndex;

	if (pivotBodyIdx < 0 || pivotBodyIdx >= static_cast<int32>(State->EditingAsset->BodySetups.size()))
	{
		ImGui::TextDisabled("Double-click a body to view graph");
	}
	else
	{
		UBodySetup* pivotBody = State->EditingAsset->BodySetups[pivotBodyIdx];
		if (!pivotBody) return;

		// 연결된 컨스트레인트 수집 (항상 pivot -> constraint -> other 방향으로)
		struct FConstraintLink
		{
			int32 ConstraintIndex;
			int32 OtherBodyIndex;
		};
		std::vector<FConstraintLink> connectedConstraints;

		for (int32 i = 0; i < static_cast<int32>(State->EditingAsset->ConstraintSetups.size()); ++i)
		{
			const FConstraintSetup& Constraint = State->EditingAsset->ConstraintSetups[i];
			if (Constraint.ParentBodyIndex == pivotBodyIdx)
			{
				connectedConstraints.push_back({ i, Constraint.ChildBodyIndex });
			}
			else if (Constraint.ChildBodyIndex == pivotBodyIdx)
			{
				connectedConstraints.push_back({ i, Constraint.ParentBodyIndex });
			}
		}

		// ImNodes 그래프 에디터
		ImNodes::BeginNodeEditor();

		// 레이아웃 상수
		const float ColumnSpacing = 200.0f;
		const float RowSpacing = 100.0f;
		const float TextWrapWidth = 140.0f;  // 텍스트 줄바꿈 너비

		float col1X = 50.0f;
		float col2X = col1X + ColumnSpacing;
		float col3X = col2X + ColumnSpacing;
		float centerY = (connectedConstraints.size() * RowSpacing) / 2.0f;

		// ─────────────────────────────────────────────────
		// 열 1: 기준 바디 노드
		// ─────────────────────────────────────────────────
		ImNodes::SetNodeGridSpacePos(pivotBodyIdx, ImVec2(col1X, centerY));

		bool bPivotSelected = (State->bBodySelectionMode && State->SelectedBodyIndex == pivotBodyIdx);
		if (bPivotSelected)
		{
			ImNodes::PushColorStyle(ImNodesCol_NodeBackground, IM_COL32(60, 100, 60, 255));
			ImNodes::PushColorStyle(ImNodesCol_TitleBar, IM_COL32(40, 80, 40, 255));
		}
		else
		{
			ImNodes::PushColorStyle(ImNodesCol_NodeBackground, IM_COL32(50, 80, 50, 255));
			ImNodes::PushColorStyle(ImNodesCol_TitleBar, IM_COL32(35, 60, 35, 255));
		}

		ImNodes::BeginNode(pivotBodyIdx);
		ImNodes::BeginNodeTitleBar();
		ImGui::Text("Body");
		ImNodes::EndNodeTitleBar();

		// 바디 이름과 Shape 개수 + 오른쪽 핀
		ImNodes::BeginOutputAttribute(pivotBodyIdx * 2);
		ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + TextWrapWidth);
		ImGui::TextWrapped("%s", pivotBody->BoneName.ToString().c_str());
		int32 shapeCount = pivotBody->AggGeom.GetElementCount();
		ImGui::Text("%d shape(s)", shapeCount);
		ImGui::PopTextWrapPos();
		ImNodes::EndOutputAttribute();

		ImNodes::EndNode();
		ImNodes::PopColorStyle();
		ImNodes::PopColorStyle();

		// ─────────────────────────────────────────────────
		// 열 2: 컨스트레인트 노드들 + 열 3: 연결된 바디들
		// ─────────────────────────────────────────────────
		for (size_t i = 0; i < connectedConstraints.size(); ++i)
		{
			const FConstraintLink& link = connectedConstraints[i];
			float rowY = i * RowSpacing;

			int32 constraintNodeId = 10000 + link.ConstraintIndex;
			int32 constraintInputAttr = 20000 + link.ConstraintIndex * 2;
			int32 constraintOutputAttr = 20000 + link.ConstraintIndex * 2 + 1;

			// 컨스트레인트 노드 (열 2)
			ImNodes::SetNodeGridSpacePos(constraintNodeId, ImVec2(col2X, rowY));

			bool bConstraintSelected = (!State->bBodySelectionMode && State->SelectedConstraintIndex == link.ConstraintIndex);
			if (bConstraintSelected)
			{
				ImNodes::PushColorStyle(ImNodesCol_NodeBackground, IM_COL32(120, 100, 40, 255));
				ImNodes::PushColorStyle(ImNodesCol_TitleBar, IM_COL32(100, 80, 30, 255));
			}
			else
			{
				ImNodes::PushColorStyle(ImNodesCol_NodeBackground, IM_COL32(80, 70, 50, 255));
				ImNodes::PushColorStyle(ImNodesCol_TitleBar, IM_COL32(60, 50, 35, 255));
			}

			ImNodes::BeginNode(constraintNodeId);
			ImNodes::BeginNodeTitleBar();
			ImGui::Text("Constraint");
			ImNodes::EndNodeTitleBar();

			// From : To 표시 (ParentBody : ChildBody)
			const FConstraintSetup& constraint = State->EditingAsset->ConstraintSetups[link.ConstraintIndex];
			FString parentName = "?";
			FString childName = "?";
			if (constraint.ParentBodyIndex >= 0 && constraint.ParentBodyIndex < static_cast<int32>(State->EditingAsset->BodySetups.size()))
			{
				parentName = State->EditingAsset->BodySetups[constraint.ParentBodyIndex]->BoneName.ToString();
			}
			if (constraint.ChildBodyIndex >= 0 && constraint.ChildBodyIndex < static_cast<int32>(State->EditingAsset->BodySetups.size()))
			{
				childName = State->EditingAsset->BodySetups[constraint.ChildBodyIndex]->BoneName.ToString();
			}
			// 핀과 내용을 같은 줄에 배치 (긴 이름은 줄바꿈)
			ImNodes::BeginInputAttribute(constraintInputAttr);
			ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + TextWrapWidth);
			ImGui::TextWrapped("%s : %s", parentName.c_str(), childName.c_str());
			ImGui::PopTextWrapPos();
			ImNodes::EndInputAttribute();
			ImGui::SameLine();
			ImNodes::BeginOutputAttribute(constraintOutputAttr);
			ImNodes::EndOutputAttribute();

			ImNodes::EndNode();
			ImNodes::PopColorStyle();
			ImNodes::PopColorStyle();

			// 연결된 바디 노드 (열 3)
			if (link.OtherBodyIndex >= 0 && link.OtherBodyIndex < static_cast<int32>(State->EditingAsset->BodySetups.size()))
			{
				UBodySetup* otherBody = State->EditingAsset->BodySetups[link.OtherBodyIndex];
				if (otherBody)
				{
					int32 otherNodeId = 30000 + static_cast<int32>(i);

					ImNodes::SetNodeGridSpacePos(otherNodeId, ImVec2(col3X, rowY));

					bool bOtherSelected = (State->bBodySelectionMode && State->SelectedBodyIndex == link.OtherBodyIndex);
					if (bOtherSelected)
					{
						ImNodes::PushColorStyle(ImNodesCol_NodeBackground, IM_COL32(60, 90, 120, 255));
						ImNodes::PushColorStyle(ImNodesCol_TitleBar, IM_COL32(50, 75, 100, 255));
					}
					else
					{
						ImNodes::PushColorStyle(ImNodesCol_NodeBackground, IM_COL32(50, 70, 90, 255));
						ImNodes::PushColorStyle(ImNodesCol_TitleBar, IM_COL32(40, 55, 70, 255));
					}

					ImNodes::BeginNode(otherNodeId);
					ImNodes::BeginNodeTitleBar();
					ImGui::Text("Body");
					ImNodes::EndNodeTitleBar();

					// 바디 이름과 Shape 개수 + 왼쪽 핀
					ImNodes::BeginInputAttribute(40000 + static_cast<int32>(i) * 2 + 1);
					ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + TextWrapWidth);
					ImGui::TextWrapped("%s", otherBody->BoneName.ToString().c_str());
					int32 otherShapeCount = otherBody->AggGeom.GetElementCount();
					ImGui::Text("%d shape(s)", otherShapeCount);
					ImGui::PopTextWrapPos();
					ImNodes::EndInputAttribute();

					ImNodes::EndNode();
					ImNodes::PopColorStyle();
					ImNodes::PopColorStyle();

					// 링크: 항상 왼쪽→오른쪽 (기준바디.Out -> Constraint.In, Constraint.Out -> 상대바디.In)
					ImNodes::Link(static_cast<int>(i) * 2, pivotBodyIdx * 2, constraintInputAttr);
					ImNodes::Link(static_cast<int>(i) * 2 + 1, constraintOutputAttr, 40000 + static_cast<int32>(i) * 2 + 1);
				}
			}
		}

		ImNodes::MiniMap(0.15f, ImNodesMiniMapLocation_BottomRight);
		ImNodes::EndNodeEditor();

		// ─────────────────────────────────────────────────
		// 상호작용 처리
		// - 일반 클릭: 선택만 변경
		// - 더블클릭: 바디 노드만 그래프 기준 변경
		// ─────────────────────────────────────────────────
		int hoveredNode = -1;
		if (ImNodes::IsNodeHovered(&hoveredNode))
		{
			// 더블클릭 처리 (바디 노드만)
			if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
			{
				if (hoveredNode >= 30000)
				{
					// 연결된 바디 노드 더블클릭 -> 그래프 기준 변경
					int32 linkIndex = hoveredNode - 30000;
					if (linkIndex >= 0 && linkIndex < static_cast<int32>(connectedConstraints.size()))
					{
						State->GraphPivotBodyIndex = connectedConstraints[linkIndex].OtherBodyIndex;
						State->SelectBody(connectedConstraints[linkIndex].OtherBodyIndex);
					}
				}
				else if (hoveredNode >= 0 && hoveredNode < 10000)
				{
					// 기준 바디 노드 더블클릭 (이미 기준이므로 선택만)
					State->SelectBody(hoveredNode);
				}
				// 컨스트레인트 노드 더블클릭 -> 아무것도 안 함
			}
			// 일반 클릭 처리 (선택만 변경, 그래프 기준은 유지)
			else if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
			{
				if (hoveredNode >= 10000 && hoveredNode < 20000)
				{
					// 컨스트레인트 노드 클릭 -> 선택만 변경
					State->SelectConstraint(hoveredNode - 10000);
				}
				else if (hoveredNode >= 30000)
				{
					// 연결된 바디 노드 클릭 -> 선택만 변경
					int32 linkIndex = hoveredNode - 30000;
					if (linkIndex >= 0 && linkIndex < static_cast<int32>(connectedConstraints.size()))
					{
						State->SelectBody(connectedConstraints[linkIndex].OtherBodyIndex);
					}
				}
				else if (hoveredNode >= 0 && hoveredNode < 10000)
				{
					// 기준 바디 노드 클릭 -> 선택만 변경
					State->SelectBody(hoveredNode);
				}
			}
		}
	}
}

void SPhysicsAssetEditorWindow::RenderRightPanel()
{
	PhysicsAssetEditorState* State = GetActivePhysicsState();
	if (!State || !State->EditingAsset) return;

	float totalHeight = ImGui::GetContentRegionAvail().y;
	float splitterHeight = 4.0f;

	// === DETAILS 섹션 (상단 60%) ===
	float detailsHeight = totalHeight * 0.6f - splitterHeight * 0.5f;

	ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 6);
	ImGui::Text("DETAILS");
	ImGui::Spacing();
	ImGui::Separator();
	ImGui::Spacing();

	ImGui::BeginChild("DetailsPanel", ImVec2(0, detailsHeight - 40), false);
	{
		// 선택된 바디 또는 제약 조건의 속성 표시
		if (State->bBodySelectionMode && State->SelectedBodyIndex >= 0)
		{
			if (BodyPropertiesWidget) BodyPropertiesWidget->RenderWidget();
		}
		else if (!State->bBodySelectionMode && State->SelectedConstraintIndex >= 0)
		{
			if (ConstraintPropertiesWidget) ConstraintPropertiesWidget->RenderWidget();
		}
		else
		{
			ImGui::TextDisabled("Select a body or constraint to view details");
		}
	}
	ImGui::EndChild();

	// === 수평 분할선 ===
	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.3f, 0.3f, 0.5f));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.4f, 0.4f, 0.4f, 0.8f));
	ImGui::Button("##Splitter", ImVec2(-1, splitterHeight));
	ImGui::PopStyleColor(2);

	// === TOOLS 섹션 (하단 40%) ===
	ImGui::Spacing();
	ImGui::Text("TOOLS");
	ImGui::Spacing();
	ImGui::Separator();
	ImGui::Spacing();

	ImGui::BeginChild("ToolsPanel", ImVec2(0, 0), false);
	{
		if (ToolsWidget)
		{
			ToolsWidget->RenderWidget();
		}
	}
	ImGui::EndChild();
}

void SPhysicsAssetEditorWindow::RenderBottomPanel()
{
	// 더 이상 사용하지 않음 - Graph가 좌측 패널로 이동됨
}

void SPhysicsAssetEditorWindow::OnSkeletalMeshLoaded(ViewerState* State, const FString& Path)
{
	PhysicsAssetEditorState* PhysicsState = static_cast<PhysicsAssetEditorState*>(State);
	if (!PhysicsState || !PhysicsState->EditingAsset) return;

	// Physics Asset에 스켈레탈 메시 경로 저장
	PhysicsState->EditingAsset->SkeletalMeshPath = Path;
	PhysicsState->RequestLinesRebuild();

	UE_LOG("[SPhysicsAssetEditorWindow] Skeletal mesh loaded: %s", Path.c_str());
}

// ────────────────────────────────────────────────────────────────
// 툴바
// ────────────────────────────────────────────────────────────────

void SPhysicsAssetEditorWindow::RenderToolbar()
{
	PhysicsAssetEditorState* State = GetActivePhysicsState();
	if (!State) return;

	// 파일 관련 버튼
	if (ImGui::Button("Save"))
	{
		SavePhysicsAsset();
	}
	ImGui::SameLine();

	if (ImGui::Button("Save As"))
	{
		SavePhysicsAssetAs();
	}
	ImGui::SameLine();

	if (ImGui::Button("Load"))
	{
		LoadPhysicsAsset();
	}
	ImGui::SameLine();

	ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
	ImGui::SameLine();

	// Simulate 버튼 (placeholder)
	if (ImGui::Button("Simulate"))
	{
		// TODO: 시뮬레이션 기능 구현
	}
	ImGui::SameLine();

	ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
	ImGui::SameLine();

	// 표시 옵션
	if (ImGui::Checkbox("Meshes", &State->bShowMesh))
	{
		// 메시 가시성 토글
		if (auto* comp = State->PreviewActor->GetSkeletalMeshComponent())
		{
			comp->SetVisibility(State->bShowMesh);
		}
	}
	ImGui::SameLine();
	if (ImGui::Checkbox("Bodies", &State->bShowBodies))
	{
		if (State->BodyPreviewLineComponent)
			State->BodyPreviewLineComponent->SetLineVisible(State->bShowBodies);
	}
	ImGui::SameLine();
	if (ImGui::Checkbox("Constraints", &State->bShowConstraints))
	{
		if (State->ConstraintPreviewLineComponent)
			State->ConstraintPreviewLineComponent->SetLineVisible(State->bShowConstraints);
	}
}

// ────────────────────────────────────────────────────────────────
// Shape 라인 관리
// ────────────────────────────────────────────────────────────────

void SPhysicsAssetEditorWindow::RebuildShapeLines()
{
	PhysicsAssetEditorState* State = GetActivePhysicsState();
	if (!State || !State->EditingAsset) return;

	ULineComponent* BodyLineComp = State->BodyPreviewLineComponent;
	ULineComponent* ConstraintLineComp = State->ConstraintPreviewLineComponent;

	// ─────────────────────────────────────────────────
	// 바디 라인 재구성 (FLinesBatch 기반 - DOD)
	// ─────────────────────────────────────────────────
	State->BodyLinesBatch.Clear();
	State->BodyLineRanges.Empty();
	State->BodyLineRanges.SetNum(static_cast<int32>(State->EditingAsset->BodySetups.size()));

	// 예상 라인 수 예약 (대략 바디당 40개 라인)
	State->BodyLinesBatch.Reserve(static_cast<int32>(State->EditingAsset->BodySetups.size()) * 40);

	USkeletalMeshComponent* MeshComp = State->GetPreviewMeshComponent();

	for (int32 i = 0; i < static_cast<int32>(State->EditingAsset->BodySetups.size()); ++i)
	{
		UBodySetup* Body = State->EditingAsset->BodySetups[i];
		if (!Body) continue;

		// 바디 라인 범위 시작
		PhysicsAssetEditorState::FBodyLineRange& Range = State->BodyLineRanges[i];
		Range.StartIndex = State->BodyLinesBatch.Num();

		// 선택된 바디는 다른 색상
		FVector4 Color = (State->bBodySelectionMode && State->SelectedBodyIndex == i)
			? FVector4(0.0f, 1.0f, 0.0f, 1.0f)  // 선택: 녹색
			: FVector4(0.0f, 0.5f, 1.0f, 1.0f); // 기본: 파랑

		// 본 트랜스폼 가져오기
		FTransform BoneTransform;
		if (MeshComp && Body->BoneIndex >= 0)
		{
			BoneTransform = MeshComp->GetBoneWorldTransform(Body->BoneIndex);
		}

		// 헬퍼 함수로 좌표 생성 (AggGeom의 모든 Shape 처리)
		TArray<FVector> StartPoints, EndPoints;
		GenerateBodyShapeLineCoordinates(Body, BoneTransform, StartPoints, EndPoints);

		// FLinesBatch에 직접 추가 (NewObject 없음!)
		for (int32 j = 0; j < StartPoints.Num(); ++j)
		{
			State->BodyLinesBatch.Add(StartPoints[j], EndPoints[j], Color);
		}

		// 바디 라인 범위 종료
		Range.Count = State->BodyLinesBatch.Num() - Range.StartIndex;
	}

	// ULineComponent에 배치 데이터 설정
	if (BodyLineComp)
	{
		BodyLineComp->ClearLines();  // 기존 ULine 정리
		BodyLineComp->SetDirectBatch(State->BodyLinesBatch);
		BodyLineComp->SetLineVisible(State->bShowBodies);
	}

	// ─────────────────────────────────────────────────
	// 제약 조건 시각화 재구성 (면 기반 + 라인)
	// ─────────────────────────────────────────────────
	State->ConstraintLinesBatch.Clear();
	State->ConstraintTrianglesBatch.Clear();

	// 예상 크기 예약
	int32 NumConstraints = static_cast<int32>(State->EditingAsset->ConstraintSetups.size());
	State->ConstraintLinesBatch.Reserve(NumConstraints * 10);  // 연결선 + 축 마커
	State->ConstraintTrianglesBatch.Reserve(NumConstraints * 50, NumConstraints * 100);  // 원뿔 + 부채꼴

	for (int32 i = 0; i < NumConstraints; ++i)
	{
		const FConstraintSetup& Constraint = State->EditingAsset->ConstraintSetups[i];
		if (Constraint.ParentBodyIndex < 0 || Constraint.ChildBodyIndex < 0) continue;
		if (Constraint.ParentBodyIndex >= static_cast<int32>(State->EditingAsset->BodySetups.size())) continue;
		if (Constraint.ChildBodyIndex >= static_cast<int32>(State->EditingAsset->BodySetups.size())) continue;

		UBodySetup* ParentBody = State->EditingAsset->BodySetups[Constraint.ParentBodyIndex];
		UBodySetup* ChildBody = State->EditingAsset->BodySetups[Constraint.ChildBodyIndex];
		if (!ParentBody || !ChildBody) continue;

		FVector ParentPos = FVector::Zero();
		FVector ChildPos = FVector::Zero();
		FQuat JointRotation = FQuat::Identity();

		if (MeshComp)
		{
			FTransform ParentTransform = MeshComp->GetBoneWorldTransform(ParentBody->BoneIndex);
			FTransform ChildTransform = MeshComp->GetBoneWorldTransform(ChildBody->BoneIndex);

			ParentPos = ParentTransform.Translation;
			ChildPos = ChildTransform.Translation;

			// 조인트 회전 계산: 부모→자식 방향을 X축으로
			FVector Direction = (ChildPos - ParentPos);
			if (Direction.SizeSquared() > KINDA_SMALL_NUMBER)
			{
				Direction = Direction.GetNormalized();
				// 기본 X축(1,0,0)을 Direction으로 회전하는 쿼터니언 계산
				FVector From(1.0f, 0.0f, 0.0f);
				FVector To = Direction;
				float Dot = FVector::Dot(From, To);

				if (Dot > 0.9999f)
				{
					JointRotation = FQuat::Identity();
				}
				else if (Dot < -0.9999f)
				{
					// 반대 방향: Y축 기준 180도 회전
					JointRotation = FQuat(0.0f, 1.0f, 0.0f, 0.0f);
				}
				else
				{
					FVector Cross = FVector::Cross(From, To);
					JointRotation = FQuat(Cross.X, Cross.Y, Cross.Z, 1.0f + Dot);
					JointRotation.Normalize();
				}
			}
		}

		bool bSelected = (!State->bBodySelectionMode && State->SelectedConstraintIndex == i);

		// 면 기반 시각화 사용
		GenerateConstraintMeshVisualization(
			Constraint, ParentPos, ChildPos, JointRotation, bSelected,
			State->ConstraintTrianglesBatch.Vertices,
			State->ConstraintTrianglesBatch.Indices,
			State->ConstraintTrianglesBatch.Colors,
			State->ConstraintLinesBatch);
	}

	// ULineComponent에 라인 배치 데이터 설정 (연결선 + 축 마커)
	if (ConstraintLineComp)
	{
		ConstraintLineComp->ClearLines();  // 기존 ULine 정리
		ConstraintLineComp->SetDirectBatch(State->ConstraintLinesBatch);
		ConstraintLineComp->SetTriangleBatch(State->ConstraintTrianglesBatch);  // 삼각형 배치도 설정
		ConstraintLineComp->SetLineVisible(State->bShowConstraints);
	}
}

void SPhysicsAssetEditorWindow::UpdateSelectedBodyLines()
{
	PhysicsAssetEditorState* State = GetActivePhysicsState();
	if (!State || !State->EditingAsset) return;
	if (State->SelectedBodyIndex < 0) return;
	if (State->SelectedBodyIndex >= static_cast<int32>(State->EditingAsset->BodySetups.size())) return;
	if (State->SelectedBodyIndex >= State->BodyLineRanges.Num()) return;

	UBodySetup* Body = State->EditingAsset->BodySetups[State->SelectedBodyIndex];
	if (!Body) return;
	const PhysicsAssetEditorState::FBodyLineRange& Range = State->BodyLineRanges[State->SelectedBodyIndex];

	// 본 트랜스폼 가져오기
	FTransform BoneTransform;
	USkeletalMeshComponent* MeshComp = State->GetPreviewMeshComponent();
	if (MeshComp && Body->BoneIndex >= 0)
	{
		BoneTransform = MeshComp->GetBoneWorldTransform(Body->BoneIndex);
	}

	// 헬퍼 함수로 좌표 생성 (AggGeom의 모든 Shape 처리)
	TArray<FVector> StartPoints, EndPoints;
	GenerateBodyShapeLineCoordinates(Body, BoneTransform, StartPoints, EndPoints);

	// FLinesBatch 직접 업데이트 (범위 내 라인 좌표만 변경)
	for (int32 i = 0; i < Range.Count && i < StartPoints.Num(); ++i)
	{
		State->BodyLinesBatch.SetLine(Range.StartIndex + i, StartPoints[i], EndPoints[i]);
	}

	// ULineComponent에 갱신된 배치 데이터 재설정
	if (ULineComponent* BodyLineComp = State->BodyPreviewLineComponent)
	{
		BodyLineComp->SetDirectBatch(State->BodyLinesBatch);
	}
}

void SPhysicsAssetEditorWindow::UpdateSelectionColors()
{
	PhysicsAssetEditorState* State = GetActivePhysicsState();
	if (!State || !State->EditingAsset) return;

	const FVector4 SelectedColor(0.0f, 1.0f, 0.0f, 1.0f);   // 녹색
	const FVector4 NormalColor(0.0f, 0.5f, 1.0f, 1.0f);     // 파랑
	const FVector4 SelectedConstraintColor(1.0f, 1.0f, 0.0f, 1.0f);  // 노랑
	const FVector4 NormalConstraintColor(1.0f, 0.5f, 0.0f, 1.0f);    // 주황

	bool bBodyBatchDirty = false;
	bool bConstraintBatchDirty = false;

	// 헬퍼 람다: 특정 바디의 라인 색상 업데이트 (FLinesBatch 기반)
	auto UpdateBodyColor = [&](int32 BodyIndex, const FVector4& Color)
	{
		if (BodyIndex >= 0 && BodyIndex < State->BodyLineRanges.Num())
		{
			const PhysicsAssetEditorState::FBodyLineRange& Range = State->BodyLineRanges[BodyIndex];
			State->BodyLinesBatch.SetColorRange(Range.StartIndex, Range.Count, Color);
			bBodyBatchDirty = true;
		}
	};

	// 헬퍼 람다: 특정 제약조건의 라인 색상 업데이트 (FLinesBatch 기반)
	auto UpdateConstraintColor = [&](int32 ConstraintIndex, const FVector4& Color)
	{
		if (ConstraintIndex >= 0 && ConstraintIndex < State->ConstraintLinesBatch.Num())
		{
			State->ConstraintLinesBatch.SetColor(ConstraintIndex, Color);
			bConstraintBatchDirty = true;
		}
	};

	// 바디 선택 변경 처리 (이전 선택 → 비선택, 새 선택 → 선택)
	if (State->CachedSelectedBodyIndex != State->SelectedBodyIndex)
	{
		// 이전에 선택된 바디를 일반 색상으로
		UpdateBodyColor(State->CachedSelectedBodyIndex, NormalColor);

		// 새로 선택된 바디를 선택 색상으로 (바디 선택 모드일 때만)
		if (State->bBodySelectionMode)
		{
			UpdateBodyColor(State->SelectedBodyIndex, SelectedColor);
		}
	}

	// 제약조건 선택 변경 처리
	if (State->CachedSelectedConstraintIndex != State->SelectedConstraintIndex)
	{
		// 이전에 선택된 제약조건을 일반 색상으로
		UpdateConstraintColor(State->CachedSelectedConstraintIndex, NormalConstraintColor);

		// 새로 선택된 제약조건을 선택 색상으로 (제약조건 선택 모드일 때만)
		if (!State->bBodySelectionMode)
		{
			UpdateConstraintColor(State->SelectedConstraintIndex, SelectedConstraintColor);
		}
	}

	// ULineComponent에 갱신된 배치 데이터 재설정
	if (bBodyBatchDirty && State->BodyPreviewLineComponent)
	{
		State->BodyPreviewLineComponent->SetDirectBatch(State->BodyLinesBatch);
	}
	if (bConstraintBatchDirty && State->ConstraintPreviewLineComponent)
	{
		State->ConstraintPreviewLineComponent->SetDirectBatch(State->ConstraintLinesBatch);
	}
}

// ────────────────────────────────────────────────────────────────
// 파일 작업
// ────────────────────────────────────────────────────────────────

void SPhysicsAssetEditorWindow::SavePhysicsAsset()
{
	PhysicsAssetEditorState* State = GetActivePhysicsState();
	if (!State || !State->EditingAsset) return;

	if (State->CurrentFilePath.empty())
	{
		SavePhysicsAssetAs();
		return;
	}

	if (PhysicsAssetEditorBootstrap::SavePhysicsAsset(State->EditingAsset, State->CurrentFilePath))
	{
		State->bIsDirty = false;
	}
}

void SPhysicsAssetEditorWindow::SavePhysicsAssetAs()
{
	PhysicsAssetEditorState* State = GetActivePhysicsState();
	if (!State || !State->EditingAsset) return;

	// 파일 저장 다이얼로그 열기 (.physicsasset 확장자)
	auto widePath = FPlatformProcess::OpenSaveFileDialog(
		UTF8ToWide(GDataDir),
		L"physicsasset",
		L"Physics Asset Files"
	);

	if (widePath.empty()) return;

	FString FilePath = WideToUTF8(widePath.wstring());

	if (PhysicsAssetEditorBootstrap::SavePhysicsAsset(State->EditingAsset, FilePath))
	{
		State->CurrentFilePath = FilePath;
		State->bIsDirty = false;
		UE_LOG("[SPhysicsAssetEditorWindow] Physics Asset saved: %s", FilePath.c_str());
	}
}

void SPhysicsAssetEditorWindow::LoadPhysicsAsset()
{
	CheckUnsavedChangesAndExecute([this]()
	{
		// 파일 열기 다이얼로그 (.physicsasset 확장자)
		auto widePath = FPlatformProcess::OpenLoadFileDialog(
			UTF8ToWide(GDataDir),
			L"physicsasset",
			L"Physics Asset Files"
		);

		if (widePath.empty()) return;

		FString FilePath = WideToUTF8(widePath.wstring());

		UPhysicsAsset* LoadedAsset = PhysicsAssetEditorBootstrap::LoadPhysicsAsset(FilePath);
		if (LoadedAsset)
		{
			PhysicsAssetEditorState* State = GetActivePhysicsState();
			if (State)
			{
				State->EditingAsset = LoadedAsset;
				State->CurrentFilePath = FilePath;
				State->ClearSelection();
				State->RequestLinesRebuild();
				State->HideGizmo();

				// 연결된 스켈레탈 메시 로드
				if (!LoadedAsset->SkeletalMeshPath.empty())
				{
					USkeletalMesh* Mesh = UResourceManager::GetInstance().Load<USkeletalMesh>(
						LoadedAsset->SkeletalMeshPath);
					if (Mesh && State->PreviewActor)
					{
						State->PreviewActor->SetSkeletalMesh(LoadedAsset->SkeletalMeshPath);
						State->CurrentMesh = Mesh;
						State->LoadedMeshPath = LoadedAsset->SkeletalMeshPath;
						State->RequestLinesRebuild();
					}
				}

				// 파일에서 로드한 것이므로 dirty 아님
				State->bIsDirty = false;
				UE_LOG("[SPhysicsAssetEditorWindow] Physics Asset loaded: %s", FilePath.c_str());
			}
		}
	});
}

void SPhysicsAssetEditorWindow::CheckUnsavedChangesAndExecute(std::function<void()> Action)
{
	PhysicsAssetEditorState* State = GetActivePhysicsState();
	if (!State || !State->bIsDirty)
	{
		// 변경사항 없으면 바로 실행
		if (Action) Action();
		return;
	}

	// 모달 다이얼로그 표시
	PendingAction = Action;
	FModalDialog::Get().Show(
		"저장 확인",
		"저장되지 않은 변경사항이 있습니다.\n저장하시겠습니까?",
		EModalType::YesNoCancel,
		[this](EModalResult Result)
		{
			if (Result == EModalResult::Yes)
			{
				SavePhysicsAsset();
				if (PendingAction) PendingAction();
			}
			else if (Result == EModalResult::No)
			{
				if (PendingAction) PendingAction();
			}
			// Cancel이면 아무것도 안 함
			PendingAction = nullptr;
		}
	);
}

void SPhysicsAssetEditorWindow::LoadMeshAndResetPhysics(PhysicsAssetEditorState* State, const FString& MeshPath)
{
	if (!State) return;

	// SkeletalMesh 로드
	USkeletalMesh* Mesh = UResourceManager::GetInstance().Load<USkeletalMesh>(MeshPath);
	if (Mesh && State->PreviewActor)
	{
		State->PreviewActor->SetSkeletalMesh(MeshPath);
		State->CurrentMesh = Mesh;
		State->LoadedMeshPath = MeshPath;

		// 새 Physics Asset 생성 (기존 캐시된 에셋 수정하지 않음)
		State->EditingAsset = NewObject<UPhysicsAsset>();
		State->EditingAsset->SkeletalMeshPath = MeshPath;
		State->CurrentFilePath.clear();  // 새 에셋이므로 파일 경로 초기화
		State->bIsDirty = false;  // 새 에셋이므로 수정되지 않은 상태

		// 상태 초기화
		State->ClearSelection();
		State->RequestLinesRebuild();
		State->HideGizmo();

		OnSkeletalMeshLoaded(State, MeshPath);

		UE_LOG("[SPhysicsAssetEditorWindow] Mesh loaded and physics reset: %s", MeshPath.c_str());
	}
}

FString SPhysicsAssetEditorWindow::ExtractFileName(const FString& Path)
{
	size_t lastSlash = Path.find_last_of("/\\");
	FString FileName = (lastSlash != FString::npos) ? Path.substr(lastSlash + 1) : Path;

	// 확장자 제거
	size_t dotPos = FileName.find_last_of('.');
	if (dotPos != FString::npos)
	{
		FileName = FileName.substr(0, dotPos);
	}

	return FileName;
}

// ────────────────────────────────────────────────────────────────
// 바디/제약 조건 작업
// ────────────────────────────────────────────────────────────────

void SPhysicsAssetEditorWindow::AutoGenerateBodies()
{
	PhysicsAssetEditorState* State = GetActivePhysicsState();
	if (!State || !State->EditingAsset || !State->CurrentMesh) return;

	const FSkeleton* Skeleton = State->CurrentMesh->GetSkeleton();
	if (!Skeleton || Skeleton->Bones.IsEmpty()) return;

	UPhysicsAsset* Asset = State->EditingAsset;

	// 기존 상태 완전 초기화
	Asset->ClearAll();
	State->BodyLinesBatch.Clear();
	State->BodyLineRanges.Empty();
	State->ConstraintLinesBatch.Clear();
	State->CachedSelectedBodyIndex = -1;
	State->CachedSelectedConstraintIndex = -1;

	// 본 길이 계산을 위한 자식 본 맵 구축
	TArray<TArray<int32>> ChildrenMap;
	ChildrenMap.SetNum(static_cast<int32>(Skeleton->Bones.size()));
	for (int32 i = 0; i < static_cast<int32>(Skeleton->Bones.size()); ++i)
	{
		int32 ParentIdx = Skeleton->Bones[i].ParentIndex;
		if (ParentIdx >= 0 && ParentIdx < static_cast<int32>(Skeleton->Bones.size()))
		{
			ChildrenMap[ParentIdx].Add(i);
		}
	}

	// 모든 본에 대해 바디 생성
	for (int32 i = 0; i < static_cast<int32>(Skeleton->Bones.size()); ++i)
	{
		const FBone& Bone = Skeleton->Bones[i];

		// 본 위치 (BindPose 행렬의 Translation 부분: M[3][0..2])
		FVector BonePos(Bone.BindPose.M[3][0], Bone.BindPose.M[3][1], Bone.BindPose.M[3][2]);

		// 본 길이 계산 (자식 본까지의 거리 또는 기본값)
		float BoneLength = 0.05f; // 기본값 (리프 본용)
		if (!ChildrenMap[i].IsEmpty())
		{
			// 첫 번째 자식까지의 거리 계산
			int32 ChildIdx = ChildrenMap[i][0];
			const FBone& ChildBone = Skeleton->Bones[ChildIdx];
			FVector ChildPos(ChildBone.BindPose.M[3][0], ChildBone.BindPose.M[3][1], ChildBone.BindPose.M[3][2]);
			BoneLength = (ChildPos - BonePos).Size();
		}

		// 최소/최대 크기 제한 (더 보수적으로)
		BoneLength = std::max(0.02f, std::min(BoneLength, 0.5f));

		// 바디 생성
		int32 BodyIndex = Asset->AddBody(Bone.Name, i);
		if (BodyIndex >= 0)
		{
			UBodySetup* Body = Asset->BodySetups[BodyIndex];
			if (Body)
			{
				// AggGeom에 캡슐 Shape 추가
				FKSphylElem NewCapsule;
				NewCapsule.Radius = BoneLength * 0.1f;       // 본 길이의 10%를 반지름으로
				NewCapsule.Length = BoneLength * 0.7f;       // 본 길이의 70%를 전체 길이로
				Body->AggGeom.SphylElems.Add(NewCapsule);
			}
		}
	}

	// 부모-자식 본 사이에 제약 조건 생성
	for (int32 i = 0; i < static_cast<int32>(Skeleton->Bones.size()); ++i)
	{
		int32 ParentBoneIdx = Skeleton->Bones[i].ParentIndex;
		if (ParentBoneIdx < 0) continue;

		int32 ChildBodyIdx = Asset->FindBodyIndexByBone(i);
		int32 ParentBodyIdx = Asset->FindBodyIndexByBone(ParentBoneIdx);

		if (ChildBodyIdx >= 0 && ParentBodyIdx >= 0)
		{
			char JointName[128];
			sprintf_s(JointName, "Joint_%s", Skeleton->Bones[i].Name.c_str());
			Asset->AddConstraint(FName(JointName), ParentBodyIdx, ChildBodyIdx);
		}
	}

	State->bIsDirty = true;
	State->RequestLinesRebuild();
	State->ClearSelection();
	State->HideGizmo();

	UE_LOG("[SPhysicsAssetEditorWindow] Auto-generated %d bodies and %d constraints",
		(int)Asset->BodySetups.size(), (int)Asset->ConstraintSetups.size());
}

void SPhysicsAssetEditorWindow::AddBodyToBone(int32 BoneIndex)
{
	PhysicsAssetEditorState* State = GetActivePhysicsState();
	if (!State || !State->EditingAsset || !State->CurrentMesh) return;
	if (BoneIndex < 0) return;

	const FSkeleton* Skeleton = State->CurrentMesh->GetSkeleton();
	if (!Skeleton || BoneIndex >= static_cast<int32>(Skeleton->Bones.size())) return;

	// 이미 바디가 있는지 확인
	if (State->EditingAsset->FindBodyIndexByBone(BoneIndex) >= 0)
	{
		UE_LOG("[SPhysicsAssetEditorWindow] Bone already has a body");
		return;
	}

	FName BoneName = Skeleton->Bones[BoneIndex].Name;
	int32 NewBodyIndex = State->EditingAsset->AddBody(BoneName, BoneIndex);

	if (NewBodyIndex >= 0)
	{
		// 부모 본의 바디 찾기
		int32 ParentBoneIndex = Skeleton->Bones[BoneIndex].ParentIndex;
		if (ParentBoneIndex >= 0)
		{
			int32 ParentBodyIndex = State->EditingAsset->FindBodyIndexByBone(ParentBoneIndex);
			if (ParentBodyIndex >= 0)
			{
				// 자동으로 제약 조건 생성
				char JointNameBuf[128];
				sprintf_s(JointNameBuf, "Joint_%s", BoneName.ToString().c_str());
				State->EditingAsset->AddConstraint(FName(JointNameBuf), ParentBodyIndex, NewBodyIndex);
			}
		}

		State->SelectBody(NewBodyIndex);
		State->bIsDirty = true;
		State->RequestLinesRebuild();
	}
}

void SPhysicsAssetEditorWindow::RemoveSelectedBody()
{
	PhysicsAssetEditorState* State = GetActivePhysicsState();
	if (!State || !State->EditingAsset) return;
	if (State->SelectedBodyIndex < 0) return;

	if (State->EditingAsset->RemoveBody(State->SelectedBodyIndex))
	{
		State->ClearSelection();
		State->bIsDirty = true;
		State->RequestLinesRebuild();
		State->HideGizmo();
	}
}

void SPhysicsAssetEditorWindow::AddConstraintBetweenBodies(int32 ParentBodyIndex, int32 ChildBodyIndex)
{
	PhysicsAssetEditorState* State = GetActivePhysicsState();
	if (!State || !State->EditingAsset) return;

	char JointNameBuf[64];
	sprintf_s(JointNameBuf, "Constraint_%d_%d", ParentBodyIndex, ChildBodyIndex);
	int32 NewIndex = State->EditingAsset->AddConstraint(FName(JointNameBuf), ParentBodyIndex, ChildBodyIndex);

	if (NewIndex >= 0)
	{
		State->SelectConstraint(NewIndex);
		State->bIsDirty = true;
		State->RequestLinesRebuild();
	}
}

void SPhysicsAssetEditorWindow::RemoveSelectedConstraint()
{
	PhysicsAssetEditorState* State = GetActivePhysicsState();
	if (!State || !State->EditingAsset) return;
	if (State->SelectedConstraintIndex < 0) return;

	if (State->EditingAsset->RemoveConstraint(State->SelectedConstraintIndex))
	{
		State->ClearSelection();
		State->bIsDirty = true;
		State->RequestLinesRebuild();
		State->HideGizmo();
	}
}

void SPhysicsAssetEditorWindow::RegenerateSelectedBody()
{
	PhysicsAssetEditorState* State = GetActivePhysicsState();
	if (!State || State->SelectedBodyIndex < 0) return;
	if (!State->EditingAsset || !State->CurrentMesh) return;

	UBodySetup* Body = State->EditingAsset->BodySetups[State->SelectedBodyIndex];
	if (!Body) return;

	int32 BoneIndex = Body->BoneIndex;

	// 기존 Shape 모두 삭제
	Body->AggGeom.SphereElems.Empty();
	Body->AggGeom.BoxElems.Empty();
	Body->AggGeom.SphylElems.Empty();

	// 본 길이 기반으로 새 캡슐 생성
	const FSkeleton* Skeleton = State->CurrentMesh->GetSkeleton();
	if (!Skeleton || BoneIndex < 0 || BoneIndex >= static_cast<int32>(Skeleton->Bones.size())) return;

	const FBone& Bone = Skeleton->Bones[BoneIndex];
	FVector BonePos(Bone.BindPose.M[3][0], Bone.BindPose.M[3][1], Bone.BindPose.M[3][2]);

	// 본 길이 계산 (자식 본까지의 거리)
	float BoneLength = 0.05f;  // 기본값
	for (size_t i = 0; i < Skeleton->Bones.size(); ++i)
	{
		if (Skeleton->Bones[i].ParentIndex == BoneIndex)
		{
			const FBone& ChildBone = Skeleton->Bones[i];
			FVector ChildPos(ChildBone.BindPose.M[3][0], ChildBone.BindPose.M[3][1], ChildBone.BindPose.M[3][2]);
			BoneLength = (ChildPos - BonePos).Size();
			break;
		}
	}

	// 최소/최대 크기 제한
	BoneLength = std::max(0.02f, std::min(BoneLength, 0.5f));

	// 캡슐 생성
	FKSphylElem NewCapsule;
	NewCapsule.Radius = BoneLength * 0.1f;
	NewCapsule.Length = BoneLength * 0.7f;
	Body->AggGeom.SphylElems.Add(NewCapsule);

	State->bIsDirty = true;
	State->RequestLinesRebuild();
}

void SPhysicsAssetEditorWindow::AddPrimitiveToBody(int32 BodyIndex, int32 PrimitiveType)
{
	PhysicsAssetEditorState* State = GetActivePhysicsState();
	if (!State || !State->EditingAsset) return;
	if (BodyIndex < 0 || BodyIndex >= static_cast<int32>(State->EditingAsset->BodySetups.size())) return;

	UBodySetup* Body = State->EditingAsset->BodySetups[BodyIndex];
	if (!Body) return;

	switch (PrimitiveType)
	{
	case 0:  // Box
	{
		FKBoxElem NewBox;
		NewBox.X = 10.0f;
		NewBox.Y = 10.0f;
		NewBox.Z = 10.0f;
		NewBox.Center = FVector(0, 0, 0);
		NewBox.Rotation = FQuat::Identity();
		Body->AggGeom.BoxElems.Add(NewBox);
		break;
	}
	case 1:  // Sphere
	{
		FKSphereElem NewSphere;
		NewSphere.Radius = 5.0f;
		NewSphere.Center = FVector(0, 0, 0);
		Body->AggGeom.SphereElems.Add(NewSphere);
		break;
	}
	case 2:  // Capsule
	{
		FKSphylElem NewCapsule;
		NewCapsule.Radius = 3.0f;
		NewCapsule.Length = 10.0f;
		NewCapsule.Center = FVector(0, 0, 0);
		NewCapsule.Rotation = FQuat::Identity();
		Body->AggGeom.SphylElems.Add(NewCapsule);
		break;
	}
	}

	State->bIsDirty = true;
	State->RequestLinesRebuild();
}

// ────────────────────────────────────────────────────────────────
// 마우스 입력 (베이스 클래스의 bone picking 제거)
// ────────────────────────────────────────────────────────────────

void SPhysicsAssetEditorWindow::OnMouseDown(FVector2D MousePos, uint32 Button)
{
	if (!ActiveState || !ActiveState->Viewport) return;

	// 현재 뷰포트가 호버되어 있어야 마우스 다운 처리 (Z-order 고려)
	if (!ActiveState->Viewport->IsViewportHovered()) return;

	if (CenterRect.Contains(MousePos))
	{
		FVector2D LocalPos = MousePos - FVector2D(CenterRect.Left, CenterRect.Top);

		// ─────────────────────────────────────────────
		// 타이밍 측정 시작
		// ─────────────────────────────────────────────
		auto T0 = std::chrono::high_resolution_clock::now();

		// 기즈모 피킹 수행
		ActiveState->Viewport->ProcessMouseButtonDown((int32)LocalPos.X, (int32)LocalPos.Y, (int32)Button);

		auto T1 = std::chrono::high_resolution_clock::now();

		// 좌클릭: 바디 피킹
		if (Button == 0)
		{
			PhysicsAssetEditorState* State = GetActivePhysicsState();
			if (State && State->EditingAsset && State->PreviewActor && State->Client && State->World)
			{
				// 기즈모가 선택되었는지 확인
				UActorComponent* SelectedComp = State->World->GetSelectionManager()->GetSelectedComponent();
				bool bGizmoSelected = SelectedComp && Cast<UBoneAnchorComponent>(SelectedComp);

				// 기즈모가 선택되지 않은 경우에만 바디 피킹
				if (!bGizmoSelected)
				{
					ACameraActor* Camera = State->Client->GetCamera();
					if (Camera)
					{
						// 카메라 벡터
						FVector CameraPos = Camera->GetActorLocation();
						FVector CameraRight = Camera->GetRight();
						FVector CameraUp = Camera->GetUp();
						FVector CameraForward = Camera->GetForward();

						// 뷰포트 마우스 위치
						FVector2D ViewportMousePos(MousePos.X - CenterRect.Left, MousePos.Y - CenterRect.Top);
						FVector2D ViewportSize(CenterRect.GetWidth(), CenterRect.GetHeight());

						// 레이 생성
						FRay Ray = MakeRayFromViewport(
							Camera->GetViewMatrix(),
							Camera->GetProjectionMatrix(CenterRect.GetWidth() / CenterRect.GetHeight(), State->Viewport),
							CameraPos,
							CameraRight,
							CameraUp,
							CameraForward,
							ViewportMousePos,
							ViewportSize
						);

						auto T2 = std::chrono::high_resolution_clock::now();

						// 모든 바디에 대해 레이캐스트
						int32 ClosestBodyIndex = -1;
						float ClosestT = FLT_MAX;

						USkeletalMeshComponent* SkelComp = State->GetPreviewMeshComponent();

						if (SkelComp)
						{
							for (int32 i = 0; i < static_cast<int32>(State->EditingAsset->BodySetups.size()); ++i)
							{
								UBodySetup* Body = State->EditingAsset->BodySetups[i];
								if (!Body || Body->BoneIndex < 0) continue;

								// 본 월드 트랜스폼
								FTransform BoneWorldTransform = SkelComp->GetBoneWorldTransform(Body->BoneIndex);

								float HitT;
								if (IntersectRayBody(Ray, Body, BoneWorldTransform, HitT))
								{
									if (HitT < ClosestT)
									{
										ClosestT = HitT;
										ClosestBodyIndex = i;
									}
								}
							}
						}

						auto T3 = std::chrono::high_resolution_clock::now();

						if (ClosestBodyIndex >= 0)
						{
							// 바디 선택 (색상만 업데이트 - 전체 rebuild 불필요)
							State->bBodySelectionMode = true;
							State->SelectedBodyIndex = ClosestBodyIndex;
							State->SelectedConstraintIndex = -1;
							State->bSelectionColorDirty = true;

							// 기즈모 위치 업데이트
							RepositionAnchorToBody(ClosestBodyIndex);
						}
						else
						{
							// 빈 공간 클릭: 선택 해제 및 기즈모 숨기기
							State->ClearSelection();
							State->HideGizmo();
						}

						auto T4 = std::chrono::high_resolution_clock::now();

						// ─────────────────────────────────────────────
						// 타이밍 출력
						// ─────────────────────────────────────────────
						double Ms1 = std::chrono::duration<double, std::milli>(T1 - T0).count();
						double Ms2 = std::chrono::duration<double, std::milli>(T2 - T1).count();
						double Ms3 = std::chrono::duration<double, std::milli>(T3 - T2).count();
						double Ms4 = std::chrono::duration<double, std::milli>(T4 - T3).count();
						double MsTotal = std::chrono::duration<double, std::milli>(T4 - T0).count();

						char buf[256];
						sprintf_s(buf, "[PhysicsAssetEditor Pick] ProcessMouseButtonDown: %.2f ms | RaySetup: %.2f ms | BodyPicking: %.2f ms | Selection: %.2f ms | Total: %.2f ms",
							Ms1, Ms2, Ms3, Ms4, MsTotal);
						UE_LOG(buf);
					}
				}
			}

			bLeftMousePressed = true;
		}

		// 우클릭: 카메라 조작 시작
		if (Button == 1)
		{
			UInputManager::GetInstance().SetCursorVisible(false);
			UInputManager::GetInstance().LockCursor();
			bRightMousePressed = true;
		}
	}
}

void SPhysicsAssetEditorWindow::OnMouseUp(FVector2D MousePos, uint32 Button)
{
	if (!ActiveState || !ActiveState->Viewport) return;

	FVector2D LocalPos = MousePos - FVector2D(CenterRect.Left, CenterRect.Top);
	ActiveState->Viewport->ProcessMouseButtonUp((int32)LocalPos.X, (int32)LocalPos.Y, (int32)Button);

	// 좌클릭 해제
	if (Button == 0)
	{
		bLeftMousePressed = false;
	}

	// 우클릭 해제: 카메라 조작 종료
	if (Button == 1)
	{
		UInputManager::GetInstance().SetCursorVisible(true);
		UInputManager::GetInstance().ReleaseCursor();
		bRightMousePressed = false;
	}
}

// ────────────────────────────────────────────────────────────────
// Sub Widget 관리
// ────────────────────────────────────────────────────────────────

void SPhysicsAssetEditorWindow::CreateSubWidgets()
{
	SkeletonTreeWidget = NewObject<USkeletonTreeWidget>();
	SkeletonTreeWidget->Initialize();

	// SkeletonTreeWidget 콜백 연결
	SkeletonTreeWidget->OnAddBody.Add([this](int32 BoneIndex) {
		AddBodyToBone(BoneIndex);
	});

	SkeletonTreeWidget->OnRemoveBody.Add([this](int32 BodyIndex) {
		PhysicsAssetEditorState* State = GetActivePhysicsState();
		if (State)
		{
			State->SelectBody(BodyIndex);
			RemoveSelectedBody();
		}
	});

	SkeletonTreeWidget->OnAddPrimitive.Add([this](int32 BodyIndex, int32 PrimitiveType) {
		AddPrimitiveToBody(BodyIndex, PrimitiveType);
	});

	SkeletonTreeWidget->OnAddConstraint.Add([this](int32 BodyIndex1, int32 BodyIndex2) {
		AddConstraintBetweenBodies(BodyIndex1, BodyIndex2);
	});

	SkeletonTreeWidget->OnRemoveConstraint.Add([this](int32 ConstraintIndex) {
		PhysicsAssetEditorState* State = GetActivePhysicsState();
		if (State)
		{
			State->SelectConstraint(ConstraintIndex);
			RemoveSelectedConstraint();
		}
	});

	BodyPropertiesWidget = NewObject<UBodyPropertiesWidget>();
	BodyPropertiesWidget->Initialize();

	ConstraintPropertiesWidget = NewObject<UConstraintPropertiesWidget>();
	ConstraintPropertiesWidget->Initialize();

	ToolsWidget = NewObject<UToolsWidget>();
	ToolsWidget->Initialize();
	ToolsWidget->SetEditorWindow(this);
}

void SPhysicsAssetEditorWindow::DestroySubWidgets()
{
	if (SkeletonTreeWidget)
	{
		// 콜백 정리
		SkeletonTreeWidget->OnAddBody.Clear();
		SkeletonTreeWidget->OnRemoveBody.Clear();
		SkeletonTreeWidget->OnAddPrimitive.Clear();
		SkeletonTreeWidget->OnAddConstraint.Clear();
		SkeletonTreeWidget->OnRemoveConstraint.Clear();

		DeleteObject(SkeletonTreeWidget);
		SkeletonTreeWidget = nullptr;
	}
	if (BodyPropertiesWidget)
	{
		DeleteObject(BodyPropertiesWidget);
		BodyPropertiesWidget = nullptr;
	}
	if (ConstraintPropertiesWidget)
	{
		DeleteObject(ConstraintPropertiesWidget);
		ConstraintPropertiesWidget = nullptr;
	}
	if (ToolsWidget)
	{
		DeleteObject(ToolsWidget);
		ToolsWidget = nullptr;
	}
}

void SPhysicsAssetEditorWindow::UpdateSubWidgetEditorState()
{
	PhysicsAssetEditorState* State = GetActivePhysicsState();
	if (SkeletonTreeWidget)
	{
		SkeletonTreeWidget->SetEditorState(State);
	}
	if (BodyPropertiesWidget)
	{
		BodyPropertiesWidget->SetEditorState(State);
	}
	if (ConstraintPropertiesWidget)
	{
		ConstraintPropertiesWidget->SetEditorState(State);
	}
	if (ToolsWidget)
	{
		ToolsWidget->SetEditorState(State);
	}
}

// ────────────────────────────────────────────────────────────────
// 기즈모 연동
// ────────────────────────────────────────────────────────────────

void SPhysicsAssetEditorWindow::RepositionAnchorToBody(int32 BodyIndex)
{
	PhysicsAssetEditorState* State = GetActivePhysicsState();
	if (!State || !State->EditingAsset || !State->PreviewActor || !State->World)
		return;

	if (BodyIndex < 0 || BodyIndex >= static_cast<int32>(State->EditingAsset->BodySetups.size()))
		return;

	ASkeletalMeshActor* SkelActor = State->GetPreviewSkeletalActor();
	if (!SkelActor) return;

	// BoneAnchor 컴포넌트가 없으면 생성
	SkelActor->EnsureViewerComponents();

	USkeletalMeshComponent* MeshComp = State->GetPreviewMeshComponent();
	USceneComponent* Anchor = SkelActor->GetBoneGizmoAnchor();

	if (!MeshComp || !Anchor)
		return;

	UBodySetup* Body = State->EditingAsset->BodySetups[BodyIndex];
	if (!Body) return;

	// 바디의 월드 트랜스폼 계산: 본 월드 트랜스폼 기준
	FTransform BoneWorldTransform;
	if (Body->BoneIndex >= 0)
	{
		BoneWorldTransform = MeshComp->GetBoneWorldTransform(Body->BoneIndex);
	}
	else
	{
		BoneWorldTransform = MeshComp->GetWorldTransform();
	}

	// AggGeom의 첫 번째 Shape 중심을 기즈모 위치로 사용
	FVector ShapeCenter = FVector(0.0f, 0.0f, 0.0f);
	FQuat ShapeRotation = FQuat::Identity();
	if (!Body->AggGeom.SphereElems.IsEmpty())
	{
		ShapeCenter = Body->AggGeom.SphereElems[0].Center;
	}
	else if (!Body->AggGeom.BoxElems.IsEmpty())
	{
		ShapeCenter = Body->AggGeom.BoxElems[0].Center;
		ShapeRotation = Body->AggGeom.BoxElems[0].Rotation;
	}
	else if (!Body->AggGeom.SphylElems.IsEmpty())
	{
		ShapeCenter = Body->AggGeom.SphylElems[0].Center;
		ShapeRotation = Body->AggGeom.SphylElems[0].Rotation;
	}

	// Shape의 로컬 트랜스폼을 본의 월드 트랜스폼으로 변환 (스케일 무시 - Shape Center는 스케일 독립적 오프셋)
	FVector WorldCenter = BoneWorldTransform.Translation + BoneWorldTransform.Rotation.RotateVector(ShapeCenter);
	FQuat WorldRotation = BoneWorldTransform.Rotation * ShapeRotation;

	// 앵커를 바디의 월드 위치로 이동
	Anchor->SetWorldLocation(WorldCenter);
	Anchor->SetWorldRotation(WorldRotation);

	// 앵커 편집 가능 상태로 설정
	if (UBoneAnchorComponent* BoneAnchor = Cast<UBoneAnchorComponent>(Anchor))
	{
		BoneAnchor->SetSuppressWriteback(true);  // 본 수정 방지 (바디만 수정)
		BoneAnchor->SetEditability(true);
		BoneAnchor->SetVisibility(true);
	}

	// SelectionManager를 통해 앵커 선택 (기즈모가 앵커에 연결됨)
	USelectionManager* SelectionManager = State->World->GetSelectionManager();
	if (SelectionManager)
	{
		SelectionManager->SelectActor(State->PreviewActor);
		SelectionManager->SelectComponent(Anchor);
	}
}

void SPhysicsAssetEditorWindow::UpdateBodyTransformFromGizmo()
{
	PhysicsAssetEditorState* State = GetActivePhysicsState();
	if (!State || !State->EditingAsset || !State->PreviewActor || !State->World)
		return;

	if (State->SelectedBodyIndex < 0 ||
		State->SelectedBodyIndex >= static_cast<int32>(State->EditingAsset->BodySetups.size()))
		return;

	ASkeletalMeshActor* SkelActor = State->GetPreviewSkeletalActor();
	if (!SkelActor) return;

	// BoneAnchor 컴포넌트가 없으면 생성
	SkelActor->EnsureViewerComponents();

	USkeletalMeshComponent* MeshComp = State->GetPreviewMeshComponent();
	USceneComponent* Anchor = SkelActor->GetBoneGizmoAnchor();
	AGizmoActor* Gizmo = State->World->GetGizmoActor();

	if (!MeshComp || !Anchor || !Gizmo)
		return;

	UBodySetup* Body = State->EditingAsset->BodySetups[State->SelectedBodyIndex];
	if (!Body) return;

	// 기즈모 모드 확인
	EGizmoMode CurrentGizmoMode = Gizmo->GetGizmoMode();

	// 앵커의 현재 월드 트랜스폼 (기즈모로 이동됨)
	const FTransform& AnchorWorldTransform = Anchor->GetWorldTransform();

	// 본의 월드 트랜스폼 가져오기 (바디의 부모)
	FTransform BoneWorldTransform;
	if (Body->BoneIndex >= 0)
	{
		BoneWorldTransform = MeshComp->GetBoneWorldTransform(Body->BoneIndex);
	}
	else
	{
		BoneWorldTransform = MeshComp->GetWorldTransform();
	}

	// 원하는 로컬 트랜스폼 계산: 본 월드 기준 앵커의 상대 트랜스폼 (스케일 무시 - Shape Center는 스케일 독립적 오프셋)
	FQuat InvBoneRotation = BoneWorldTransform.Rotation.Inverse();
	FVector DesiredLocalCenter = InvBoneRotation.RotateVector(AnchorWorldTransform.Translation - BoneWorldTransform.Translation);
	FQuat DesiredLocalRotation = InvBoneRotation * AnchorWorldTransform.Rotation;

	// 벡터 비교 헬퍼 람다
	auto VectorEquals = [](const FVector& A, const FVector& B, float Tolerance) -> bool
	{
		return std::abs(A.X - B.X) <= Tolerance &&
		       std::abs(A.Y - B.Y) <= Tolerance &&
		       std::abs(A.Z - B.Z) <= Tolerance;
	};

	bool bChanged = false;

	// AggGeom의 첫 번째 Shape의 Center/Rotation 업데이트
	// (기즈모는 첫 번째 Shape를 기준으로 위치함)
	switch (CurrentGizmoMode)
	{
	case EGizmoMode::Translate:
		{
			FVector NewCenter = DesiredLocalCenter;
			if (!Body->AggGeom.SphereElems.IsEmpty())
			{
				if (!VectorEquals(NewCenter, Body->AggGeom.SphereElems[0].Center, 0.001f))
				{
					Body->AggGeom.SphereElems[0].Center = NewCenter;
					bChanged = true;
				}
			}
			else if (!Body->AggGeom.BoxElems.IsEmpty())
			{
				if (!VectorEquals(NewCenter, Body->AggGeom.BoxElems[0].Center, 0.001f))
				{
					Body->AggGeom.BoxElems[0].Center = NewCenter;
					bChanged = true;
				}
			}
			else if (!Body->AggGeom.SphylElems.IsEmpty())
			{
				if (!VectorEquals(NewCenter, Body->AggGeom.SphylElems[0].Center, 0.001f))
				{
					Body->AggGeom.SphylElems[0].Center = NewCenter;
					bChanged = true;
				}
			}
		}
		break;

	case EGizmoMode::Rotate:
		{
			FQuat NewRotation = DesiredLocalRotation;
			// Box와 Capsule만 회전을 가짐 (Sphere는 회전 무의미)
			if (!Body->AggGeom.BoxElems.IsEmpty())
			{
				float Dot = std::abs(FQuat::Dot(Body->AggGeom.BoxElems[0].Rotation, NewRotation));
				if (Dot < 0.9999f)
				{
					Body->AggGeom.BoxElems[0].Rotation = NewRotation;
					bChanged = true;
				}
			}
			else if (!Body->AggGeom.SphylElems.IsEmpty())
			{
				float Dot = std::abs(FQuat::Dot(Body->AggGeom.SphylElems[0].Rotation, NewRotation));
				if (Dot < 0.9999f)
				{
					Body->AggGeom.SphylElems[0].Rotation = NewRotation;
					bChanged = true;
				}
			}
		}
		break;

	case EGizmoMode::Scale:
		// Scale 모드: Shape 크기 조절 (향후 구현 가능)
		break;

	default:
		// Select 모드 등에서는 아무것도 하지 않음
		return;
	}

	if (bChanged)
	{
		State->bIsDirty = true;
		State->RequestSelectedBodyLinesUpdate();  // 라인 좌표 업데이트 (색상/개수 유지)
	}
}

