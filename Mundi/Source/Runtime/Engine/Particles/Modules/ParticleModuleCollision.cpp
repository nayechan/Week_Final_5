#include "pch.h"
#include "ParticleModuleCollision.h"
#include "ParticleEmitterInstance.h"
#include "ParticleSystemComponent.h"
#include "World.h"
#include "WorldPartitionManager.h"
#include "BVHierarchy.h"
#include "ShapeComponent.h"
#include "Collision.h"
#include "BoundingSphere.h"
#include "AABB.h"
#include "OBB.h"

void UParticleModuleCollision::Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle* ParticleBase)
{
	if (!ParticleBase || !Owner || !Owner->Component)
	{
		return;
	}

	// PARTICLE_ELEMENT 매크로 사용을 위한 CurrentOffset 초기화
	uint32 CurrentOffset = Offset;

	// 페이로드 접근 (매크로 사용)
	PARTICLE_ELEMENT(FParticleCollisionPayload, CollPayload);

	// 페이로드 초기화
	CollPayload.UsedCollisionCount = 0;
	CollPayload.DelayTimer = 0.0f;
	CollPayload.UsedDampingFactor = DampingFactor.GetValue(Owner->EmitterTime, Owner->RandomStream, Owner->Component);
}

void UParticleModuleCollision::Update(FModuleUpdateContext& Context)
{
	// World와 BVH 가져오기
	UParticleSystemComponent* PSC = Context.Owner.Component;
	if (!PSC)
	{
		return;
	}

	UWorld* World = PSC->GetWorld();
	if (!World)
	{
		return;
	}

	UWorldPartitionManager* PartitionMgr = World->GetPartitionManager();
	if (!PartitionMgr)
	{
		return;
	}

	FBVHierarchy* BVH = PartitionMgr->GetBVH();
	if (!BVH)
	{
		return;
	}

	float DeltaTime = Context.DeltaTime;
	int32 Offset = Context.Offset;

	BEGIN_UPDATE_LOOP
		PARTICLE_ELEMENT(FParticleCollisionPayload, CollPayload);

		// 딜레이 체크
		CollPayload.DelayTimer += DeltaTime;
		if (CollPayload.DelayTimer < DelayAmount)
		{
			continue;
		}

		// MaxCollisions 체크
		if (CollPayload.UsedCollisionCount >= MaxCollisions)
		{
			HandleCollisionComplete(Particle, CollPayload);
			continue;
		}

		// 이동 경로 계산 (터널링 방지)
		FVector Start = Particle.OldLocation;
		FVector End = Particle.Location;

		// 1. 광역 검사 (BVH AABB 쿼리) - 경로 전체를 포함하는 바운드
		FAABB PathBounds;
		// Component-wise min/max
		PathBounds.Min = FVector(
			FMath::Min(Start.X, End.X) - ParticleRadius,
			FMath::Min(Start.Y, End.Y) - ParticleRadius,
			FMath::Min(Start.Z, End.Z) - ParticleRadius
		);
		PathBounds.Max = FVector(
			FMath::Max(Start.X, End.X) + ParticleRadius,
			FMath::Max(Start.Y, End.Y) + ParticleRadius,
			FMath::Max(Start.Z, End.Z) + ParticleRadius
		);

		TArray<UPrimitiveComponent*> Candidates = BVH->QueryIntersectedComponents(PathBounds);

		// 2. 정밀 검사 (ShapeComponent만)
		for (UPrimitiveComponent* Comp : Candidates)
		{
			UShapeComponent* ShapeComp = Cast<UShapeComponent>(Comp);
			if (!ShapeComp)
			{
				continue;
			}

			// 충돌 검사: 파티클 현재 위치를 구체로 취급
			FBoundingSphere ParticleSphere;
			ParticleSphere.Center = Particle.Location;
			ParticleSphere.Radius = ParticleRadius;

			// ShapeComponent의 형상 가져오기
			FShape Shape;
			ShapeComp->GetShape(Shape);
			FTransform ShapeTransform = ShapeComp->GetWorldTransform();

			// 충돌 여부 검사
			bool bCollided = false;
			FVector HitNormal = FVector(0.0f, 0.0f, 0.0f);

			switch (Shape.Kind)
			{
			case EShapeKind::Box:
				{
					FOBB BoxOBB;
					Collision::BuildOBB(Shape, ShapeTransform, BoxOBB);
					bCollided = Collision::Overlap_Sphere_OBB(ParticleSphere.Center, ParticleSphere.Radius, BoxOBB);
					if (bCollided)
					{
						// 박스의 가장 가까운 표면 법선 계산 (단순화)
						// 역변환을 통해 로컬 공간으로 변환
						FMatrix InvMatrix = ShapeTransform.ToMatrix().Inverse();
						FVector LocalPos = InvMatrix.TransformPosition(Particle.Location);
						FVector Extent = Shape.Box.BoxExtent;

						// 각 축에서 가장 가까운 면 찾기
						float MinDist = FLT_MAX;
						for (int32 Axis = 0; Axis < 3; Axis++)
						{
							float Dist = FMath::Abs(FMath::Abs(LocalPos[Axis]) - Extent[Axis]);
							if (Dist < MinDist)
							{
								MinDist = Dist;
								HitNormal = FVector(0.0f, 0.0f, 0.0f);
								HitNormal[Axis] = (LocalPos[Axis] > 0) ? 1.0f : -1.0f;
							}
						}
						// 월드 공간으로 변환
						HitNormal = ShapeTransform.ToMatrix().TransformVector(HitNormal);
						HitNormal.Normalize();
					}
				}
				break;

			case EShapeKind::Sphere:
				{
					FVector SphereCenter = ShapeTransform.Translation;
					float SphereRadius = Shape.Sphere.SphereRadius * Collision::UniformScaleMax(ShapeTransform.Scale3D);
					float Dist = FVector::Distance(ParticleSphere.Center, SphereCenter);
					bCollided = (Dist < (ParticleSphere.Radius + SphereRadius));
					if (bCollided)
					{
						HitNormal = (ParticleSphere.Center - SphereCenter);
						HitNormal.Normalize();
					}
				}
				break;

			case EShapeKind::Capsule:
				{
					// 캡슐 충돌 검사 (단순화 - 구체로 취급)
					FVector P0, P1;
					float CapsuleRadius;
					Collision::BuildCapsule(Shape, ShapeTransform, P0, P1, CapsuleRadius);

					// 파티클 위치에서 캡슐 중심선까지의 최단 거리
					FVector CapsuleDir = P1 - P0;
					float CapsuleLen = CapsuleDir.Size();
					if (CapsuleLen > KINDA_SMALL_NUMBER)
					{
						CapsuleDir /= CapsuleLen;
					}

					FVector ToParticle = ParticleSphere.Center - P0;
					float Projection = FMath::Clamp(FVector::Dot(ToParticle, CapsuleDir), 0.0f, CapsuleLen);
					FVector ClosestPoint = P0 + CapsuleDir * Projection;

					float Dist = FVector::Distance(ParticleSphere.Center, ClosestPoint);
					bCollided = (Dist < (ParticleSphere.Radius + CapsuleRadius));
					if (bCollided)
					{
						HitNormal = (ParticleSphere.Center - ClosestPoint);
						HitNormal.Normalize();
					}
				}
				break;
			}

			if (bCollided)
			{
				// 충돌 이벤트 생성
				if (bGenerateCollisionEvents && PSC)
				{
					FParticleEventCollideData Event;
					Event.Type = EParticleEventType::Collision;
					Event.Position = Particle.Location;
					Event.Velocity = Particle.Velocity;
					Event.Normal = HitNormal;
					Event.HitComponent = ShapeComp;
					Event.HitActor = ShapeComp->GetOwner();
					Event.EmitterTime = Context.Owner.EmitterTime;

					PSC->AddCollisionEvent(Event);
				}

				// 충돌 위치 보정 (표면에서 약간 띄움)
				Particle.Location += HitNormal * ParticleRadius * 0.1f;

				// 바운스 처리
				ApplyDamping(Particle, CollPayload, HitNormal);
				CollPayload.UsedCollisionCount++;

				// 충돌 발생 플래그 설정
				Particle.Flags |= STATE_Particle_CollisionHasOccurred;

				break;  // 한 프레임에 하나의 충돌만 처리
			}
		}
	END_UPDATE_LOOP
}

