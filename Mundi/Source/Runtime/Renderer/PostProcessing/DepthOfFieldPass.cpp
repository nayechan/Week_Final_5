#include "pch.h"
#include "DepthOfFieldPass.h"
#include "../SceneView.h"
#include "../../RHI/SwapGuard.h"
#include "DOFComponent.h"

const char* FDepthOfFieldPass::DOF_CompositePSPath = "Shaders/PostProcess/DOF_Composite_PS.hlsl";
const char* FDepthOfFieldPass::DOF_McIntoshPSPath = "Shaders/PostProcess/DOF_McIntosh_PS.hlsl";
const char* FDepthOfFieldPass::DOF_GaussianBlurPSPath = "Shaders/PostProcess/DOF_GaussianBlur_PS.hlsl";
const char* FDepthOfFieldPass::DOF_CalcCOC_PSPath = "Shaders/PostProcess/DOF_CalcCOC_PS.hlsl";
const char* FDepthOfFieldPass::DOF_COCGaussianBlurPSPath = "Shaders/PostProcess/DOF_COCGaussianBlur_PS.hlsl";

void FDepthOfFieldPass::Execute(const FPostProcessModifier& M, FSceneView* View, D3D11RHI* RHIDevice)
{
    UDOFComponent* Comp = nullptr;
    if (Comp = Cast<UDOFComponent>(M.SourceObject))
    {
        
    }
    else
    {
        return;
    }
    //Common
    FSwapGuard Swap(RHIDevice, /*FirstSlot*/0, /*NumSlotsToUnbind*/2);
    RHIDevice->OMSetDepthStencilState(EComparisonFunc::Always);
    RHIDevice->OMSetBlendState(false);
    ID3D11SamplerState* LinearClampSamplerState = RHIDevice->GetSamplerState(RHI_Sampler_Index::LinearClamp);
    ID3D11SamplerState* PointClampSamplerState = RHIDevice->GetSamplerState(RHI_Sampler_Index::PointClamp);
    ID3D11SamplerState* Smps[2] = { LinearClampSamplerState, PointClampSamplerState };
    RHIDevice->GetDeviceContext()->PSSetSamplers(0, 2, Smps);
    ECameraProjectionMode ProjectionMode = View->ProjectionMode;
    RHIDevice->SetAndUpdateConstantBuffer(PostProcessBufferType(View->NearClip, View->FarClip, ProjectionMode == ECameraProjectionMode::Orthographic));

    //CB
    FDepthOfFieldCB DOFCB = FDepthOfFieldCB(Comp->FocusDistance, Comp->FocusRange, Comp->COCSize);
    FDOFGaussianCB DOFGaussianCB = FDOFGaussianCB(Comp->GaussianBlurWeight, true, true);
    RHIDevice->SetAndUpdateConstantBuffer(DOFCB);

    //RT
    URenderTexture* COCRT = RHIDevice->GetRenderTexture("DOF_COC");
    URenderTexture* BluredCOCRT = RHIDevice->GetRenderTexture("DOF_BluredCOC");
    URenderTexture* NearPing = RHIDevice->GetRenderTexture("DOF_NearPing");
    URenderTexture* NearPong = RHIDevice->GetRenderTexture("DOF_NearPong");
    URenderTexture* FarPing = RHIDevice->GetRenderTexture("DOF_FarPing");
    URenderTexture* FarPong = RHIDevice->GetRenderTexture("DOF_FarPong");
    URenderTexture* CompositeRT = RHIDevice->GetRenderTexture("DOF_Composite");
    URenderTexture* HorizontalTempRT = RHIDevice->GetRenderTexture("HorizontalTemp");
    HorizontalTempRT->InitResolution(RHIDevice, 1.0f);
    COCRT->InitResolution(RHIDevice, 1.0f);
    BluredCOCRT->InitResolution(RHIDevice, 1.0f);
    NearPing->InitResolution(RHIDevice, 1.0f);
    NearPong->InitResolution(RHIDevice, 1.0f);
    FarPing->InitResolution(RHIDevice, 1.0f);
    FarPong->InitResolution(RHIDevice, 1.0f);
    CompositeRT->InitResolution(RHIDevice, 1.0f);

    //CalcCOC
    TArray<ID3D11ShaderResourceView*> SRVs;
    SRVs.push_back(RHIDevice->GetSRV(RHI_SRV_Index::SceneDepth));
    SRVs.push_back(RHIDevice->GetSRV(RHI_SRV_Index::SceneColorSource));
    Pass(RHIDevice, SRVs, COCRT->GetRTV(), DOF_CalcCOC_PSPath);
    
    //COCBlur
    SRVs.clear();
    SRVs.push_back(COCRT->GetSRV());
    RHIDevice->SetAndUpdateConstantBuffer(FDOFGaussianCB(Comp->GaussianBlurWeight, true, true));
    Pass(RHIDevice, SRVs, BluredCOCRT->GetRTV(), DOF_COCGaussianBlurPSPath);
    SRVs[0] = BluredCOCRT->GetSRV();
    RHIDevice->SetAndUpdateConstantBuffer(FDOFGaussianCB(Comp->GaussianBlurWeight, false, true));
    Pass(RHIDevice, SRVs, COCRT->GetRTV(), DOF_COCGaussianBlurPSPath);

    //Gaussian
    //Horizontal Near
    SRVs.clear();
    SRVs.push_back(RHIDevice->GetSRV(RHI_SRV_Index::SceneColorSource));
    SRVs.push_back(COCRT->GetSRV());
    RHIDevice->SetAndUpdateConstantBuffer(FDOFGaussianCB(Comp->GaussianBlurWeight, true, true));
    Pass(RHIDevice, SRVs, NearPing->GetRTV(), DOF_GaussianBlurPSPath);
    //Horizontal Far
    RHIDevice->SetAndUpdateConstantBuffer(FDOFGaussianCB(Comp->GaussianBlurWeight, true, false));
    Pass(RHIDevice, SRVs, FarPing->GetRTV(), DOF_GaussianBlurPSPath);
    //Vertical Near
    SRVs[0] = NearPing->GetSRV();
    RHIDevice->SetAndUpdateConstantBuffer(FDOFGaussianCB(Comp->GaussianBlurWeight, false, true));
    Pass(RHIDevice, SRVs, NearPong->GetRTV(), DOF_GaussianBlurPSPath);
    //Vertical Far
    SRVs[0] = FarPing->GetSRV();
    RHIDevice->SetAndUpdateConstantBuffer(FDOFGaussianCB(Comp->GaussianBlurWeight, false, false));
    Pass(RHIDevice, SRVs, FarPong->GetRTV(), DOF_GaussianBlurPSPath);

    URenderTexture* LastNearSRV = NearPing;
    URenderTexture* LastFarSRV = FarPing;
    URenderTexture* LastNearRTV = NearPong;
    URenderTexture* LastFarRTV = FarPong;

    //McIntosh
    for (int i = 0; i < Comp->McIntoshCount; i++)
    {
        //Horizontal Near
        SRVs[0] = LastNearRTV->GetSRV();
        RHIDevice->SetAndUpdateConstantBuffer(FDOFMcIntoshCB(1, Comp->McIntoshRange));
        Pass(RHIDevice, SRVs, LastNearSRV->GetRTV(), DOF_McIntoshPSPath);
        //Horizontal Far
        SRVs[0] = LastFarRTV->GetSRV();
        RHIDevice->SetAndUpdateConstantBuffer(FDOFMcIntoshCB(1, Comp->McIntoshRange));
        Pass(RHIDevice, SRVs, LastFarSRV->GetRTV(), DOF_McIntoshPSPath);
        //Vertical Near
        SRVs[0] = LastNearSRV->GetSRV();
        RHIDevice->SetAndUpdateConstantBuffer(FDOFMcIntoshCB(0, Comp->McIntoshRange));
        Pass(RHIDevice, SRVs, LastNearRTV->GetRTV(), DOF_McIntoshPSPath);
        //Vertical Far
        SRVs[0] = LastFarSRV->GetSRV();
        RHIDevice->SetAndUpdateConstantBuffer(FDOFMcIntoshCB(0, Comp->McIntoshRange));
        Pass(RHIDevice, SRVs, LastFarRTV->GetRTV(), DOF_McIntoshPSPath);
    }

    //Composite
    UShader* FullScreenTriangleVS = UResourceManager::GetInstance().Load<UShader>(UResourceManager::FullScreenVSPath);
    UShader* CompositePS = UResourceManager::GetInstance().Load<UShader>(DOF_CompositePSPath);
    RHIDevice->PrepareShader(FullScreenTriangleVS, CompositePS);
    RHIDevice->OMSetRenderTargets(ERTVMode::SceneColorTargetWithoutDepth);
    SRVs.clear();
    SRVs.push_back(RHIDevice->GetSRV(RHI_SRV_Index::SceneColorSource));
    SRVs.push_back(COCRT->GetSRV());
    SRVs.push_back(LastNearRTV->GetSRV());
    SRVs.push_back(LastFarRTV->GetSRV());
    RHIDevice->GetDeviceContext()->PSSetShaderResources(0, 4, SRVs.data());
    RHIDevice->DrawFullScreenQuad();

    // 8) 확정
    Swap.Commit();

    ID3D11ShaderResourceView* nulls[4] = {};
    RHIDevice->GetDeviceContext()->PSSetShaderResources(0, 4, nulls);
}

void FDepthOfFieldPass::Pass(D3D11RHI* RHIDevice, TArray<ID3D11ShaderResourceView*> SRVs, ID3D11RenderTargetView* RTV, const char* PSPath)
{
    UShader* FullScreenTriangleVS = UResourceManager::GetInstance().Load<UShader>(UResourceManager::FullScreenVSPath);
    UShader* PS = UResourceManager::GetInstance().Load<UShader>(PSPath);
    RHIDevice->PrepareShader(FullScreenTriangleVS, PS);
    RHIDevice->GetDeviceContext()->OMSetRenderTargets(1, &RTV, nullptr);
    RHIDevice->GetDeviceContext()->PSSetShaderResources(0, SRVs.size(), SRVs.data());
    RHIDevice->DrawFullScreenQuad();
}
