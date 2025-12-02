#include "pch.h"
#include "DepthOfFieldPass.h"
#include "../SceneView.h"
#include "../../RHI/SwapGuard.h"
#include "DOFComponent.h"

const char* FDepthOfFieldPass::DOF_CompositePSPath = "Shaders/PostProcess/DOF_Composite_PS.hlsl";
const char* FDepthOfFieldPass::DOF_GaussianBlurPSPath = "Shaders/PostProcess/DOF_GaussianBlur_PS.hlsl";
const char* FDepthOfFieldPass::DOF_CalcCOC_PSPath = "Shaders/PostProcess/DOF_CalcCOC_PS.hlsl";
const char* FDepthOfFieldPass::DOF_COCGaussianBlurPSPath = "Shaders/PostProcess/DOF_COCGaussianBlur_PS.hlsl";
const char* FDepthOfFieldPass::DownSamplingPSPath = "Shaders/PostProcess/DownSampling_PS.hlsl";
const char* FDepthOfFieldPass::UpSamplingPSPath = "Shaders/PostProcess/UpSampling_PS.hlsl";

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
    RHIDevice->GetViewportHeight();
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
    FDepthOfFieldCB DOFCB = FDepthOfFieldCB(Comp->FocusDistance, Comp->FocusRange, 1);
    FDOFGaussianCB DOFGaussianCB = FDOFGaussianCB(Comp->GaussianBlurWeight, true, true);
    RHIDevice->SetAndUpdateConstantBuffer(DOFCB);

    //RT
    URenderTexture* COCRT = RHIDevice->GetRenderTexture("DOF_COC");
    URenderTexture* TempRT = RHIDevice->GetRenderTexture("DOF_Temp");
    URenderTexture* BluredCOCRT = RHIDevice->GetRenderTexture("DOF_BluredCOC");
    URenderTexture* SceneDownSampleRT = RHIDevice->GetRenderTexture("DOF_DownSample");
    URenderTexture* NearPing = RHIDevice->GetRenderTexture("DOF_NearPing");
    URenderTexture* NearPong = RHIDevice->GetRenderTexture("DOF_NearPong");
    URenderTexture* FarPing = RHIDevice->GetRenderTexture("DOF_FarPing");
    URenderTexture* FarPong = RHIDevice->GetRenderTexture("DOF_FarPong");
    URenderTexture* NearUpSampleRT = RHIDevice->GetRenderTexture("DOF_NearUpSample");
    URenderTexture* FarUpSampleRT = RHIDevice->GetRenderTexture("DOF_FarUpSample");
    URenderTexture* CompositeRT = RHIDevice->GetRenderTexture("DOF_Composite");
    URenderTexture* HorizontalTempRT = RHIDevice->GetRenderTexture("HorizontalTemp");
    HorizontalTempRT->InitResolution(RHIDevice, 1.0f);
    COCRT->InitResolution(RHIDevice, 1.0f);
    TempRT->InitResolution(RHIDevice, 1.0f);
    BluredCOCRT->InitResolution(RHIDevice, 1.0f);
    if (Comp->bUseDownSampling) 
    {
        SceneDownSampleRT->InitResolution(RHIDevice, 0.5f);
        NearPing->InitResolution(RHIDevice, 0.5f);
        NearPong->InitResolution(RHIDevice, 0.5f);
        FarPing->InitResolution(RHIDevice, 0.5f);
        FarPong->InitResolution(RHIDevice, 0.5f);
    }
    else
    {
        SceneDownSampleRT->InitResolution(RHIDevice, 1.0f);
        NearPing->InitResolution(RHIDevice, 1.0f);
        NearPong->InitResolution(RHIDevice, 1.0f);
        FarPing->InitResolution(RHIDevice, 1.0f);
        FarPong->InitResolution(RHIDevice, 1.0f);
    }
   
    NearUpSampleRT->InitResolution(RHIDevice, 1.0f);
    FarUpSampleRT->InitResolution(RHIDevice, 1.0f);
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
    Pass(RHIDevice, SRVs, TempRT->GetRTV(), DOF_COCGaussianBlurPSPath);
    SRVs[0] = nullptr;
    RHIDevice->GetDeviceContext()->PSSetShaderResources(0, 1, SRVs.data());
    SRVs[0] = TempRT->GetSRV();
    RHIDevice->SetAndUpdateConstantBuffer(FDOFGaussianCB(Comp->GaussianBlurWeight, false, true));
    Pass(RHIDevice, SRVs, BluredCOCRT->GetRTV(), DOF_COCGaussianBlurPSPath);

    if (Comp->bUseDownSampling) 
    {
        //DownSampling
        SRVs.clear();
        SRVs.push_back(RHIDevice->GetSRV(RHI_SRV_Index::SceneColorSource));
        Pass(RHIDevice, SRVs, SceneDownSampleRT->GetRTV(), DownSamplingPSPath);
        SRVs.clear();
        SRVs.push_back(SceneDownSampleRT->GetSRV());
    }
    else
    {
        SRVs.clear();
        SRVs.push_back(RHIDevice->GetSRV(RHI_SRV_Index::SceneColorSource));
    }
    

    //Gaussian
    //Horizontal Near
    SRVs.push_back(BluredCOCRT->GetSRV());
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



    if (Comp->bUseDownSampling)
    {
        //NearUpSampling
        SRVs.clear();
        SRVs.push_back(LastNearRTV->GetSRV());
        Pass(RHIDevice, SRVs, NearUpSampleRT->GetRTV(), UpSamplingPSPath);
        //FarUpSampling
        SRVs.clear();
        SRVs.push_back(LastFarRTV->GetSRV());
        Pass(RHIDevice, SRVs, FarUpSampleRT->GetRTV(), UpSamplingPSPath);

        SRVs.clear();
        SRVs.push_back(RHIDevice->GetSRV(RHI_SRV_Index::SceneColorSource));
        SRVs.push_back(COCRT->GetSRV());
        SRVs.push_back(BluredCOCRT->GetSRV());
        SRVs.push_back(NearUpSampleRT->GetSRV());
        SRVs.push_back(FarUpSampleRT->GetSRV());
    }
    else
    {
        SRVs.clear();
        SRVs.push_back(RHIDevice->GetSRV(RHI_SRV_Index::SceneColorSource));
        SRVs.push_back(COCRT->GetSRV());
        SRVs.push_back(BluredCOCRT->GetSRV());
        SRVs.push_back(LastNearRTV->GetSRV());
        SRVs.push_back(LastFarRTV->GetSRV());
    }
    


    //Composite
    UShader* FullScreenTriangleVS = UResourceManager::GetInstance().Load<UShader>(UResourceManager::FullScreenVSPath);
    UShader* CompositePS = UResourceManager::GetInstance().Load<UShader>(DOF_CompositePSPath);
    RHIDevice->PrepareShader(FullScreenTriangleVS, CompositePS);
    RHIDevice->OMSetRenderTargets(ERTVMode::SceneColorTargetWithoutDepth);
    
    RHIDevice->GetDeviceContext()->PSSetShaderResources(0, 5, SRVs.data());
    RHIDevice->DrawFullScreenQuad();

    // 8) 확정
    Swap.Commit();

    ID3D11ShaderResourceView* nulls[5] = {};
    RHIDevice->GetDeviceContext()->PSSetShaderResources(0, 5, nulls);
    RHIDevice->GetDeviceContext()->OMSetRenderTargets(0, nullptr, nullptr);
}
