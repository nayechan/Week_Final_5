#include "pch.h"
#include "OutlinePass.h"
#include "../SceneView.h"
#include "../../RHI/SwapGuard.h"
#include "../../RHI/ConstantBufferType.h"

void FOutlinePass::Execute(const FPostProcessModifier& M, FSceneView* View, D3D11RHI* RHIDevice)
{
    if (!IsApplicable(M)) return;

    // 1) 스왑 + SRV 언바인드 관리 (SceneColor + SceneDepth + IdBuffer 읽음)
    FSwapGuard Swap(RHIDevice, /*FirstSlot*/0, /*NumSlotsToUnbind*/3);

    // 2) 타깃 RTV 설정
    RHIDevice->OMSetRenderTargets(ERTVMode::SceneColorTargetWithoutDepth);

    // Depth State: Depth Test/Write 모두 OFF
    RHIDevice->OMSetDepthStencilState(EComparisonFunc::Always);
    RHIDevice->OMSetBlendState(false);

    // 3) 셰이더
    UShader* FullScreenTriangleVS = UResourceManager::GetInstance().Load<UShader>(UResourceManager::FullScreenVSPath);
    UShader* OutlinePS = UResourceManager::GetInstance().Load<UShader>("Shaders/PostProcess/Outline_PS.hlsl");
    if (!FullScreenTriangleVS || !FullScreenTriangleVS->GetVertexShader() || !OutlinePS || !OutlinePS->GetPixelShader())
    {
        UE_LOG("Outline용 셰이더 없음!\n");
        return;
    }

    RHIDevice->PrepareShader(FullScreenTriangleVS, OutlinePS);

    // 4) SRV / Sampler (SceneColor + SceneDepth + IdBuffer)
    ID3D11ShaderResourceView* SceneSRV = RHIDevice->GetSRV(RHI_SRV_Index::SceneColorSource);
    ID3D11ShaderResourceView* DepthSRV = RHIDevice->GetSRV(RHI_SRV_Index::SceneDepth);
    ID3D11ShaderResourceView* IdSRV = RHIDevice->GetSRV(RHI_SRV_Index::IdBuffer);
    ID3D11SamplerState* LinearClampSampler = RHIDevice->GetSamplerState(RHI_Sampler_Index::LinearClamp);
    ID3D11SamplerState* PointClampSampler = RHIDevice->GetSamplerState(RHI_Sampler_Index::PointClamp);

    if (!SceneSRV || !DepthSRV || !LinearClampSampler || !PointClampSampler)
    {
        UE_LOG("Outline: SRV / Sampler is null!\n");
        return;
    }

    ID3D11DeviceContext* Context = RHIDevice->GetDeviceContext();
    Context->PSSetShaderResources(0, 1, &SceneSRV);
    Context->PSSetShaderResources(1, 1, &DepthSRV);
    Context->PSSetShaderResources(2, 1, &IdSRV);
    Context->PSSetSamplers(0, 1, &LinearClampSampler);
    Context->PSSetSamplers(1, 1, &PointClampSampler);

    // 5) 상수 버퍼 업데이트
    FOutlineBufferType OutlineConstant;
    OutlineConstant.OutlineColor = M.Payload.Color;
    OutlineConstant.Thickness = M.Payload.Params0.X;
    OutlineConstant.DepthThreshold = M.Payload.Params0.Y;
    OutlineConstant.Intensity = M.Payload.Params0.Z;
    OutlineConstant.Weight = M.Weight;
    // Params0.W에 하이라이트할 ObjectID 저장 (0 = 전체 에지)
    OutlineConstant.HighlightedObjectID = static_cast<uint32>(M.Payload.Params1.X);

    RHIDevice->SetAndUpdateConstantBuffer(OutlineConstant);

    // 6) Draw
    RHIDevice->DrawFullScreenQuad();

    // 7) 확정
    Swap.Commit();
}
