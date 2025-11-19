#include "pch.h"
#include "BlendSpaceEditorViewportClient.h"

FBlendSpaceEditorViewportClient::FBlendSpaceEditorViewportClient()
{
    // Start with defaults suitable for character preview
    ViewportType = EViewportType::Perspective;
    ViewMode = EViewMode::VMI_Lit_Phong;

    // Place camera at a reasonable spot
    Camera->SetActorLocation(FVector(3.0f, 0.0f, 2.0f));
    Camera->SetActorRotation(FVector(0.0f, 0.0f, 180.0f));
    Camera->SetCameraPitch(0.0f);
    Camera->SetCameraYaw(180.0f);
}

void FBlendSpaceEditorViewportClient::Draw(FViewport* Viewport)
{
    FAnimationViewerViewportClient::Draw(Viewport);
}
