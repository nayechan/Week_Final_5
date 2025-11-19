#pragma once
#include "AnimationViewerViewportClient.h"
#include "CameraActor.h"

// Dedicated viewport client for the Blend Space 2D Editor.
// Reuses animation viewer camera/input and rendering behavior.
class FBlendSpaceEditorViewportClient : public FAnimationViewerViewportClient
{
public:
    FBlendSpaceEditorViewportClient();
    ~FBlendSpaceEditorViewportClient() override = default;

    // Rendering entry
    void Draw(FViewport* Viewport) override;
};
