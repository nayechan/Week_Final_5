#pragma once
#include "Actor.h"
#include "SkinnedMeshComponent.h"
#include "LineComponent.h"
#include "ASkinnedMeshActor.generated.h"

UCLASS(DisplayName="스킨드 메시", Description="스켈레탈(스킨드) 메시를 배치하는 액터입니다")

class ASkinnedMeshActor : public AActor
{
public:
    GENERATED_REFLECTION_BODY()

    ASkinnedMeshActor();
    ~ASkinnedMeshActor() override;

    // AActor
    void Tick(float DeltaTime) override;
    FAABB GetBounds() const override;

    // 컴포넌트 접근자
    USkinnedMeshComponent* GetSkinnedMeshComponent() const { return SkinnedMeshComponent; }
    void SetSkinnedMeshComponent(USkinnedMeshComponent* InComp);
    
    // - 본 오버레이(뼈대 선) 시각화를 위한 라인 컴포넌트
    ULineComponent* GetBoneLineComponent() const { return BoneLineComponent; }
    class UBoneAnchorComponent* GetBoneGizmoAnchor() const { return BoneAnchor; }

    // Convenience: forward to component
    void SetSkeletalMesh(const FString& PathFileName);

    // Rebuild bone line overlay from the current skeletal mesh bind pose
    // SelectedBoneIndex: highlight this bone and its parent connection
    void RebuildBoneLines(int32 SelectedBoneIndex);

    // Move a hidden anchor component to the specified bone's world position
    // and make it selectable for the gizmo to latch onto
    void MoveGizmoToBone(int32 BoneIndex);

    // Copy/Serialize
    void DuplicateSubObjects() override;
    void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;

protected:
    // 스켈레탈 메시를 실제로 렌더링하는 컴포넌트 (미리뷰/프리뷰 액터의 루트로 사용)
    USkinnedMeshComponent* SkinnedMeshComponent = nullptr;
    
    // 본(부모-자식) 연결을 라인으로 그리기 위한 디버그용 컴포넌트
    // - 액터의 로컬 공간에서 선분을 추가하고, 액터 트랜스폼에 따라 함께 이동/회전/스케일됨
    ULineComponent* BoneLineComponent = nullptr;
    // Anchor component used for gizmo selection/transform at a bone
    class UBoneAnchorComponent* BoneAnchor = nullptr;

    // Incremental bone line overlay cache (avoid ClearLines every frame)
    struct FBoneDebugLines
    {
        ULine* ParentLine = nullptr;      // parent-child connection
        TArray<ULine*> Rings;             // 3 * NumSegments lines per bone
    };

    bool bBoneLinesInitialized = false;
    int32 CachedSegments = 16;
    int32 CachedSelected = -1;
    TArray<FBoneDebugLines> BoneLinesCache; // size == BoneCount
    TArray<TArray<int32>> BoneChildren;     // adjacency for subtree updates

    void BuildBoneLinesCache();
    void UpdateBoneSubtreeTransforms(int32 BoneIndex);
    void UpdateBoneSelectionHighlight(int32 SelectedBoneIndex);
};
