#pragma once
#include "FViewportClient.h"

// A dedicated viewport client for the Skeletal Mesh Viewer tab.
// Keeps engine show-flags as-is and lets the user choose any EViewMode.
class FSkeletalViewerViewportClient : public FViewportClient
{
public:
    FSkeletalViewerViewportClient();
    virtual ~FSkeletalViewerViewportClient() override;

    // Rendering entry (uses base behavior to build FSceneView and call renderer)
    virtual void Draw(FViewport* Viewport) override;

    // Per-frame update and input (reuse base camera/input; override for future viewer-specific behavior)
    virtual void Tick(float DeltaTime) override;
    virtual void MouseMove(FViewport* Viewport, int32 X, int32 Y) override;
    virtual void MouseButtonDown(FViewport* Viewport, int32 X, int32 Y, int32 Button) override;
    virtual void MouseButtonUp(FViewport* Viewport, int32 X, int32 Y, int32 Button) override;
    virtual void MouseWheel(float DeltaSeconds) override;
};
