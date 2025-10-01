#include "pch.h"
#include "StaticMeshComponent.h"
#include "StaticMesh.h"
#include "Shader.h"
#include "Texture.h"
#include "ResourceManager.h"
#include "ObjManager.h"
#include "SceneLoader.h"

UStaticMeshComponent::UStaticMeshComponent()
{
    SetMaterial("StaticMeshShader.hlsl", EVertexLayoutType::PositionColorTexturNormal);
}

UStaticMeshComponent::~UStaticMeshComponent()
{
    if (StaticMesh != nullptr)
    {
        StaticMesh->EraseUsingComponets(this);
    }
}

void UStaticMeshComponent::Render(URenderer* Renderer, const FMatrix& ViewMatrix, const FMatrix& ProjectionMatrix)
{
    UStaticMesh* Mesh = GetStaticMesh();
    if (!Mesh)
    {
        return; // 아직 메쉬가 지정되지 않은 새 컴포넌트는 렌더 패스에서 건너뛴다.
    }
    Renderer->UpdateConstantBuffer(GetWorldMatrix(), ViewMatrix, ProjectionMatrix);
    Renderer->PrepareShader(GetMaterial()->GetShader());
    Renderer->DrawIndexedPrimitiveComponent(GetStaticMesh(), D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST, MaterailSlots);
}

void UStaticMeshComponent::SetStaticMesh(const FString& PathFileName)
{
    if (StaticMesh != nullptr)
    {
        StaticMesh->EraseUsingComponets(this);
    }
	StaticMesh = FObjManager::LoadObjStaticMesh(PathFileName);
    StaticMesh->AddUsingComponents(this);
    
    const TArray<FGroupInfo>& GroupInfos = StaticMesh->GetMeshGroupInfo();
    if (MaterailSlots.size() < GroupInfos.size())
    {
        MaterailSlots.resize(GroupInfos.size());
    }

    // MaterailSlots.size()가 GroupInfos.size() 보다 클 수 있기 때문에, GroupInfos.size()로 설정
    for (int i = 0; i < GroupInfos.size(); ++i) 
    {
        if (MaterailSlots[i].bChangedByUser == false)
        {
            MaterailSlots[i].MaterialName = GroupInfos[i].InitialMaterialName;
        }
    }
}

void UStaticMeshComponent::Serialize(bool bIsLoading, FPrimitiveData& InOut)
{
    // 0) 트랜스폼 직렬화/역직렬화는 상위(UPrimitiveComponent)에서 처리
    UPrimitiveComponent::Serialize(bIsLoading, InOut);

    if (bIsLoading)
    {
        // 1) 신규 포맷: ObjStaticMeshAsset가 있으면 우선 사용
        if (!InOut.ObjStaticMeshAsset.empty())
        {
            SetStaticMesh(InOut.ObjStaticMeshAsset);
            return;
        }

        // 2) 레거시 호환: Type을 "Data/<Type>.obj"로 매핑
        if (!InOut.Type.empty())
        {
            const FString LegacyPath = "Data/" + InOut.Type + ".obj";
            SetStaticMesh(LegacyPath);
        }
    }
    else
    {
        // 저장 시: 현재 StaticMesh가 있다면 실제 에셋 경로를 기록
        if (UStaticMesh* Mesh = GetStaticMesh())
        {
            InOut.ObjStaticMeshAsset = Mesh->GetAssetPathFileName();
        }
        else
        {
            InOut.ObjStaticMeshAsset.clear();
        }
        // Type은 상위(월드/액터) 정책에 따라 별도 기록 (예: "StaticMeshComp")
    }
}

void UStaticMeshComponent::SetMaterialByUser(const uint32 InMaterialSlotIndex, const FString& InMaterialName)
{
    assert((0 <= InMaterialSlotIndex && InMaterialSlotIndex < MaterailSlots.size()) && "out of range InMaterialSlotIndex");

    if (0 <= InMaterialSlotIndex && InMaterialSlotIndex < MaterailSlots.size())
    {
        MaterailSlots[InMaterialSlotIndex].MaterialName = InMaterialName;
        MaterailSlots[InMaterialSlotIndex].bChangedByUser = true;

        bChangedMaterialByUser = true;
    }
    else
    {
        UE_LOG("out of range InMaterialSlotIndex: %d", InMaterialSlotIndex);
    }

    assert(MaterailSlots[InMaterialSlotIndex].bChangedByUser == true);
}

void UStaticMeshComponent::DuplicateSubObjects()
{
    Super::DuplicateSubObjects();
}
