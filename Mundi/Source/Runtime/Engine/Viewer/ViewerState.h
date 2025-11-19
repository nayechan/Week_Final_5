#pragma once

class UWorld; class FViewport; class FViewportClient; class ASkeletalMeshActor; class USkeletalMesh; class UAnimSequence;

struct FAnimNotifyEvent
{
    float TriggerTime;
    float Duration;
    FName NotifyName;
    FString SoundPath;
    FLinearColor Color = FLinearColor(1.0f, 1.0f, 1.0f, 1.0f);
};

struct FNotifyTrack
{
    FString Name;
    TArray<FAnimNotifyEvent> Notifies;

    FNotifyTrack() = default;
    FNotifyTrack(const FString& InName) : Name(InName) {}
};

struct FSelectedNotify
{
    int32 TrackIndex = -1;
    int32 NotifyIndex = -1;

    bool IsValid() const { return TrackIndex != -1 && NotifyIndex != -1; }
    void Invalidate() { TrackIndex = -1; NotifyIndex = -1; }
};

class ViewerState
{
public:
    FName Name;
    UWorld* World = nullptr;
    FViewport* Viewport = nullptr;
    FViewportClient* Client = nullptr;
    
    // Have a pointer to the currently selected mesh to render in the viewer
    ASkeletalMeshActor* PreviewActor = nullptr;
    USkeletalMesh* CurrentMesh = nullptr;
    FString LoadedMeshPath;  // Track loaded mesh path for unloading
    int32 SelectedBoneIndex = -1;
    bool bShowMesh = true;
    bool bShowBones = true;
    // Bone line rebuild control
    bool bBoneLinesDirty = true;      // true면 본 라인 재구성
    int32 LastSelectedBoneIndex = -1; // 색상 갱신을 위한 이전 선택 인덱스
    // UI path buffer per-tab
    char MeshPathBuffer[260] = {0};
    std::set<int32> ExpandedBoneIndices;

    // 본 트랜스폼 편집 관련
    FVector EditBoneLocation;
    FVector EditBoneRotation;  // Euler angles in degrees
    FVector EditBoneScale;
    
    bool bBoneTransformChanged = false;
    bool bBoneRotationEditing = false;
    bool bRequestScrollToBone = false;

    // Animation State
    UAnimSequence* CurrentAnimation = nullptr;
    bool bIsPlaying = false;
    bool bIsLooping = true;
    bool bReversePlay = false;
    float PlaybackSpeed = 1.0f;
    float CurrentTime = 0.0f;
    float TotalTime = 0.0f;

    bool bIsScrubbing = false;

    float PreviousTime = 0.0f;  // The animation time from the previous frame to detect notify triggers

    TArray<UAnimSequence*> CompatibleAnimations;
    bool bShowOnlyCompatible = false;
    
    TArray<FNotifyTrack> NotifyTracks;

    bool bFoldNotifies = false;
    bool bFoldCurves = false;
    bool bFoldAttributes = false;

    FSelectedNotify SelectedNotify;

    // Additive bone transforms applied on top of animation
    TMap<int32, FTransform> BoneAdditiveTransforms;

    // 기즈모 드래그 첫 프레임 감지용 (부동소수점 오차로 인한 불필요한 업데이트 방지)
    bool bWasGizmoDragging = false;
};
