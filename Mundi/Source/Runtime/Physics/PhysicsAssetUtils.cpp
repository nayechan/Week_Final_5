#include "pch.h"
#include "PhysicsAssetUtils.h"
#include "PhysicsAsset.h"
#include "SkeletalBodySetup.h"
#include "PhysicsConstraintTemplate.h"
#include "SkeletalMesh.h"
#include "Source/Runtime/Engine/Components/SkeletalMeshComponent.h"

int32 FPhysicsAssetUtils::CreateAllBodies(
	UPhysicsAsset* PhysAsset,
	USkeletalMesh* Mesh,
	USkeletalMeshComponent* MeshComp,
	const FPhysicsAssetCreateOptions& Options)
{
	if (!PhysAsset || !Mesh) return 0;

	const FSkeletalMeshData* MeshData = Mesh->GetSkeletalMeshData();
	if (!MeshData) return 0;

	const FSkeleton* Skeleton = &MeshData->Skeleton;
	if (!Skeleton || Skeleton->Bones.IsEmpty()) return 0;

	// 기존 바디/컨스트레인트 모두 삭제
	RemoveAllBodiesAndConstraints(PhysAsset);

	const TArray<FSkinnedVertex>& Vertices = MeshData->Vertices;
	const TArray<FBone>& Bones = Skeleton->Bones;
	const float MinBoneSize = Options.MinBoneSize * 100.0f;  // m -> cm 변환
	const float MinPrimSize = 0.5f;  // 최소 프리미티브 크기 (0.5cm)
	const int32 ShapeType = Options.ShapeType;

	// === 각 본에 대해 가중치된 버텍스 수집 ===
	TArray<TArray<FVector>> BoneVertices;
	BoneVertices.SetNum(Bones.Num());

	for (const FSkinnedVertex& Vertex : Vertices)
	{
		// DominantWeight 방식: 가장 높은 가중치 본에만 귀속
		int32 DominantBone = -1;
		float DominantWeight = 0.0f;

		for (int32 i = 0; i < 4; ++i)
		{
			if (Vertex.BoneWeights[i] > DominantWeight)
			{
				DominantWeight = Vertex.BoneWeights[i];
				DominantBone = Vertex.BoneIndices[i];
			}
		}

		if (DominantBone >= 0 && DominantBone < (int32)Bones.Num())
		{
			// 버텍스를 본의 로컬 좌표계로 변환
			const FBone& Bone = Bones[DominantBone];
			FVector LocalPos = Bone.InverseBindPose.TransformPosition(Vertex.Position);
			BoneVertices[DominantBone].Add(LocalPos);
		}
	}

	// === 본 크기 계산 및 작은 본 병합 (Bottom-Up) ===
	TArray<float> BoneSizes;
	BoneSizes.SetNum(Bones.Num());
	TArray<bool> ShouldCreateBody;
	ShouldCreateBody.SetNum(Bones.Num());

	// 1. 각 본의 바운딩 박스 크기 계산
	for (int32 BoneIdx = 0; BoneIdx < (int32)Bones.Num(); ++BoneIdx)
	{
		TArray<FVector>& Verts = BoneVertices[BoneIdx];
		if (Verts.Num() >= 3)
		{
			FVector MinB(FLT_MAX, FLT_MAX, FLT_MAX);
			FVector MaxB(-FLT_MAX, -FLT_MAX, -FLT_MAX);
			for (const FVector& V : Verts)
			{
				MinB.X = FMath::Min(MinB.X, V.X);
				MinB.Y = FMath::Min(MinB.Y, V.Y);
				MinB.Z = FMath::Min(MinB.Z, V.Z);
				MaxB.X = FMath::Max(MaxB.X, V.X);
				MaxB.Y = FMath::Max(MaxB.Y, V.Y);
				MaxB.Z = FMath::Max(MaxB.Z, V.Z);
			}
			FVector Extent = MaxB - MinB;
			BoneSizes[BoneIdx] = Extent.Size();
		}
		else
		{
			BoneSizes[BoneIdx] = 0.0f;
		}
		ShouldCreateBody[BoneIdx] = false;
	}

	// 2. Bottom-Up 병합: 리프 → 루트 순으로 (역순 순회)
	for (int32 BoneIdx = (int32)Bones.Num() - 1; BoneIdx >= 0; --BoneIdx)
	{
		const FBone& Bone = Bones[BoneIdx];
		float MySize = BoneSizes[BoneIdx];

		// 모든 본에 바디 생성 옵션이 켜져있으면 병합 안함
		if (Options.bBodyForAll)
		{
			if (BoneVertices[BoneIdx].Num() >= 3 || BoneIdx == 0)
			{
				ShouldCreateBody[BoneIdx] = true;
			}
			continue;
		}

		// 크기가 MinBoneSize 미만이면 부모에 병합
		if (MySize < MinBoneSize && Bone.ParentIndex >= 0)
		{
			int32 ParentIdx = Bone.ParentIndex;

			// 자식 버텍스들을 부모 본 좌표계로 변환하여 병합
			const FBone& ParentBone = Bones[ParentIdx];
			for (const FVector& LocalPos : BoneVertices[BoneIdx])
			{
				// 자식 본 로컬 → 월드 → 부모 본 로컬
				FVector WorldPos = Bone.BindPose.TransformPosition(LocalPos);
				FVector ParentLocalPos = ParentBone.InverseBindPose.TransformPosition(WorldPos);
				BoneVertices[ParentIdx].Add(ParentLocalPos);
			}

			// 병합된 크기를 부모에 누적
			BoneSizes[ParentIdx] += MySize;

			// 이 본에는 바디 생성 안함
			ShouldCreateBody[BoneIdx] = false;
		}
		else
		{
			// 충분히 크면 바디 생성
			if (BoneVertices[BoneIdx].Num() >= 3)
			{
				ShouldCreateBody[BoneIdx] = true;
			}
		}
	}

	// 루트 본은 항상 바디 생성
	if (Bones.Num() > 0 && BoneVertices[0].Num() >= 3)
	{
		ShouldCreateBody[0] = true;
	}

	int32 CreatedBodyCount = 0;

	// === 각 본에 바디 생성 ===
	for (int32 BoneIdx = 0; BoneIdx < (int32)Bones.Num(); ++BoneIdx)
	{
		const FBone& Bone = Bones[BoneIdx];
		TArray<FVector>& Verts = BoneVertices[BoneIdx];

		// 바디 생성 대상이 아니면 스킵
		if (!ShouldCreateBody[BoneIdx])
		{
			continue;
		}

		// 본 크기 계산 (버텍스 바운딩 박스 또는 부모-자식 거리)
		FVector BoxCenter = FVector::Zero();
		FVector BoxExtent = FVector::Zero();
		FQuat ElemRotation = FQuat::Identity();

		if (Verts.Num() >= 3)
		{
			// === PCA 기반 바운딩 계산 ===

			// 1. 평균 위치 계산
			FVector Mean = FVector::Zero();
			for (const FVector& V : Verts)
			{
				Mean = Mean + V;
			}
			Mean = Mean / (float)Verts.Num();

			// 2. 공분산 행렬 계산
			float Cov[3][3] = { {0,0,0}, {0,0,0}, {0,0,0} };
			for (const FVector& V : Verts)
			{
				FVector D = V - Mean;
				Cov[0][0] += D.X * D.X;
				Cov[0][1] += D.X * D.Y;
				Cov[0][2] += D.X * D.Z;
				Cov[1][0] += D.Y * D.X;
				Cov[1][1] += D.Y * D.Y;
				Cov[1][2] += D.Y * D.Z;
				Cov[2][0] += D.Z * D.X;
				Cov[2][1] += D.Z * D.Y;
				Cov[2][2] += D.Z * D.Z;
			}
			float N = (float)Verts.Num();
			for (int i = 0; i < 3; ++i)
				for (int j = 0; j < 3; ++j)
					Cov[i][j] /= N;

			// 3. 거듭제곱법으로 지배적 고유벡터 찾기 (주축 Z)
			FVector Bk(0, 0, 1);
			for (int iter = 0; iter < 32; ++iter)
			{
				FVector ABk;
				ABk.X = Cov[0][0] * Bk.X + Cov[0][1] * Bk.Y + Cov[0][2] * Bk.Z;
				ABk.Y = Cov[1][0] * Bk.X + Cov[1][1] * Bk.Y + Cov[1][2] * Bk.Z;
				ABk.Z = Cov[2][0] * Bk.X + Cov[2][1] * Bk.Y + Cov[2][2] * Bk.Z;
				float Len = ABk.Size();
				if (Len > 0.0001f)
					Bk = ABk / Len;
			}
			FVector ZAxis = Bk.GetNormalized();

			// 4. 직교 축 계산
			FVector YAxis, XAxis;
			if (FMath::Abs(ZAxis.Z) < 0.999f)
			{
				YAxis = FVector::Cross(FVector(0, 0, 1), ZAxis).GetNormalized();
			}
			else
			{
				YAxis = FVector::Cross(FVector(1, 0, 0), ZAxis).GetNormalized();
			}
			XAxis = FVector::Cross(YAxis, ZAxis).GetNormalized();

			// 5. 정렬된 바운딩 박스 계산
			FVector MinB(FLT_MAX, FLT_MAX, FLT_MAX);
			FVector MaxB(-FLT_MAX, -FLT_MAX, -FLT_MAX);
			for (const FVector& V : Verts)
			{
				FVector D = V - Mean;
				float ProjX = FVector::Dot(D, XAxis);
				float ProjY = FVector::Dot(D, YAxis);
				float ProjZ = FVector::Dot(D, ZAxis);

				MinB.X = FMath::Min(MinB.X, ProjX);
				MinB.Y = FMath::Min(MinB.Y, ProjY);
				MinB.Z = FMath::Min(MinB.Z, ProjZ);
				MaxB.X = FMath::Max(MaxB.X, ProjX);
				MaxB.Y = FMath::Max(MaxB.Y, ProjY);
				MaxB.Z = FMath::Max(MaxB.Z, ProjZ);
			}

			// 6. 중심과 범위 계산
			FVector LocalCenter = (MinB + MaxB) * 0.5f;
			BoxExtent = (MaxB - MinB) * 0.5f;

			// 월드 중심으로 변환
			BoxCenter = Mean + XAxis * LocalCenter.X + YAxis * LocalCenter.Y + ZAxis * LocalCenter.Z;

			// 회전 행렬 생성
			FMatrix RotMat;
			RotMat.M[0][0] = XAxis.X; RotMat.M[0][1] = XAxis.Y; RotMat.M[0][2] = XAxis.Z; RotMat.M[0][3] = 0;
			RotMat.M[1][0] = YAxis.X; RotMat.M[1][1] = YAxis.Y; RotMat.M[1][2] = YAxis.Z; RotMat.M[1][3] = 0;
			RotMat.M[2][0] = ZAxis.X; RotMat.M[2][1] = ZAxis.Y; RotMat.M[2][2] = ZAxis.Z; RotMat.M[2][3] = 0;
			RotMat.M[3][0] = 0;       RotMat.M[3][1] = 0;       RotMat.M[3][2] = 0;       RotMat.M[3][3] = 1;
			ElemRotation = FQuat(RotMat);
		}
		else
		{
			// 버텍스가 부족하면 본 길이 기반 간소화 방식
			if (Bone.ParentIndex >= 0 && MeshComp)
			{
				FTransform BoneTM = MeshComp->GetBoneWorldTransform(BoneIdx);
				FTransform ParentTM = MeshComp->GetBoneWorldTransform(Bone.ParentIndex);
				FVector BoneDir = BoneTM.Translation - ParentTM.Translation;
				float BoneLen = BoneDir.Size();

				if (BoneLen < MinBoneSize && !Options.bBodyForAll)
				{
					continue;
				}

				// 간소화 바운딩
				float Radius = FMath::Max(BoneLen * 0.2f, MinPrimSize);
				BoxCenter = FVector::Zero();  // 본 로컬 원점
				BoxExtent = FVector(Radius, Radius, BoneLen * 0.5f);

				// 본 방향으로 회전
				FVector ZDir = BoneDir.GetNormalized();
				if (ZDir.Size() > 0.001f)
				{
					ElemRotation = FQuat::FindQuatBetween(FVector(0, 0, 1), ZDir);
				}
			}
			else
			{
				// 루트 본: 최소 크기 구
				BoxCenter = FVector::Zero();
				BoxExtent = FVector(MinPrimSize, MinPrimSize, MinPrimSize);
			}
		}

		// 최소 크기 보장
		BoxExtent.X = FMath::Max(BoxExtent.X, MinPrimSize);
		BoxExtent.Y = FMath::Max(BoxExtent.Y, MinPrimSize);
		BoxExtent.Z = FMath::Max(BoxExtent.Z, MinPrimSize);

		// === BodySetup 생성 ===
		USkeletalBodySetup* NewBody = new USkeletalBodySetup();
		NewBody->BoneName = Bone.Name;
		NewBody->PhysicsType = EPhysicsType::Default;
		NewBody->CollisionResponse = EBodyCollisionResponse::BodyCollision_Enabled;

		// 버텍스 좌표가 이미 cm 단위이므로 변환 불필요
		FVector CenterCm = BoxCenter;
		FVector ExtentCm = BoxExtent;

		// 바디 크기 비율 적용
		ExtentCm = ExtentCm * Options.BodySizeScale;

		// Shape 타입에 따라 프리미티브 생성
		switch (ShapeType)
		{
		case 0:  // Sphere
		{
			FKSphereElem Elem;
			Elem.Center = CenterCm;
			float MaxExtent = FMath::Max(ExtentCm.X, FMath::Max(ExtentCm.Y, ExtentCm.Z));
			Elem.Radius = MaxExtent;
			NewBody->AggGeom.SphereElems.Add(Elem);
			break;
		}
		case 1:  // Box
		{
			FKBoxElem Elem;
			Elem.Center = CenterCm;
			Elem.Rotation = ElemRotation;
			Elem.X = ExtentCm.X * 2.0f;
			Elem.Y = ExtentCm.Y * 2.0f;
			Elem.Z = ExtentCm.Z * 2.0f;
			NewBody->AggGeom.BoxElems.Add(Elem);
			break;
		}
		case 2:  // Capsule (Sphyl)
		default:
		{
			FKSphylElem Elem;
			Elem.Center = CenterCm;

			// 가장 긴 축을 캡슐 축으로
			if (ExtentCm.X > ExtentCm.Z && ExtentCm.X > ExtentCm.Y)
			{
				// X가 가장 큼
				Elem.Rotation = ElemRotation * FQuat::FromAxisAngle(FVector(0, 1, 0), -PI * 0.5f);
				Elem.Radius = FMath::Max(ExtentCm.Y, ExtentCm.Z);
				Elem.Length = FMath::Max(0.0f, (ExtentCm.X - Elem.Radius) * 2.0f);
			}
			else if (ExtentCm.Y > ExtentCm.Z && ExtentCm.Y > ExtentCm.X)
			{
				// Y가 가장 큼
				Elem.Rotation = ElemRotation * FQuat::FromAxisAngle(FVector(1, 0, 0), PI * 0.5f);
				Elem.Radius = FMath::Max(ExtentCm.X, ExtentCm.Z);
				Elem.Length = FMath::Max(0.0f, (ExtentCm.Y - Elem.Radius) * 2.0f);
			}
			else
			{
				// Z가 가장 큼
				Elem.Rotation = ElemRotation;
				Elem.Radius = FMath::Max(ExtentCm.X, ExtentCm.Y);
				Elem.Length = FMath::Max(0.0f, (ExtentCm.Z - Elem.Radius) * 2.0f);
			}
			NewBody->AggGeom.SphylElems.Add(Elem);
			break;
		}
		}

		PhysAsset->AddBodySetup(NewBody);
		++CreatedBodyCount;
	}

	return CreatedBodyCount;
}

