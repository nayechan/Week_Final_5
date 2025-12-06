#include "pch.h"
#include "StaticMeshComponent.h"
#include "StaticMesh.h"
#include "Shader.h"
#include "ResourceManager.h"
#include "ObjManager.h"
#include "JsonSerializer.h"
#include "CameraComponent.h"
#include "MeshBatchElement.h"
#include "Material.h"
#include "SceneView.h"
#include "LuaBindHelpers.h"
#include "BodySetup.h"
#include "PhysicsDebugUtils.h"
#include "World.h"
#include "Actor.h"
#include "Renderer.h"
// IMPLEMENT_CLASS is now auto-generated in .generated.cpp
UStaticMeshComponent::UStaticMeshComponent()
{
	SetStaticMesh(GDataDir + "/cube-tex.obj");     // 임시 기본 static mesh 설정
}

UStaticMeshComponent::~UStaticMeshComponent() = default;

void UStaticMeshComponent::OnStaticMeshReleased(UStaticMesh* ReleasedMesh)
{
	// TODO : 왜 this가 없는지 추적 필요!
	if (!this || !StaticMesh || StaticMesh != ReleasedMesh)
	{
		return;
	}

	StaticMesh = nullptr;
}

void UStaticMeshComponent::CollectMeshBatches(TArray<FMeshBatchElement>& OutMeshBatchElements, const FSceneView* View)
{
	if (!StaticMesh || !StaticMesh->GetStaticMeshAsset())
	{
		return;
	}

	const TArray<FGroupInfo>& MeshGroupInfos = StaticMesh->GetMeshGroupInfo();

	auto DetermineMaterialAndShader = [&](uint32 SectionIndex) -> TPair<UMaterialInterface*, UShader*>
		{
			UMaterialInterface* Material = GetMaterial(SectionIndex);
			UShader* Shader = nullptr;

			if (Material && Material->GetShader())
			{
				Shader = Material->GetShader();
			}
			else
			{
				UE_LOG("UStaticMeshComponent: 머티리얼이 없거나 셰이더가 없어서 기본 머티리얼 사용 section %u.", SectionIndex);
				Material = UResourceManager::GetInstance().GetDefaultMaterial();
				if (Material)
				{
					Shader = Material->GetShader();
				}
				if (!Material || !Shader)
				{
					UE_LOG("UStaticMeshComponent: 기본 머티리얼이 없습니다.");
					return { nullptr, nullptr };
				}
			}
			return { Material, Shader };
		};

	const bool bHasSections = !MeshGroupInfos.IsEmpty();
	const uint32 NumSectionsToProcess = bHasSections ? static_cast<uint32>(MeshGroupInfos.size()) : 1;

	for (uint32 SectionIndex = 0; SectionIndex < NumSectionsToProcess; ++SectionIndex)
	{
		uint32 IndexCount = 0;
		uint32 StartIndex = 0;

		if (bHasSections)
		{
			const FGroupInfo& Group = MeshGroupInfos[SectionIndex];
			IndexCount = Group.IndexCount;
			StartIndex = Group.StartIndex;
		}
		else
		{
			IndexCount = StaticMesh->GetIndexCount();
			StartIndex = 0;
		}

		if (IndexCount == 0)
		{
			continue;
		}

		auto [MaterialToUse, ShaderToUse] = DetermineMaterialAndShader(SectionIndex);
		if (!MaterialToUse || !ShaderToUse)
		{
			continue;
		}

		FMeshBatchElement BatchElement;
		// View 모드 전용 매크로와 머티리얼 개인 매크로를 결합한다
		TArray<FShaderMacro> ShaderMacros = View->ViewShaderMacros;
		if (0 < MaterialToUse->GetShaderMacros().Num())
		{
			ShaderMacros.Append(MaterialToUse->GetShaderMacros());
		}
		FShaderVariant* ShaderVariant = ShaderToUse->GetOrCompileShaderVariant(ShaderMacros);

		if (ShaderVariant)
		{
			BatchElement.VertexShader = ShaderVariant->VertexShader;
			BatchElement.PixelShader = ShaderVariant->PixelShader;
			BatchElement.InputLayout = ShaderVariant->InputLayout;
		}

		// UMaterialInterface를 UMaterial로 캐스팅해야 할 수 있음. 렌더러가 UMaterial을 기대한다면.
		// 지금은 Material.h 구조상 UMaterialInterface에 필요한 정보가 다 있음.
		BatchElement.Material = MaterialToUse;
		BatchElement.VertexBuffer = StaticMesh->GetVertexBuffer();
		BatchElement.IndexBuffer = StaticMesh->GetIndexBuffer();
		BatchElement.VertexStride = StaticMesh->GetVertexStride();
		BatchElement.IndexCount = IndexCount;
		BatchElement.StartIndex = StartIndex;
		BatchElement.BaseVertexIndex = 0;
		BatchElement.WorldMatrix = GetWorldMatrix();
		BatchElement.ObjectID = InternalIndex;
		BatchElement.PrimitiveTopology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

		// FBX에서 가져온 Transparency 값이 있으면 반투명 렌더링 모드로 설정
		if (MaterialToUse->GetMaterialInfo().Transparency > 0.0f)
		{
			BatchElement.RenderMode = EBatchRenderMode::Translucent;
		}

		OutMeshBatchElements.Add(BatchElement);
	}
}

