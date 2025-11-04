#pragma once
#include  "PostProcessing.h"

class FSceneView;
class FHeightFogPass : public IPostProcessPass
{
public:
    virtual void Execute(const FPostProcessModifier& M, FSceneView* View, D3D11RHI* RHIDevice) override;
};
