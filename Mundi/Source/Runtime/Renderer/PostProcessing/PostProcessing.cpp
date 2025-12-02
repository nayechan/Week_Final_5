#include "pch.h"
#include "PostProcessing.h"

void IPostProcessPass::Pass(D3D11RHI* RHIDevice, TArray<ID3D11ShaderResourceView*> SRVs, ID3D11RenderTargetView* RTV, const char* PSPath)
{
    UShader* FullScreenTriangleVS = UResourceManager::GetInstance().Load<UShader>(UResourceManager::FullScreenVSPath);
    UShader* PS = UResourceManager::GetInstance().Load<UShader>(PSPath);
    RHIDevice->PrepareShader(FullScreenTriangleVS, PS);
    RHIDevice->GetDeviceContext()->OMSetRenderTargets(1, &RTV, nullptr);
    RHIDevice->GetDeviceContext()->PSSetShaderResources(0, SRVs.size(), SRVs.data());
    RHIDevice->DrawFullScreenQuad();
}