int32 FPhysicsAssetUtils::CreateConstraintsForBodies(
	UPhysicsAsset* PhysAsset,
	USkeletalMesh* Mesh,
	const FPhysicsAssetCreateOptions& Options)
{
	if (!PhysAsset || !Mesh) return 0;

	const FSkeleton* Skeleton = Mesh->GetSkeleton();
	if (!Skeleton) return 0;

	const TArray<FBone>& Bones = Skeleton->Bones;

	// 각 컨스트레인트 모드 설정
	EAngularConstraintMotion AngularMode;
	switch (Options.AngularMode)
	{
	case 0: AngularMode = EAngularConstraintMotion::Free; break;
	case 2: AngularMode = EAngularConstraintMotion::Locked; break;
	case 1:
	default: AngularMode = EAngularConstraintMotion::Limited; break;
	}

	int32 CreatedConstraintCount = 0;

	// 각 바디에 대해 부모 바디와 컨스트레인트 생성
	int32 BodyCount = PhysAsset->GetBodySetupCount();
	for (int32 BodyIdx = 0; BodyIdx < BodyCount; ++BodyIdx)
	{
		USkeletalBodySetup* Body = PhysAsset->GetBodySetup(BodyIdx);
		if (!Body) continue;

		// 현재 바디의 본 인덱스 찾기
		auto it = Skeleton->BoneNameToIndex.find(Body->BoneName.ToString());
		if (it == Skeleton->BoneNameToIndex.end()) continue;

		int32 BoneIndex = it->second;
		if (BoneIndex < 0 || BoneIndex >= (int32)Bones.Num()) continue;

		// 부모 바디 찾기 (계층 구조 상향 탐색)
		int32 ParentBoneIndex = Bones[BoneIndex].ParentIndex;
		int32 ParentBodyIndex = -1;

		while (ParentBoneIndex >= 0 && ParentBoneIndex < (int32)Bones.Num())
		{
			FName ParentBoneName = Bones[ParentBoneIndex].Name;
			ParentBodyIndex = PhysAsset->FindBodySetupIndex(ParentBoneName);

			if (ParentBodyIndex != -1)
			{
				break;  // 부모 바디 찾음
			}

			// 더 위로 탐색
			ParentBoneIndex = Bones[ParentBoneIndex].ParentIndex;
		}

		// 부모 바디가 없으면 스킵 (루트 바디)
		if (ParentBodyIndex == -1) continue;

		USkeletalBodySetup* ParentBody = PhysAsset->GetBodySetup(ParentBodyIndex);
		if (!ParentBody) continue;

		// 이미 같은 본 쌍의 컨스트레인트가 있는지 확인
		bool bAlreadyExists = false;
		for (int32 i = 0; i < PhysAsset->GetConstraintCount(); ++i)
		{
			UPhysicsConstraintTemplate* Existing = PhysAsset->ConstraintSetup[i];
			if (Existing)
			{
				FName Bone1 = Existing->GetBone1Name();
				FName Bone2 = Existing->GetBone2Name();
				if ((Bone1 == Body->BoneName && Bone2 == ParentBody->BoneName) ||
					(Bone1 == ParentBody->BoneName && Bone2 == Body->BoneName))
				{
					bAlreadyExists = true;
					break;
				}
			}
		}

		if (bAlreadyExists) continue;

		// 컨스트레인트 생성
		UPhysicsConstraintTemplate* NewConstraint = new UPhysicsConstraintTemplate();
		FConstraintInstance& Instance = NewConstraint->DefaultInstance;

		// 본 설정 (자식 = Bone1, 부모 = Bone2 - 언리얼 컨벤션)
		Instance.ConstraintBone1 = ParentBody->BoneName;  // 부모
		Instance.ConstraintBone2 = Body->BoneName;        // 자식 (조인트 위치)

		// 각 제한 모드 설정
		Instance.AngularSwing1Motion = AngularMode;
		Instance.AngularSwing2Motion = AngularMode;
		Instance.AngularTwistMotion = AngularMode;

		// Limited 모드일 때 각도 제한 설정
		if (AngularMode == EAngularConstraintMotion::Limited)
		{
			Instance.Swing1LimitAngle = Options.SwingLimit;
			Instance.Swing2LimitAngle = Options.SwingLimit;
			Instance.TwistLimitAngle = Options.TwistLimit;
		}

		PhysAsset->AddConstraint(NewConstraint);
		++CreatedConstraintCount;
	}

	return CreatedConstraintCount;
}

