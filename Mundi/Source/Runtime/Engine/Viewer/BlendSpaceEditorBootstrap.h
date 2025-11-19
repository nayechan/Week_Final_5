#pragma once
class ViewerState;
class UWorld;
struct ID3D11Device;

// Bootstrap helpers to construct/destroy per-tab viewer state for the BlendSpace editor
class BlendSpaceEditorBootstrap
{
public:
    static ViewerState* CreateViewerState(const char* Name, UWorld* InWorld, ID3D11Device* InDevice);
    static void DestroyViewerState(ViewerState*& State);
};

