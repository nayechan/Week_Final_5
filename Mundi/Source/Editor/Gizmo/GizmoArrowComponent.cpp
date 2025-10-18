#include "pch.h"
#include "GizmoArrowComponent.h"
#include "EditorEngine.h"

IMPLEMENT_CLASS(UGizmoArrowComponent)

UGizmoArrowComponent::UGizmoArrowComponent()
{
    SetStaticMesh("Data/Gizmo/TranslationHandle.obj");
    SetMaterial("Shaders/StaticMesh/StaticMeshShader.hlsl");
}

float UGizmoArrowComponent::ComputeScreenConstantScale(URenderer* Renderer, const FMatrix& View, const FMatrix& Proj, float targetPixels) const
{
    float h = (float)std::max<uint32>(1, Renderer->GetCurrentViewportHeight());
    float w = (float)std::max<uint32>(1, Renderer->GetCurrentViewportWidth());
    if (h <= 1.0f || w <= 1.0f)
    {
        // Fallback to RHI viewport if not set
        if (auto* rhi = dynamic_cast<D3D11RHI*>(Renderer->GetRHIDevice()))
        {
            h = (float)std::max<UINT>(1, rhi->GetViewportHeight());
            w = (float)std::max<UINT>(1, rhi->GetViewportWidth());
        }
    }

    const bool bOrtho = std::fabs(Proj.M[3][3] - 1.0f) < KINDA_SMALL_NUMBER;
    float worldPerPixel = 0.0f;
    if (bOrtho)
    {
        const float halfH = 1.0f / Proj.M[1][1];
        worldPerPixel = (2.0f * halfH) / h;
    }
    else
    {
        const float yScale = Proj.M[1][1];
        const FVector gizPosWorld = GetWorldLocation();
        const FVector4 gizPos4(gizPosWorld.X, gizPosWorld.Y, gizPosWorld.Z, 1.0f);
        const FVector4 viewPos4 = gizPos4 * View;
        float z = viewPos4.Z;
        if (z < 1.0f) z = 1.0f;
        worldPerPixel = (2.0f * z) / (h * yScale);
    }

    float ScaleFactor = targetPixels * worldPerPixel;
    if (ScaleFactor < 0.001f) ScaleFactor = 0.001f;
    if (ScaleFactor > 10000.0f) ScaleFactor = 10000.0f;
    return ScaleFactor;
}

//뷰포트 사이즈에 맞게 기즈모 resize => render 로직에 scale 변경... 
void UGizmoArrowComponent::Render(URenderer* Renderer, const FMatrix& View, const FMatrix& Proj)
{
    if (!IsActive()) return;

    // 모드/상태 적용(오버레이)
    auto& RS = GEngine.GetDefaultWorld()->GetRenderSettings();
    EViewModeIndex saved = RS.GetViewModeIndex();
    Renderer->GetRHIDevice()->RSSetState(ERasterizerMode::Solid);
    // 스텐실에 1을 기록해, 이후 라인 렌더링이 겹치는 픽셀을 그리지 않도록 마스크
    //Renderer->GetRHIDevice()->OMSetBlendState(true);

    // 하이라이트 상수
    Renderer->GetRHIDevice()->SetAndUpdateConstantBuffer(HighLightBufferType(true, FVector(1, 1, 1), AxisIndex, bHighlighted ? 1 : 0, 0, 1));
    Renderer->GetRHIDevice()->SetAndUpdateConstantBuffer(ColorBufferType(FVector4(), 0));
    // 리사이징
    const float ScaleFactor = ComputeScreenConstantScale(Renderer, View, Proj, 30.0f);

    SetWorldScale(DefaultScale * ScaleFactor);

    FMatrix M = GetWorldMatrix();
    FMatrix MInvTranspose = M.InverseAffine().Transpose();
    Renderer->GetRHIDevice()->SetAndUpdateConstantBuffer(ModelBufferType(M, MInvTranspose));
    Renderer->GetRHIDevice()->SetAndUpdateConstantBuffer(ViewProjBufferType(View, Proj));

    UStaticMeshComponent::Render(Renderer, View, Proj);

    // 상태 복구
    //Renderer->GetRHIDevice()->OMSetBlendState(false);
}

UGizmoArrowComponent::~UGizmoArrowComponent()
{

}

void UGizmoArrowComponent::DuplicateSubObjects()
{
    Super::DuplicateSubObjects();
}