void FPhysicsAssetUtils::RemoveAllBodiesAndConstraints(UPhysicsAsset* PhysAsset)
{
	if (!PhysAsset) return;

	RemoveAllBodies(PhysAsset);
	RemoveAllConstraints(PhysAsset);
}

void FPhysicsAssetUtils::RemoveAllBodies(UPhysicsAsset* PhysAsset)
{
	if (!PhysAsset) return;

	while (PhysAsset->GetBodySetupCount() > 0)
	{
		PhysAsset->RemoveBodySetup(0);
	}
}

void FPhysicsAssetUtils::RemoveAllConstraints(UPhysicsAsset* PhysAsset)
{
	if (!PhysAsset) return;

	while (PhysAsset->GetConstraintCount() > 0)
	{
		PhysAsset->RemoveConstraint(0);
	}
}

physx::PxQuat FPhysicsAssetUtils::ComputeJointFrameRotation(const physx::PxVec3& Direction)
{
	PxVec3 DefaultAxis(1, 0, 0);  // PhysX D6 Joint의 기본 Twist 축
	PxVec3 Dir = Direction.getNormalized();

	float Dot = DefaultAxis.dot(Dir);

	// 이미 정렬됨 (같은 방향)
	if (Dot > 0.9999f)
	{
		return PxQuat(PxIdentity);
	}

	// 반대 방향
	if (Dot < -0.9999f)
	{
		return PxQuat(PxPi, PxVec3(0, 1, 0));
	}

	// 두 벡터 사이의 회전 축과 각도 계산
	PxVec3 Axis = DefaultAxis.cross(Dir).getNormalized();
	float Angle = std::acos(Dot);
	return PxQuat(Angle, Axis);
}

