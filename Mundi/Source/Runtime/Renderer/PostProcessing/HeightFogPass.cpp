#include "pch.h"
#include "HeightFogPass.h"
#include "../SceneView.h"
#include "../../RHI/SwapGuard.h"
#include "../Components/HeightFogComponent.h"

void FHeightFogPass::Execute(const FPostProcessModifier& M, FSceneView* View, D3D11RHI* RHIDevice)
{
    // 1) 스왑 + SRV 언바인드 관리
    FSwapGuard Swap(RHIDevice, /*FirstSlot*/0, /*NumSlotsToUnbind*/2);

    // 2) 타깃 RTV 설정
    RHIDevice->OMSetRenderTargets(ERTVMode::SceneColorTargetWithoutDepth);

    // Depth State: Depth Test/Write 모두 OFF
    RHIDevice->OMSetDepthStencilState(EComparisonFunc::Always);
    RHIDevice->OMSetBlendState(false);

    // 3) 셰이더
    UShader* FullScreenTriangleVS = UResourceManager::GetInstance().Load<UShader>("Shaders/Utility/FullScreenTriangle_VS.hlsl");
    UShader* HeightFogPS = UResourceManager::GetInstance().Load<UShader>("Shaders/PostProcess/HeightFog_PS.hlsl");
    if (!FullScreenTriangleVS || !FullScreenTriangleVS->GetVertexShader() || !HeightFogPS || !HeightFogPS->GetPixelShader())
    {
        UE_LOG("HeightFog용 셰이더 없음!\n");
        return;   
    }
    
    RHIDevice->PrepareShader(FullScreenTriangleVS, HeightFogPS);

    // 4) SRV/Sampler (깊이 + 현재 SceneColorSource)
    ID3D11ShaderResourceView* DepthSRV = RHIDevice->GetSRV(RHI_SRV_Index::SceneDepth);
    ID3D11ShaderResourceView* SceneSRV   = RHIDevice->GetSRV(RHI_SRV_Index::SceneColorSource);
    ID3D11SamplerState* LinearClampSamplerState = RHIDevice->GetSamplerState(RHI_Sampler_Index::LinearClamp);
    ID3D11SamplerState* PointClampSamplerState  = RHIDevice->GetSamplerState(RHI_Sampler_Index::PointClamp);
    
    if (!DepthSRV || !SceneSRV || !PointClampSamplerState || !LinearClampSamplerState)
    {
        UE_LOG("Depth SRV / Scene SRV / PointClamp Sampler / LinearClamp Sampler is null!\n");
        return;
    }
    
    ID3D11ShaderResourceView* Srvs[2]  = { DepthSRV, SceneSRV };
    RHIDevice->GetDeviceContext()->PSSetShaderResources(0, 2, Srvs);
    
    ID3D11SamplerState* Smps[2] = { LinearClampSamplerState, PointClampSamplerState };
    RHIDevice->GetDeviceContext()->PSSetSamplers(0, 2, Smps);

    // 상수 버퍼 업데이트
    ECameraProjectionMode ProjectionMode = View->ProjectionMode;
    
    RHIDevice->SetAndUpdateConstantBuffer(PostProcessBufferType(View->ZNear, View->ZFar, ProjectionMode == ECameraProjectionMode::Orthographic));
    if (UHeightFogComponent* F = Cast<UHeightFogComponent>(M.SourceObject))
    {
        RHIDevice->SetAndUpdateConstantBuffer(FogBufferType(F->GetFogDensity(), F->GetFogHeightFalloff(), F->GetStartDistance(), F->GetFogCutoffDistance(), F->GetFogInscatteringColor().ToFVector4(), F->GetFogMaxOpacity(), F->GetFogHeight()));   
    }
    else
    {
        UE_LOG("Post Processing's Source Object is Not Fog Component!\n");
        return;
    }
    
    // 7) Draw
    RHIDevice->DrawFullScreenQuad();

    // 8) 확정
    Swap.Commit();
}
