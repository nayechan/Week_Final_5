#include "pch.h"
#include "StaticMeshComponent.h"
#include "StaticMesh.h"
#include "Shader.h"
#include "Texture.h"
#include "ResourceManager.h"
#include "ObjManager.h"
#include "World.h"
#include "WorldPartitionManager.h"
#include "JsonSerializer.h"
#include "CameraActor.h"
#include "CameraComponent.h"
#include "MeshBatchElement.h"

IMPLEMENT_CLASS(UStaticMeshComponent)

BEGIN_PROPERTIES(UStaticMeshComponent)
MARK_AS_COMPONENT("스태틱 메시 컴포넌트", "스태틱 메시를 렌더링하는 컴포넌트입니다.")
ADD_PROPERTY_STATICMESH(UStaticMesh*, StaticMesh, "Static Mesh", true, "렌더링할 스태틱 메시입니다.")
END_PROPERTIES()

UStaticMeshComponent::UStaticMeshComponent()
{
	SetStaticMesh("Data/cube-tex.obj");     // 임시 기본 static mesh 설정

	//// 기본 Material 생성 (기본 Phong 셰이더 사용)
	//FString ShaderPath = "Shaders/Materials/UberLit.hlsl";
	//TArray<FShaderMacro> DefaultMacros;
	//DefaultMacros.push_back(FShaderMacro{ "LIGHTING_MODEL_PHONG", "1" });

	//UShader* DefaultShader = UResourceManager::GetInstance().Load<UShader>(ShaderPath, DefaultMacros);
	//if (DefaultShader)
	//{
	//	Material = UResourceManager::GetInstance().GetOrCreateMaterial(
	//		ShaderPath + "_DefaultMaterial"
	//	);
	//	Material->SetShader(DefaultShader);
	//}
}

UStaticMeshComponent::~UStaticMeshComponent()
{
	if (StaticMesh != nullptr)
	{
		StaticMesh->EraseUsingComponets(this);
	}
}

void UStaticMeshComponent::SetViewModeShader(UShader* InShader)
{
	if (!InShader)
		return;

	for (int i = 0; i < MaterialSlots.Num(); i++)
	{
		MaterialSlots[i]->SetShader(InShader);
	}
}

void UStaticMeshComponent::Render(URenderer* Renderer, const FMatrix& ViewMatrix, const FMatrix& ProjectionMatrix)
{
	//// NOTE: 기즈모 출력을 위해 일단 남겨둠

	//UStaticMesh* Mesh = GetStaticMesh();
	//if (Mesh && Mesh->GetStaticMeshAsset())
	//{
	//	FMatrix WorldMatrix = GetWorldMatrix();
	//	FMatrix WorldInverseTranspose = WorldMatrix.InverseAffine().Transpose();
	//	Renderer->GetRHIDevice()->SetAndUpdateConstantBuffer(ModelBufferType(WorldMatrix, WorldInverseTranspose));
	//	Renderer->GetRHIDevice()->SetAndUpdateConstantBuffer(ViewProjBufferType(ViewMatrix, ProjectionMatrix));
	//	Renderer->GetRHIDevice()->SetAndUpdateConstantBuffer(ColorBufferType(FLinearColor(), this->InternalIndex));
	//	// b7: CameraBuffer - Renderer에서 카메라 위치 가져오기
	//	FVector CameraPos = FVector::Zero();
	//	if (ACameraActor* Camera = Renderer->GetCurrentCamera())
	//	{
	//		if (UCameraComponent* CamComp = Camera->GetCameraComponent())
	//		{
	//			CameraPos = CamComp->GetWorldLocation();
	//		}
	//	}
	//	Renderer->GetRHIDevice()->SetAndUpdateConstantBuffer(CameraBufferType(CameraPos, 0.0f));

	//	Renderer->GetRHIDevice()->PrepareShader(GetMaterial(0)->GetShader());
	//	Renderer->DrawIndexedPrimitiveComponent(GetStaticMesh(), D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST, MaterialSlots);
	//}
}

