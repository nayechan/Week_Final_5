#include "pch.h"
#include "FViewportClient.h"
#include "FViewport.h"
#include "CameraComponent.h"
#include "CameraActor.h"
#include "World.h"
#include "Picking.h"
#include "SelectionManager.h"
#include "Gizmo/GizmoActor.h"
#include "RenderManager.h"
#include "RenderSettings.h"
#include "EditorEngine.h"
#include "PrimitiveComponent.h"

FVector FViewportClient::CameraAddPosition{};

FViewportClient::FViewportClient()
{
    ViewportType = EViewportType::Perspective;
    // 직교 뷰별 기본 카메라 설정
    Camera = NewObject<ACameraActor>();
    SetupCameraMode();
}

FViewportClient::~FViewportClient()
{
}

void FViewportClient::Tick(float DeltaTime) {
    if (PerspectiveCameraInput)
    {
        Camera->ProcessEditorCameraInput(DeltaTime);
    }
    MouseWheel(DeltaTime);
}

void FViewportClient::Draw(FViewport* Viewport)
{
    if (!Viewport || !World) return;

    // 1. 뷰 타입에 따라 카메라 설정 등 사전 작업을 먼저 수행
    switch (ViewportType)
    {
    case EViewportType::Perspective:
    {
        Camera->GetCameraComponent()->SetProjectionMode(ECameraProjectionMode::Perspective);
        break;
    }
    default: // 모든 Orthographic 케이스
    {
        Camera->GetCameraComponent()->SetProjectionMode(ECameraProjectionMode::Orthographic);
        SetupCameraMode();
        break;
    }
    }

    // 2. 렌더링 호출은 뷰 타입 설정이 모두 끝난 후 마지막에 한 번만 수행
    URenderer* Renderer = URenderManager::GetInstance().GetRenderer();
    if (Renderer)
    {
        World->GetRenderSettings().SetViewModeIndex(ViewModeIndex);

        // 더 명확한 이름의 함수를 호출
        Renderer->RenderSceneForView(World, Camera, Viewport);
    }
}

