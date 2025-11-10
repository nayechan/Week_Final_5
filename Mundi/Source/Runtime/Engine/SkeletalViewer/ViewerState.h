#pragma once

class UWorld; class FViewport; class FViewportClient; class ASkinnedMeshActor; class USkeletalMesh;

class ViewerState
{
public:
    std::string Name;
    UWorld* World = nullptr;
    FViewport* Viewport = nullptr;
    FViewportClient* Client = nullptr;
    
    // Have a pointer to the currently selected mesh to render in the viewer
    ASkinnedMeshActor* PreviewActor = nullptr;
    USkeletalMesh* CurrentMesh = nullptr;
    int32 SelectedBoneIndex = -1;
    bool bShowMesh = true;
    bool bShowBones = true;
    // Bone line rebuild control
    bool bBoneLinesDirty = true;      // true면 본 라인 재구성
    int32 LastSelectedBoneIndex = -1; // 색상 갱신을 위한 이전 선택 인덱스
    // UI path buffer per-tab
    char MeshPathBuffer[260] = {0};
};
