#include "pch.h"
#include "GammaPass.h"
#include "../SceneView.h"
#include "../../RHI/SwapGuard.h"
#include "../../RHI/ConstantBufferType.h"

void FGammaPass::Execute(const FPostProcessModifier& M, FSceneView* View, D3D11RHI* RHIDevice)
{
    if (!IsApplicable(M)) return;

    // 1) 스왑 + SRV 언바인드 관리 (SceneColorSource 한 장만 읽음)
    FSwapGuard Swap(RHIDevice, /*FirstSlot*/0, /*NumSlotsToUnbind*/1);

    // 2) 타깃 RTV 설정
    RHIDevice->OMSetRenderTargets(ERTVMode::SceneColorTargetWithoutDepth);

    // Depth State: Depth Test/Write 모두 OFF
    RHIDevice->OMSetDepthStencilState(EComparisonFunc::Always);
    RHIDevice->OMSetBlendState(false); // 전화면 덮어쓰기. 필요 시 true + 알파 블렌딩도 가능

    // 3) 셰이더
    UShader* FullScreenTriangleVS = UResourceManager::GetInstance().Load<UShader>(UResourceManager::FullScreenVSPath);
    UShader* GammaPS = UResourceManager::GetInstance().Load<UShader>("Shaders/PostProcess/GammaCorrection_PS.hlsl");
    if (!FullScreenTriangleVS || !FullScreenTriangleVS->GetVertexShader() || !GammaPS || !GammaPS->GetPixelShader())
    {
        UE_LOG("Gamma용 셰이더 없음!\n");
        return;
    }

    RHIDevice->PrepareShader(FullScreenTriangleVS, GammaPS);

    // 4) SRV / Sampler (현재 SceneColorSource)
    ID3D11ShaderResourceView* SceneSRV = RHIDevice->GetSRV(RHI_SRV_Index::SceneColorSource);
    ID3D11SamplerState* LinearClampSamplerState = RHIDevice->GetSamplerState(RHI_Sampler_Index::LinearClamp);

    if (!SceneSRV || !LinearClampSamplerState)
    {
        UE_LOG("Gamma: Scene SRV / LinearClamp Sampler is null!\n");
        return;
    }

    RHIDevice->GetDeviceContext()->PSSetShaderResources(0, 1, &SceneSRV);
    RHIDevice->GetDeviceContext()->PSSetSamplers(0, 1, &LinearClampSamplerState);

    // 5) 상수 버퍼 업데이트 (색상/반경 등)
    FGammaCorrectionBufferType GammaConstant;
    GammaConstant.Gamma = M.Payload.Params0.X;
    
    RHIDevice->SetAndUpdateConstantBuffer(GammaConstant);

    // 7) Draw
    RHIDevice->DrawFullScreenQuad();

    // 8) 확정
    Swap.Commit();
}
