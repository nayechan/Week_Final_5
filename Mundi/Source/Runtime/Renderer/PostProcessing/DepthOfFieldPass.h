#pragma once
#include "PostProcessing.h"

class FDepthOfFieldPass : public IPostProcessPass
{
public:
    virtual void Execute(const FPostProcessModifier& M, FSceneView* View, D3D11RHI* RHIDevice) override;

private:
    void ExecuteCoCPass(const FPostProcessModifier& M, FSceneView* View, D3D11RHI* RHIDevice);
    void ExecuteBlurPass(const FPostProcessModifier& M, FSceneView* View, D3D11RHI* RHIDevice);
    void ExecuteCompositePass(const FPostProcessModifier& M, FSceneView* View, D3D11RHI* RHIDevice);
};

