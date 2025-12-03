#include "pch.h"
#include "ClothComponent.h"
#include "Source/Runtime/Engine/Cloth/ClothManager.h"
#include "D3D11RHI.h"
#include "MeshBatchElement.h"
#include "SceneView.h"
#include "Source/Runtime/Engine/Cloth/ClothMesh.h"

UClothComponent::UClothComponent()
{
	bCanEverTick = true;
	ClothInstance = NewObject<UClothMeshInstance>();
	ClothInstance->Init(UClothManager::Instance->GetTestCloth());
}
UClothComponent::~UClothComponent()
{
	DeleteObject(ClothInstance);
	ClothInstance = nullptr;
}

void UClothComponent::BeginPlay()
{
	UClothManager::Instance->RegisterCloth(ClothInstance->Cloth);
}
void UClothComponent::TickComponent(float DeltaSeconds)
{
	Super::TickComponent(DeltaSeconds);
	ClothInstance->Sync();
	FVector Down = FVector(0, 0, -1);
	FQuat WorldRot = GetWorldRotation();
	FVector WorldDown = WorldRot.Inverse().RotateVector(Down) * Gravity;
	UE_LOG("%f,%f,%f", WorldDown.X, WorldDown.Y, WorldDown.Z);
	ClothInstance->Cloth->setGravity(PxVec3(WorldDown.X, WorldDown.Y, WorldDown.Z));
}
void UClothComponent::EndPlay()
{
	UClothManager::Instance->UnRegisterCloth(ClothInstance->Cloth);
}
void UClothComponent::DuplicateSubObjects()
{
	Super::DuplicateSubObjects();
	ClothInstance = ClothInstance->GetDuplicated();
}

void UClothComponent::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	auto temp = this;
	auto temp2 = &Winds;
	Super::Serialize(bInIsLoading, InOutHandle);
}
void UClothComponent::CollectMeshBatches(TArray<FMeshBatchElement>& OutMeshBatchElements, const FSceneView* View)
{
	TArray<FShaderMacro> ShaderMacros = View->ViewShaderMacros;
	UShader* UberShader = UResourceManager::GetInstance().Load<UShader>("Shaders/Materials/UberLit.hlsl");
	FShaderVariant* ShaderVariant = UberShader->GetOrCompileShaderVariant(ShaderMacros);

	FMeshBatchElement BatchElement;
	if (ShaderVariant)
	{
		BatchElement.VertexShader = ShaderVariant->VertexShader;
		BatchElement.PixelShader = ShaderVariant->PixelShader;
		BatchElement.InputLayout = ShaderVariant->InputLayout;
	}
	else
	{
		BatchElement.InputLayout = UberShader->GetInputLayout();
		BatchElement.VertexShader = UberShader->GetVertexShader();
		BatchElement.PixelShader = UberShader->GetPixelShader();
	}
	BatchElement.VertexBuffer = ClothInstance->VertexBuffer;
	BatchElement.IndexBuffer = ClothInstance->IndexBuffer;
	BatchElement.VertexStride = sizeof(FVertexDynamic);
	BatchElement.IndexCount = ClothInstance->Indices.size();
	BatchElement.BaseVertexIndex = 0;
	BatchElement.WorldMatrix = GetWorldMatrix();
	BatchElement.ObjectID = InternalIndex;
	BatchElement.PrimitiveTopology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	OutMeshBatchElements.Add(BatchElement);
}
