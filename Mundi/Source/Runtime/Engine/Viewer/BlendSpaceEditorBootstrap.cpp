#include "pch.h"
#include "BlendSpaceEditorBootstrap.h"
#include "FViewport.h"
#include "Source/Runtime/Engine/Viewer/ViewerState.h"
#include "Source/Runtime/Renderer/BlendSpaceEditorViewportClient.h"
#include "Source/Runtime/Engine/GameFramework/SkeletalMeshActor.h"

ViewerState* BlendSpaceEditorBootstrap::CreateViewerState(const char* Name, UWorld* InWorld, ID3D11Device* InDevice)
{
    if (!InDevice) return nullptr;

    ViewerState* State = new ViewerState();
    State->Name = Name ? Name : "BlendSpace2D";

    // Preview world
    State->World = NewObject<UWorld>();
    State->World->SetWorldType(EWorldType::PreviewMinimal);
    State->World->Initialize();
    State->World->GetRenderSettings().DisableShowFlag(EEngineShowFlags::SF_EditorIcon);

    // Viewport
    State->Viewport = new FViewport();
    State->Viewport->Initialize(0, 0, 1, 1, InDevice);

    // Client
    auto* Client = new FBlendSpaceEditorViewportClient();
    Client->SetWorld(State->World);
    Client->SetViewportType(EViewportType::Perspective);
    Client->SetViewMode(EViewMode::VMI_Lit_Phong);
    State->Client = Client;
    State->Viewport->SetViewportClient(Client);
    State->World->SetEditorCameraActor(Client->GetCamera());

    // Preview actor for skeletal mesh
    if (State->World)
    {
        ASkeletalMeshActor* Preview = State->World->SpawnActor<ASkeletalMeshActor>();
        State->PreviewActor = Preview;
        State->CurrentMesh = Preview && Preview->GetSkeletalMeshComponent()
            ? Preview->GetSkeletalMeshComponent()->GetSkeletalMesh() : nullptr;
    }

    return State;
}

void BlendSpaceEditorBootstrap::DestroyViewerState(ViewerState*& State)
{
    if (!State) return;
    if (State->Viewport) { delete State->Viewport; State->Viewport = nullptr; }
    if (State->Client) { delete State->Client; State->Client = nullptr; }
    if (State->World) { ObjectFactory::DeleteObject(State->World); State->World = nullptr; }
    delete State; State = nullptr;
}

