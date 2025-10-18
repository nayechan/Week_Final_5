#include "pch.h"
#include "GizmoArrowComponent.h"
#include "EditorEngine.h"
#include "MeshBatchElement.h"
#include "SceneView.h"

IMPLEMENT_CLASS(UGizmoArrowComponent)

UGizmoArrowComponent::UGizmoArrowComponent()
{
    SetStaticMesh("Data/Gizmo/TranslationHandle.obj");
    // 임시 코드 MaterialSlots 크기 체크 필요?
    MaterialSlots[0] = UResourceManager::GetInstance().Load<UMaterial>("Shaders/StaticMesh/StaticMeshShader.hlsl");
    //MaterialSlots[0].MaterialName = "Shaders/StaticMesh/StaticMeshShader.hlsl";
    //UShader* FixedShader = UResourceManager::GetInstance().Load<UShader>("Shaders/StaticMesh/StaticMeshShader.hlsl");
}

float UGizmoArrowComponent::ComputeScreenConstantScale(const FSceneView* View, float TargetPixels) const
{
    float H = (float)std::max<uint32>(1, View->ViewRect.Height());
    float W = (float)std::max<uint32>(1, View->ViewRect.Width());

    const FMatrix& Proj = View->ProjectionMatrix;
    const FMatrix& ViewMatrix = View->ViewMatrix;

    const bool bOrtho = std::fabs(Proj.M[3][3] - 1.0f) < KINDA_SMALL_NUMBER;
    float WorldPerPixel = 0.0f;
    if (bOrtho)
    {
        const float HalfH = 1.0f / Proj.M[1][1];
        WorldPerPixel = (2.0f * HalfH) / H;
    }
    else
    {
        const float YScale = Proj.M[1][1];
        const FVector GizPosWorld = GetWorldLocation();
        const FVector4 GizPos4(GizPosWorld.X, GizPosWorld.Y, GizPosWorld.Z, 1.0f);
        const FVector4 ViewPos4 = GizPos4 * ViewMatrix;
        float Z = ViewPos4.Z;
        if (Z < 1.0f) Z = 1.0f;
        WorldPerPixel = (2.0f * Z) / (H * YScale);
    }

    float ScaleFactor = TargetPixels * WorldPerPixel;
    if (ScaleFactor < 0.001f) ScaleFactor = 0.001f;
    if (ScaleFactor > 10000.0f) ScaleFactor = 10000.0f;
    return ScaleFactor;
}

void UGizmoArrowComponent::CollectMeshBatches(
    TArray<FMeshBatchElement>& OutMeshBatchElements,
    const FSceneView* View)
{
    if (!IsActive() || !StaticMesh)
    {
        return; // 그릴 메시 애셋이 없음
    }

    // --- 스케일 계산 ---
    if (!View)
    {
        UE_LOG("GizmoArrowComponent requires a valid FSceneView to compute scale. Gizmo might not scale correctly.");
        return;
    }
    else
    {
        const float ScaleFactor = ComputeScreenConstantScale(View, 30.0f);
        SetWorldScale(DefaultScale * ScaleFactor);
    }

    // --- 사용할 머티리얼 결정 ---
    UMaterial* MaterialToUse = GizmoMaterial;
    UShader* ShaderToUse = nullptr;

    if (MaterialToUse && MaterialToUse->GetShader())
    {
        ShaderToUse = MaterialToUse->GetShader();
    }
    else
    {
        // [Fallback 로직]
        UE_LOG("GizmoArrowComponent: GizmoMaterial is invalid. Falling back to default vertex color shader.");
        MaterialToUse = UResourceManager::GetInstance().Load<UMaterial>("Shaders/StaticMesh/StaticMeshShader.hlsl");
        if (MaterialToUse)
        {
            ShaderToUse = MaterialToUse->GetShader();
        }

        if (!MaterialToUse || !ShaderToUse)
        {
            UE_LOG("GizmoArrowComponent: Default vertex color material/shader not found!");
            return;
        }
    }

    // --- FMeshBatchElement 수집 ---
    const TArray<FGroupInfo>& MeshGroupInfos = StaticMesh->GetMeshGroupInfo();

    // GroupInfos가 비어 있는지 확인
    if (MeshGroupInfos.IsEmpty())
    {
        // --- 단일 메시 처리 ---
        // GroupInfos가 없으면 메시 전체를 단일 배치로 생성합니다.

        // 메시 전체 인덱스 수가 0이면 그릴 수 없습니다.
        if (StaticMesh->GetIndexCount() == 0)
        {
            return;
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
        // --- 서브 메시 처리 (기존 로직) ---
        // GroupInfos가 있으면 각 그룹별로 배치를 생성합니다.
        for (const FGroupInfo& Group : MeshGroupInfos)
        {
            // 그룹의 인덱스 수가 0이면 그릴 수 없습니다.
            if (Group.IndexCount == 0)
            {
                continue;
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

UGizmoArrowComponent::~UGizmoArrowComponent()
{

}

void UGizmoArrowComponent::DuplicateSubObjects()
{
    Super::DuplicateSubObjects();
}