void FPhysicsAssetUtils::CalculateConstraintFramesFromPhysX(FConstraintInstance& Instance,
const physx::PxTransform& ParentGlobalPose, 
const physx::PxTransform& ChildGlobalPose)
{
	// 1. Joint 위치: 자식 Body의 위치 (일반적인 Ragdoll 관례)
	PxVec3 JointWorldPos = ChildGlobalPose.p;

	// 2. 본 방향 계산: 부모 → 자식
	PxVec3 BoneDirection = (ChildGlobalPose.p - ParentGlobalPose.p);
    
	// 예외 처리: 위치가 겹쳐있거나 너무 가까우면 부모의 X축 등을 사용
	if (BoneDirection.magnitude() < 1e-4f) // KINDA_SMALL_NUMBER
	{
		// 뼈 길이가 0이면 그냥 부모의 회전을 따라가거나 기본값 사용
		BoneDirection = ParentGlobalPose.q.getBasisVector0(); 
	}
	else
	{
		BoneDirection.normalize();
	}

	// 3. Joint Frame 회전: X축을 뼈 방향(Twist Axis)으로 정렬
	// (ComputeJointFrameRotation 함수는 PxVec3를 받아 PxQuat를 리턴한다고 가정)
	PxQuat JointRotation = ComputeJointFrameRotation(BoneDirection); 

	// 4. World -> Local 변환 (핵심 로직 - 이건 아주 정확함)
	// Frame1: 부모 기준에서 본 Joint 위치/회전
	PxTransform LocalFrame1 = ParentGlobalPose.getInverse() * PxTransform(JointWorldPos, JointRotation);
    
	// Frame2: 자식 기준에서 본 Joint 위치/회전
	// (보통 위치는 (0,0,0)에 가깝고, 회전만 남게 됨)
	PxTransform LocalFrame2 = ChildGlobalPose.getInverse() * PxTransform(JointWorldPos, JointRotation);

	// 5. 결과 저장 (PhysX -> Unreal 변환)
	Instance.Frame1Loc = PhysXConvert::FromPx(LocalFrame1.p);
	Instance.Frame1Rot = PhysXConvert::FromPx(LocalFrame1.q).ToEulerZYXDeg();

	Instance.Frame2Loc = PhysXConvert::FromPx(LocalFrame2.p);
	Instance.Frame2Rot = PhysXConvert::FromPx(LocalFrame2.q).ToEulerZYXDeg();
}