void UStaticMeshComponent::SetStaticMesh(const FString& PathFileName)
{
	// 새 메시를 설정하기 전에, 기존에 생성된 모든 MID와 슬롯 정보를 정리합니다.
	ClearDynamicMaterials();

	// 새 메시를 로드합니다.
	StaticMesh = UResourceManager::GetInstance().Load<UStaticMesh>(PathFileName);
	if (StaticMesh && StaticMesh->GetStaticMeshAsset())
	{
		const TArray<FGroupInfo>& GroupInfos = StaticMesh->GetMeshGroupInfo();

		// 4. 새 메시 정보에 맞게 슬롯을 재설정합니다.
		MaterialSlots.resize(GroupInfos.size()); // ClearDynamicMaterials()에서 비워졌으므로, 새 크기로 재할당

		for (int i = 0; i < GroupInfos.size(); ++i)
		{
			SetMaterialByName(i, GroupInfos[i].InitialMaterialName);
		}
		MarkWorldPartitionDirty();
	}
	else
	{
		// 메시 로드에 실패한 경우, StaticMesh 포인터를 nullptr로 보장합니다.
		// (슬롯은 이미 위에서 비워졌습니다.)
		StaticMesh = nullptr;
	}
}

FAABB UStaticMeshComponent::GetWorldAABB() const
{
	const FTransform WorldTransform = GetWorldTransform();
	const FMatrix WorldMatrix = GetWorldMatrix();

	if (!StaticMesh)
	{
		const FVector Origin = WorldTransform.TransformPosition(FVector());
		return FAABB(Origin, Origin);
	}

	const FAABB LocalBound = StaticMesh->GetLocalBound();
	const FVector LocalMin = LocalBound.Min;
	const FVector LocalMax = LocalBound.Max;

	const FVector LocalCorners[8] = {
		FVector(LocalMin.X, LocalMin.Y, LocalMin.Z),
		FVector(LocalMax.X, LocalMin.Y, LocalMin.Z),
		FVector(LocalMin.X, LocalMax.Y, LocalMin.Z),
		FVector(LocalMax.X, LocalMax.Y, LocalMin.Z),
		FVector(LocalMin.X, LocalMin.Y, LocalMax.Z),
		FVector(LocalMax.X, LocalMin.Y, LocalMax.Z),
		FVector(LocalMin.X, LocalMax.Y, LocalMax.Z),
		FVector(LocalMax.X, LocalMax.Y, LocalMax.Z)
	};

	FVector4 WorldMin4 = FVector4(LocalCorners[0].X, LocalCorners[0].Y, LocalCorners[0].Z, 1.0f) * WorldMatrix;
	FVector4 WorldMax4 = WorldMin4;

	for (int32 CornerIndex = 1; CornerIndex < 8; ++CornerIndex)
	{
		const FVector4 WorldPos = FVector4(LocalCorners[CornerIndex].X
			, LocalCorners[CornerIndex].Y
			, LocalCorners[CornerIndex].Z
			, 1.0f)
			* WorldMatrix;
		WorldMin4 = WorldMin4.ComponentMin(WorldPos);
		WorldMax4 = WorldMax4.ComponentMax(WorldPos);
	}

	FVector WorldMin = FVector(WorldMin4.X, WorldMin4.Y, WorldMin4.Z);
	FVector WorldMax = FVector(WorldMax4.X, WorldMax4.Y, WorldMax4.Z);
	return FAABB(WorldMin, WorldMax);
}