void UStaticMeshComponent::CollectMeshBatches(
	TArray<FMeshBatchElement>& OutMeshBatchElements,
	const FSceneView* View)
{
	// 1. 렌더링할 메시 애셋이 유효한지 검사
	if (!StaticMesh || !StaticMesh->GetStaticMeshAsset())
	{
		return; // 그릴 메시 데이터 없음
	}

	// 2. 메시의 서브 그룹(섹션) 정보 가져오기
	const TArray<FGroupInfo>& MeshGroupInfos = StaticMesh->GetMeshGroupInfo();

	// 3. 사용할 머티리얼과 셰이더를 결정하는 람다 함수 (코드 중복 방지)
	auto DetermineMaterialAndShader = [&](uint32 SectionIndex) -> std::pair<UMaterial*, UShader*>
		{
			UMaterial* Material = GetMaterial(SectionIndex); // 컴포넌트 슬롯에서 머티리얼 가져오기
			UShader* Shader = nullptr;

			if (Material && Material->GetShader())
			{
				Shader = Material->GetShader();
			}
			else
			{
				// [Fallback 로직]
				// 머티리얼이 없거나 셰이더가 없으면 기본 머티리얼 사용
				UE_LOG("UStaticMeshComponent: Material or Shader invalid for section %u. Falling back to default.", SectionIndex);

				Material = UResourceManager::GetInstance().GetDefaultMaterial(); // 기본 머티리얼 요청
				if (Material)
				{
					Shader = Material->GetShader();
				}

				// 기본 머티리얼/셰이더조차 없으면 렌더링 불가
				if (!Material || !Shader)
				{
					UE_LOG("UStaticMeshComponent: Default material/shader not found!");
					return { nullptr, nullptr }; // 렌더링 불가 표시
				}
			}
			return { Material, Shader };
		};

	// 4. GroupInfos 유무에 따라 분기 처리
	if (MeshGroupInfos.IsEmpty())
	{
		// --- [단일 배치 처리] ---
		// GroupInfos가 없으면 메시 전체를 0번 머티리얼 슬롯을 사용하여 단일 배치로 생성합니다.

		// 메시 전체 인덱스 수가 0이면 그릴 수 없습니다.
		if (StaticMesh->GetIndexCount() == 0)
		{
			return;
		}

		// 0번 슬롯의 머티리얼과 셰이더 결정 (Fallback 포함)
		auto [MaterialToUse, ShaderToUse] = DetermineMaterialAndShader(0);
		if (!MaterialToUse || !ShaderToUse)
		{
			return; // 렌더링 불가
		}

		FMeshBatchElement BatchElement;

		// --- 정렬 키 ---
		BatchElement.VertexShader = ShaderToUse;
		BatchElement.PixelShader = ShaderToUse;
		BatchElement.Material = MaterialToUse;
		BatchElement.Mesh = StaticMesh;

		// --- 드로우 데이터 (메시 전체 범위 사용) ---
		BatchElement.IndexCount = StaticMesh->GetIndexCount(); // 전체 인덱스 수
		BatchElement.StartIndex = 0;                          // 시작은 0
		BatchElement.BaseVertexIndex = 0;

		// --- 인스턴스 데이터 ---
		BatchElement.WorldMatrix = GetWorldMatrix();
		BatchElement.ObjectID = InternalIndex;
		BatchElement.PrimitiveTopology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

		OutMeshBatchElements.Add(BatchElement);
	}
	else
	{
		// --- [서브 메시 처리] ---
		// GroupInfos가 있으면 각 그룹별로 배치를 생성합니다.
		const uint32 NumSections = static_cast<uint32>(MeshGroupInfos.size());
		for (uint32 SectionIndex = 0; SectionIndex < NumSections; ++SectionIndex)
		{
			const FGroupInfo& Group = MeshGroupInfos[SectionIndex];
			// 그룹의 인덱스 수가 0이면 그릴 수 없습니다.
			if (Group.IndexCount == 0)
			{
				continue;
			}

			// 이 섹션의 머티리얼과 셰이더 결정 (Fallback 포함)
			auto [MaterialToUse, ShaderToUse] = DetermineMaterialAndShader(SectionIndex);
			if (!MaterialToUse || !ShaderToUse)
			{
				continue; // 이 섹션 렌더링 불가
			}

			FMeshBatchElement BatchElement;

			// --- 정렬 키 ---
			BatchElement.VertexShader = ShaderToUse;
			BatchElement.PixelShader = ShaderToUse;
			BatchElement.Material = MaterialToUse;
			BatchElement.Mesh = StaticMesh;

			// --- 드로우 데이터 (그룹 정보 사용) ---
			BatchElement.IndexCount = Group.IndexCount;
			BatchElement.StartIndex = Group.StartIndex;
			BatchElement.BaseVertexIndex = 0;

			// --- 인스턴스 데이터 ---
			BatchElement.WorldMatrix = GetWorldMatrix();
			BatchElement.ObjectID = InternalIndex;
			BatchElement.PrimitiveTopology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

			OutMeshBatchElements.Add(BatchElement);
		}
	}
}

void UStaticMeshComponent::SetStaticMesh(const FString& PathFileName)
{
	if (StaticMesh != nullptr)
	{
		StaticMesh->EraseUsingComponets(this);
	}

	StaticMesh = FObjManager::LoadObjStaticMesh(PathFileName);
	if (StaticMesh && StaticMesh->GetStaticMeshAsset())
	{
		StaticMesh->AddUsingComponents(this);

		const TArray<FGroupInfo>& GroupInfos = StaticMesh->GetMeshGroupInfo();
		if (MaterialSlots.size() < GroupInfos.size())
		{
			MaterialSlots.resize(GroupInfos.size());
		}

		// MaterailSlots.size()가 GroupInfos.size() 보다 클 수 있기 때문에, GroupInfos.size()로 설정
		for (int i = 0; i < GroupInfos.size(); ++i)
		{
			MaterialSlots[i] = UResourceManager::GetInstance().Load<UMaterial>(GroupInfos[i].InitialMaterialName);
		}
		MarkWorldPartitionDirty();
	}
}

