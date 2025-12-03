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
	//-x 방향으로 천이 저절로 움직이는 버그 있음
	//해결하면 호민에게 원인좀 알려주셈

	Super::TickComponent(DeltaSeconds);
	ElapsedTime += DeltaSeconds;
	ClothInstance->Sync();
	ClothInstance->Cloth->setGravity(ToPxVec(GetWorldVector(FVector(0, 0, -1) * Gravity)));
	FVector NoizeWind = Wind;
	NoizeWind.X += InvWindFrequency.X < 0.1f ? 0 : cos(ElapsedTime / InvWindFrequency.X) * WindAmplitude.X;
	NoizeWind.Y += InvWindFrequency.Y < 0.1f ? 0 : cos(ElapsedTime / InvWindFrequency.Y) * WindAmplitude.Y;
	NoizeWind.Z += InvWindFrequency.Z < 0.1f ? 0 : cos(ElapsedTime / InvWindFrequency.Z) * WindAmplitude.Z;
	ClothInstance->Cloth->setWindVelocity(ToPxVec(GetWorldVector(NoizeWind)));

	// ===== Stiffness 설정 =====
	   // PhaseConfig 직접 생성
	Fabric& Fabric = ClothInstance->Cloth->getFabric();
	int numPhases = Fabric.getNumPhases();
	std::vector<nv::cloth::PhaseConfig> phaseConfigs(numPhases);

	for (int i = 0; i < numPhases; i++)
	{
		phaseConfigs[i].mPhaseIndex = i;
		phaseConfigs[i].mStiffness = Stiffness;           // 0.0 ~ 1.0
		phaseConfigs[i].mStiffnessMultiplier = 1.0f;
		phaseConfigs[i].mCompressionLimit = 1.0f;
		phaseConfigs[i].mStretchLimit = 1.0f;        // 늘어남 제한
	}

	nv::cloth::Range<nv::cloth::PhaseConfig> range(phaseConfigs.data(), phaseConfigs.data() + numPhases);
	ClothInstance->Cloth->setPhaseConfig(range);
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
FVector UClothComponent::GetWorldVector(const FVector& Vector)
{
	FQuat WorldRot = GetWorldRotation();
	return WorldRot.Inverse().RotateVector(Vector);
}