void UParticleModuleCollision::HandleCollisionComplete(FBaseParticle& Particle, FParticleCollisionPayload& Payload)
{
	switch (CollisionCompletionOption)
	{
	case EParticleCollisionComplete::HaltCollisions:
		// 더 이상 충돌 검사 안함 (플래그 설정)
		Particle.Flags |= STATE_Particle_IgnoreCollisions;
		break;

	case EParticleCollisionComplete::Freeze:
		// 파티클 프리즈
		Particle.Flags |= STATE_Particle_Freeze;
		break;

	case EParticleCollisionComplete::Kill:
		// 파티클 제거 (RelativeTime을 1.0 이상으로 설정하면 다음 프레임에 제거됨)
		Particle.RelativeTime = 1.1f;
		break;
	}
}

void UParticleModuleCollision::ApplyDamping(FBaseParticle& Particle, const FParticleCollisionPayload& Payload, const FVector& Normal)
{
	// 속도를 법선 방향 성분과 접선 방향 성분으로 분해
	float VelDotNormal = FVector::Dot(Particle.Velocity, Normal);
	FVector NormalComponent = Normal * VelDotNormal;      // 법선 방향 속도 (반사됨)
	FVector TangentComponent = Particle.Velocity - NormalComponent;  // 접선 방향 속도 (마찰)

	// 감쇠 적용
	// DampingFactor.X, Y: 접선 감쇠 (마찰)
	// DampingFactor.Z: 법선 감쇠 (탄성)
	FVector DampedTangent = TangentComponent * FVector(Payload.UsedDampingFactor.X, Payload.UsedDampingFactor.Y, 1.0f);
	FVector DampedNormal = -NormalComponent * Payload.UsedDampingFactor.Z;  // 반사 (부호 반전)

	Particle.Velocity = DampedTangent + DampedNormal;
	Particle.BaseVelocity = Particle.Velocity;  // BaseVelocity도 업데이트
}

