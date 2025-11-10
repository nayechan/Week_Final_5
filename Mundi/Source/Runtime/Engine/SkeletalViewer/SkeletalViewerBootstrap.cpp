#include "pch.h"
#include "SkeletalViewerBootstrap.h"
#include "Source/Runtime/Engine/SkeletalViewer/ViewerState.h"
#include "FViewport.h"
#include "FSkeletalViewerViewportClient.h"
#include "Source/Runtime/Engine/GameFramework/SkinnedMeshActor.h"

ViewerState* SkeletalViewerBootstrap::CreateViewerState(const char* Name, UWorld* InWorld, ID3D11Device* InDevice)
{
    if (!InDevice) return nullptr;

    ViewerState* State = new ViewerState();
    State->Name = Name ? Name : "Viewer";
    
    // Preview world 만들기
    State->World = NewObject<UWorld>();
    State->World->Initialize();

    // Viewport + client per tab
    State->Viewport = new FViewport();
    // 프레임 마다 initial size가 바꿜 것이다
    State->Viewport->Initialize(0, 0, 1, 1, InDevice);

    auto* Client = new FSkeletalViewerViewportClient();
    Client->SetWorld(State->World);
    Client->SetViewportType(EViewportType::Perspective);
    Client->SetViewMode(EViewMode::VMI_Unlit);

    State->Client = Client;
    State->Viewport->SetViewportClient(Client);

    State->World->SetEditorCameraActor(Client->GetCamera());

    // Spawn a persistent preview actor (mesh can be set later from UI)
    if (State->World)
    {
        ASkinnedMeshActor* Preview = State->World->SpawnActor<ASkinnedMeshActor>();
        State->PreviewActor = Preview;
    }

    return State;
}

void SkeletalViewerBootstrap::DestroyViewerState(ViewerState*& State)
{
    if (!State) return;
    if (State->Viewport) { delete State->Viewport; State->Viewport = nullptr; }
    if (State->Client) { delete State->Client; State->Client = nullptr; }
    if (State->World) { ObjectFactory::DeleteObject(State->World); State->World = nullptr; }
    delete State; State = nullptr;
}