void FViewportClient::SetupCameraMode()
{
    switch (ViewportType)
    {
    case EViewportType::Perspective:

        Camera->SetActorLocation(PerspectiveCameraPosition);
        Camera->SetRotationFromEulerAngles(PerspectiveCameraRotation);
        Camera->GetCameraComponent()->SetFOV(PerspectiveCameraFov);
        Camera->GetCameraComponent()->SetClipPlanes(0.1f, 1000.0f);
        break;
    case EViewportType::Orthographic_Top:

        Camera->SetActorLocation({ CameraAddPosition.X, CameraAddPosition.Y, 1000 });
        Camera->SetActorRotation(FQuat::MakeFromEulerZYX({ 0, 90, 0 }));
        Camera->GetCameraComponent()->SetFOV(100);
        Camera->GetCameraComponent()->SetClipPlanes(0.1f, 1000.0f);
        break;
    case EViewportType::Orthographic_Bottom:

        Camera->SetActorLocation({ CameraAddPosition.X, CameraAddPosition.Y, -1000 });
        Camera->SetActorRotation(FQuat::MakeFromEulerZYX({ 0, -90, 0 }));
        Camera->GetCameraComponent()->SetFOV(100);
        Camera->GetCameraComponent()->SetClipPlanes(0.1f, 1000.0f);
        break;
    case EViewportType::Orthographic_Left:
        Camera->SetActorLocation({ CameraAddPosition.X, 1000 , CameraAddPosition.Z });
        Camera->SetActorRotation(FQuat::MakeFromEulerZYX({ 0, 0, -90 }));
        Camera->GetCameraComponent()->SetFOV(100);
        Camera->GetCameraComponent()->SetClipPlanes(0.1f, 1000.0f);
        break;
    case EViewportType::Orthographic_Right:
        Camera->SetActorLocation({ CameraAddPosition.X, -1000, CameraAddPosition.Z });
        Camera->SetActorRotation(FQuat::MakeFromEulerZYX({ 0, 0, 90 }));
        Camera->GetCameraComponent()->SetFOV(100);
        Camera->GetCameraComponent()->SetClipPlanes(0.1f, 1000.0f);
        break;

    case EViewportType::Orthographic_Front:
        Camera->SetActorLocation({ -1000 , CameraAddPosition.Y, CameraAddPosition.Z });
        Camera->SetActorRotation(FQuat::MakeFromEulerZYX({ 0, 0, 0 }));
        Camera->GetCameraComponent()->SetFOV(100);
        Camera->GetCameraComponent()->SetClipPlanes(0.1f, 1000.0f);
        break;
    case EViewportType::Orthographic_Back:
        Camera->SetActorLocation({ 1000 , CameraAddPosition.Y, CameraAddPosition.Z });
        Camera->SetActorRotation(FQuat::MakeFromEulerZYX({ 0, 0, 180 }));
        Camera->GetCameraComponent()->SetFOV(100);
        Camera->GetCameraComponent()->SetClipPlanes(0.1f, 1000.0f);
        break;
    }
}
void FViewportClient::MouseMove(FViewport* Viewport, int32 X, int32 Y) 
{

    if (World->GetGizmoActor())
        World->GetGizmoActor()->ProcessGizmoInteraction(Camera, Viewport, static_cast<float>(X), static_cast<float>(Y));

    if ( !bIsMouseButtonDown && 
        (!World->GetGizmoActor() || !World->GetGizmoActor()->GetbIsHovering()) &&
        bIsMouseRightButtonDown) // 직교투영이고 마우스 버튼이 눌려있을 때
    {
        if (ViewportType != EViewportType::Perspective) {
            
            int32 deltaX = X - MouseLastX;
            int32 deltaY = Y - MouseLastY;

            if (Camera && (deltaX != 0 || deltaY != 0))
            {
                // 기준 픽셀→월드 스케일
                const float basePixelToWorld = 0.05f;

                // 줌인(값↑)일수록 더 천천히 움직이도록 역수 적용
                float zoom = Camera->GetCameraComponent()->GetZoomFactor();
                zoom = (zoom <= 0.f) ? 1.f : zoom; // 안전장치
                const float pixelToWorld = basePixelToWorld * zoom;

                const FVector right = Camera->GetRight();
                const FVector up = Camera->GetUp();

                CameraAddPosition = CameraAddPosition
                    - right * (deltaX * pixelToWorld)
                    + up * (deltaY * pixelToWorld);

                SetupCameraMode();
            }

            MouseLastX = X;
            MouseLastY = Y;
        }
        else if (ViewportType == EViewportType::Perspective ) {
            PerspectiveCameraInput=true;
        }
    }
   
}
void FViewportClient::MouseButtonDown(FViewport* Viewport, int32 X, int32 Y, int32 Button)
{
    if (!Viewport || !World) // Only handle left mouse button
        return;
    
    // GetInstance viewport size
    FVector2D ViewportSize(static_cast<float>(Viewport->GetSizeX()), static_cast<float>(Viewport->GetSizeY()));
    FVector2D ViewportOffset(static_cast<float>(Viewport->GetStartX()), static_cast<float>(Viewport->GetStartY()));

    // X, Y are already local coordinates within the viewport, convert to global coordinates for picking
    FVector2D ViewportMousePos(static_cast<float>(X) + ViewportOffset.X, static_cast<float>(Y) + ViewportOffset.Y);
    UPrimitiveComponent* PickedComponent = nullptr;
    TArray<AActor*> AllActors = World->GetActors();
    if (Button == 0) 
    {
        if (!World->GetGizmoActor())
            return;

        bIsMouseButtonDown = true;
        // 뷰포트의 실제 aspect ratio 계산
        float PickingAspectRatio = ViewportSize.X / ViewportSize.Y;
        if (ViewportSize.Y == 0) PickingAspectRatio = 1.0f; // 0으로 나누기 방지
        if (World->GetGizmoActor()->GetbIsHovering()) {
            return;
        }
        Camera->SetWorld(World);
        PickedComponent = URenderManager::GetInstance().GetRenderer()->GetPrimitiveCollided(static_cast<int>(ViewportMousePos.X), static_cast<int>(ViewportMousePos.Y));
       // PickedActor = CPickingSystem::PerformViewportPicking(AllActors, Camera, ViewportMousePos, ViewportSize, ViewportOffset, PickingAspectRatio,  Viewport);


        if (PickedComponent)
        {
            if (World) World->GetSelectionManager()->SelectComponent(PickedComponent);
            
        }
        else
        {
            // Clear selection if nothing was picked
            if (World) World->GetSelectionManager()->ClearSelection();
        }
    }
    else if (Button==1)
    {//우클릭시 
        bIsMouseRightButtonDown = true;
        MouseLastX = X;
        MouseLastY = Y;
    }

}

void FViewportClient::MouseButtonUp(FViewport* Viewport, int32 X, int32 Y, int32 Button)
{
    if (Button == 0) // Left mouse button
    {
        bIsMouseButtonDown = false;
    }
    else {
        bIsMouseRightButtonDown = false;
        PerspectiveCameraInput=false;
    }
}

void FViewportClient::MouseWheel(float DeltaSeconds)
{
    if (!Camera) return;

    UCameraComponent* CameraComponent = Camera->GetCameraComponent();
    if (!CameraComponent) return;
    float WheelDelta = UInputManager::GetInstance().GetMouseWheelDelta();

    float zoomFactor = CameraComponent->GetZoomFactor();
    zoomFactor *= (1.0f - WheelDelta * DeltaSeconds * 100.0f);
    
    CameraComponent->SetZoomFactor(zoomFactor);
}

