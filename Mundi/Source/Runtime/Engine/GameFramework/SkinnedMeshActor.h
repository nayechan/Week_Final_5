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

    // Convenience: forward to component
    void SetSkeletalMesh(const FString& PathFileName);

    // Rebuild bone line overlay from the current skeletal mesh bind pose
    // SelectedBoneIndex: highlight this bone and its parent connection
    void RebuildBoneLines(int32 SelectedBoneIndex);

    // Copy/Serialize
    void DuplicateSubObjects() override;
    void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;

protected:
    // 스켈레탈 메시를 실제로 렌더링하는 컴포넌트 (미리뷰/프리뷰 액터의 루트로 사용)
    USkinnedMeshComponent* SkinnedMeshComponent = nullptr;
    
    // 본(부모-자식) 연결을 라인으로 그리기 위한 디버그용 컴포넌트
    // - 액터의 로컬 공간에서 선분을 추가하고, 액터 트랜스폼에 따라 함께 이동/회전/스케일됨
    ULineComponent* BoneLineComponent = nullptr;
};
