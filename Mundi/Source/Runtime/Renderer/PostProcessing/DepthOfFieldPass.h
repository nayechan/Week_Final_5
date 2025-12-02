#pragma once
#include  "PostProcessing.h"

class FSceneView;
class UDOFComponent;
class FDepthOfFieldPass final : public IPostProcessPass
{
public:
    static const char* DOF_CompositePSPath;
    static const char* DOF_GaussianBlurPSPath;
    static const char* DOF_CalcCOC_PSPath;
    static const char* DOF_COCGaussianBlurPSPath;
    static const char* DownSamplingPSPath;
    static const char* UpSamplingPSPath;
public:
    virtual void Execute(const FPostProcessModifier& M, FSceneView* View, D3D11RHI* RHIDevice) override;

private:
};
