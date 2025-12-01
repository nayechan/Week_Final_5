#pragma once
#include  "PostProcessing.h"

class FSceneView;
class UDOFComponent;
class FDepthOfFieldPass final : public IPostProcessPass
{
public:
    static const char* DOF_CompositePSPath;
    static const char* DOF_McIntoshPSPath;
    static const char* DOF_GaussianBlurPSPath;
    static const char* DOF_CalcCOC_PSPath;
    static const char* DOF_COCGaussianBlurPSPath;
public:
    virtual void Execute(const FPostProcessModifier& M, FSceneView* View, D3D11RHI* RHIDevice) override;

private:
    void Pass(D3D11RHI* RHIDevice, TArray<ID3D11ShaderResourceView*> SRVs, ID3D11RenderTargetView* RTV, const char* PSPath);
};
