#include "pch.h"
#include "StaticMeshComponent.h"
#include "StaticMesh.h"
#include "Shader.h"
#include "Texture.h"
#include "ResourceManager.h"
#include "ObjManager.h"
#include "SceneLoader.h"
#include "World.h"
#include "WorldPartitionManager.h"

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
    Renderer->GetRHIDevice()->UpdateConstantBuffers(GetWorldMatrix(), ViewMatrix, ProjectionMatrix);
    Renderer->GetRHIDevice()->PrepareShader(GetMaterial()->GetShader());
    Renderer->DrawIndexedPrimitiveComponent(GetStaticMesh(), D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST, MaterialSlots);
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
    if (MaterialSlots.size() < GroupInfos.size())
    {
        MaterialSlots.resize(GroupInfos.size());
    }

    // MaterailSlots.size()가 GroupInfos.size() 보다 클 수 있기 때문에, GroupInfos.size()로 설정
    for (int i = 0; i < GroupInfos.size(); ++i) 
    {
        if (MaterialSlots[i].bChangedByUser == false)
        {
            MaterialSlots[i].MaterialName = GroupInfos[i].InitialMaterialName;
        }
    }
    MarkWorldPartitionDirty();
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
    assert((0 <= InMaterialSlotIndex && InMaterialSlotIndex < MaterialSlots.size()) && "out of range InMaterialSlotIndex");

    if (0 <= InMaterialSlotIndex && InMaterialSlotIndex < MaterialSlots.size())
    {
        MaterialSlots[InMaterialSlotIndex].MaterialName = InMaterialName;
        MaterialSlots[InMaterialSlotIndex].bChangedByUser = true;

        bChangedMaterialByUser = true;
    }
    else
    {
        UE_LOG("out of range InMaterialSlotIndex: %d", InMaterialSlotIndex);
    }

    assert(MaterialSlots[InMaterialSlotIndex].bChangedByUser == true);
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
