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
void UClothComponent::TickComponent(float DeltaTime)
{
	Super::TickComponent(DeltaTime);

	MappedRange<physx::PxVec4> Particles = ClothInstance->Cloth->getCurrentParticles();
	for (int i = 0; i < Particles.size(); i++)
	{
		PxVec3 PxPos = Particles[i].getXYZ();
		FVector Pos = FVector(PxPos.x, PxPos.y, PxPos.z);
		ClothInstance->Vertices[i].Position = Pos;
		//do something with particles[i]
		//the xyz components are the current positions
		//the w component is the invMass.
	}
	ID3D11DeviceContext* Context = UClothManager::Instance->GetContext();

	D3D11RHI::VertexBufferUpdate(Context, ClothInstance->VertexBuffer, ClothInstance->Vertices);
	//destructor of particles should be called before mCloth is destroyed.
}

void UClothComponent::DuplicateSubObjects()
{
	Super::DuplicateSubObjects();
}

void UClothComponent::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
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
UClothComponent::~UClothComponent()
{
	DeleteObject(ClothInstance);
	ClothInstance = nullptr;
}