void UParticleModuleCollision::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	UParticleModule::Serialize(bInIsLoading, InOutHandle);

	JSON TempJson;
	if (bInIsLoading)
	{
		if (FJsonSerializer::ReadObject(InOutHandle, "DampingFactor", TempJson))
			DampingFactor.Serialize(true, TempJson);
		FJsonSerializer::ReadInt32(InOutHandle, "MaxCollisions", MaxCollisions);

		// CollisionCompletionOption enum 직렬화
		int32 OptionValue = 0;
		FJsonSerializer::ReadInt32(InOutHandle, "CollisionCompletionOption", OptionValue);
		CollisionCompletionOption = static_cast<EParticleCollisionComplete>(OptionValue);

		FJsonSerializer::ReadFloat(InOutHandle, "DelayAmount", DelayAmount);
		FJsonSerializer::ReadFloat(InOutHandle, "ParticleRadius", ParticleRadius);
		FJsonSerializer::ReadBool(InOutHandle, "bGenerateCollisionEvents", bGenerateCollisionEvents);
	}
	else
	{
		TempJson = JSON::Make(JSON::Class::Object);
		DampingFactor.Serialize(false, TempJson);
		InOutHandle["DampingFactor"] = TempJson;
		InOutHandle["MaxCollisions"] = MaxCollisions;
		InOutHandle["CollisionCompletionOption"] = static_cast<int32>(CollisionCompletionOption);
		InOutHandle["DelayAmount"] = DelayAmount;
		InOutHandle["ParticleRadius"] = ParticleRadius;
		InOutHandle["bGenerateCollisionEvents"] = bGenerateCollisionEvents;
	}
}