UMaterial* UStaticMeshComponent::GetMaterial(uint32 InSectionIndex) const
{
	// 1. 머티리얼 슬롯 인덱스가 유효한지 확인합니다.
	if (MaterialSlots.size() <= InSectionIndex)
	{
		return nullptr;
	}

	// 3. 리소스 매니저에서 이름으로 UMaterial 객체를 찾습니다.
	//    (Get<T>는 이미 로드된 리소스를 찾는 것을 가정합니다)
	UMaterial* FoundMaterial = MaterialSlots[InSectionIndex];

	if (!FoundMaterial)
	{
		// 리소스 매니저에 해당 이름의 머티리얼이 없습니다.
		// 이것은 SetStaticMesh에서 머티리얼을 로드/생성하는 로직이
		// 완벽하지 않다는 의미일 수 있습니다.
		UE_LOG("GetMaterial: Failed to find material Section %d", InSectionIndex);
		return nullptr; // TODO: UResourceManager::GetDefaultMaterial() 반환
	}

	return FoundMaterial;
}

//void UStaticMeshComponent::Serialize(bool bIsLoading, FSceneCompData& InOut)
//{
//    // 0) 트랜스폼 직렬화/역직렬화는 상위(UPrimitiveComponent)에서 처리
//    UPrimitiveComponent::Serialize(bIsLoading, InOut);
//
//    if (bIsLoading)
//    {
//        // 1) 신규 포맷: ObjStaticMeshAsset가 있으면 우선 사용
//        if (!InOut.ObjStaticMeshAsset.empty())
//        {
//            SetStaticMesh(InOut.ObjStaticMeshAsset);
//            return;
//        }
//
//        // 2) 레거시 호환: Type을 "Data/<Type>.obj"로 매핑
//        if (!InOut.Type.empty())
//        {
//            const FString LegacyPath = "Data/" + InOut.Type + ".obj";
//            SetStaticMesh(LegacyPath);
//        }
//    }
//    else
//    {
//        // 저장 시: 현재 StaticMesh가 있다면 실제 에셋 경로를 기록
//        if (UStaticMesh* Mesh = GetStaticMesh())
//        {
//            InOut.ObjStaticMeshAsset = Mesh->GetAssetPathFileName();
//        }
//        else
//        {
//            InOut.ObjStaticMeshAsset.clear();
//        }
//        // Type은 상위(월드/액터) 정책에 따라 별도 기록 (예: "StaticMeshComp")
//    }
//}

void UStaticMeshComponent::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	Super::Serialize(bInIsLoading, InOutHandle);

	if (bInIsLoading)
	{
		FString ObjPath;
		FJsonSerializer::ReadString(InOutHandle, "ObjStaticMeshAsset", ObjPath);
		if (!ObjPath.empty())
		{
			SetStaticMesh(ObjPath);
		}
	}
	else
	{
		if (UStaticMesh* Mesh = GetStaticMesh())
		{
			InOutHandle["ObjStaticMeshAsset"] = Mesh->GetAssetPathFileName();
		}
		else
		{
			InOutHandle["ObjStaticMeshAsset"] = "";
		}
	}

	AutoSerialize(bInIsLoading, InOutHandle, UStaticMeshComponent::StaticClass());
}

void UStaticMeshComponent::SetMaterialByUser(const uint32 InMaterialSlotIndex, const FString& InMaterialName)
{
	assert((0 <= InMaterialSlotIndex && InMaterialSlotIndex < MaterialSlots.size()) && "out of range InMaterialSlotIndex");

	if (0 <= InMaterialSlotIndex && InMaterialSlotIndex < MaterialSlots.size())
	{
		MaterialSlots[InMaterialSlotIndex] = UResourceManager::GetInstance().Load<UMaterial>(InMaterialName);

		bChangedMaterialByUser = true;
	}
	else
	{
		UE_LOG("out of range InMaterialSlotIndex: %d", InMaterialSlotIndex);
	}

	//assert(MaterialSlots[InMaterialSlotIndex].bChangedByUser == true);
}

void UStaticMeshComponent::SetMaterial(uint32 InMaterialSlotIndex, const FString& InMaterialName)
{
	if (0 <= InMaterialSlotIndex && InMaterialSlotIndex < MaterialSlots.size())
	{
		MaterialSlots[InMaterialSlotIndex] = UResourceManager::GetInstance().Load<UMaterial>(InMaterialName);
	}
	else
	{
		UE_LOG("out of range InMaterialSlotIndex: %d", InMaterialSlotIndex);
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

void UStaticMeshComponent::DuplicateSubObjects()
{
	Super::DuplicateSubObjects();
}

void UStaticMeshComponent::OnTransformUpdatedChildImpl()
{
	MarkWorldPartitionDirty();
}

void UStaticMeshComponent::MarkWorldPartitionDirty()
{
	if (UWorld* World = GetWorld())
	{
		if (UWorldPartitionManager* Partition = World->GetPartitionManager())
		{
			Partition->MarkDirty(this);
		}
	}
}