void UStaticMeshComponent::OnUpdateTransform(EUpdateTransformFlags UpdateTransformFlags, ETeleportType Teleport)
{
	Super::OnUpdateTransform(UpdateTransformFlags, Teleport);
	MarkWorldPartitionDirty();
}

void UStaticMeshComponent::DuplicateSubObjects()
{
	Super::DuplicateSubObjects();
}

void UStaticMeshComponent::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	Super::Serialize(bInIsLoading, InOutHandle);
}

UBodySetup* UStaticMeshComponent::GetBodySetup()
{
	if (StaticMesh)
	{
		return StaticMesh->GetBodySetup();
	}
	return nullptr;
}

void UStaticMeshComponent::RenderDebugVolume(URenderer* Renderer) const
{
	if (!Renderer) return;
	if (!GetOwner()) return;

	UWorld* World = GetOwner()->GetWorld();
	if (!World) return;

	// PIE 모드에서는 충돌체 디버그 안 그림
	if (World->bPie) return;

	// NoCollision이면 시각화하지 않음
	if (CollisionEnabled == ECollisionEnabled::NoCollision) return;

	// BodySetup 가져오기
	UBodySetup* BodySetup = StaticMesh ? StaticMesh->GetBodySetup() : nullptr;
	if (!BodySetup) return;

	// Shape가 없으면 리턴
	if (BodySetup->AggGeom.GetElementCount() == 0) return;

	// 컴포넌트 월드 트랜스폼
	FTransform WorldTransform = GetWorldTransform();

	// 디버그 메쉬 생성
	TArray<FVector> Vertices;
	TArray<uint32> Indices;
	TArray<FVector4> Colors;

	FVector4 DebugColor(0.0f, 1.0f, 0.5f, 1.0f); // 청록색 라인

	FPhysicsDebugUtils::GenerateBodyShapeMesh(
		BodySetup,
		WorldTransform,
		DebugColor,
		Vertices,
		Indices,
		Colors);

	// 라인 렌더링 (삼각형 edge 추출)
	// SceneRenderer에서 이미 BeginLineBatch를 호출했으므로, AddLine만 호출
	if (Vertices.Num() > 0 && Indices.Num() >= 3)
	{
		// 각 삼각형의 3개 edge를 라인으로 추가
		for (int32 i = 0; i + 2 < Indices.Num(); i += 3)
		{
			uint32 Idx0 = Indices[i];
			uint32 Idx1 = Indices[i + 1];
			uint32 Idx2 = Indices[i + 2];

			// Edge 0-1
			Renderer->AddLine(Vertices[Idx0], Vertices[Idx1], DebugColor);
			// Edge 1-2
			Renderer->AddLine(Vertices[Idx1], Vertices[Idx2], DebugColor);
			// Edge 2-0
			Renderer->AddLine(Vertices[Idx2], Vertices[Idx0], DebugColor);
		}
	}
}
