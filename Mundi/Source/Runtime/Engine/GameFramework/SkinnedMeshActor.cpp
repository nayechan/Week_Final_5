#include "pch.h"
#include "SkinnedMeshActor.h"

ASkinnedMeshActor::ASkinnedMeshActor()
{
    ObjectName = "Skinned Mesh Actor";

    // 스킨드 메시 렌더용 컴포넌트 생성 및 루트로 설정
    // - 프리뷰 장면에서 메시를 표시하는 실제 렌더링 컴포넌트
    SkinnedMeshComponent = CreateDefaultSubobject<USkinnedMeshComponent>("SkinnedMeshComponent");
    RootComponent = SkinnedMeshComponent;

    // 뼈 라인 오버레이용 컴포넌트 생성 후 루트에 부착
    // - 이 컴포넌트는 "라인 데이터"(시작/끝점, 색상)를 모아 렌더러에 배치합니다.
    // - 액터의 로컬 공간으로 선을 추가하면, 액터의 트랜스폼에 따라 선도 함께 변환됩니다.
    BoneLineComponent = CreateDefaultSubobject<ULineComponent>("BoneLines");
    if (BoneLineComponent && RootComponent)
    {
        // 부모 트랜스폼을 유지하면서(=로컬 좌표 유지) 루트에 붙입니다.
        BoneLineComponent->SetupAttachment(RootComponent, EAttachmentRule::KeepRelative);
    }
}

ASkinnedMeshActor::~ASkinnedMeshActor() = default;

void ASkinnedMeshActor::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
}

FAABB ASkinnedMeshActor::GetBounds() const
{
    // Be robust to component replacement: query current root
    if (auto* Current = Cast<USkinnedMeshComponent>(RootComponent))
    {
        return Current->GetWorldAABB();
    }
    return FAABB();
}

void ASkinnedMeshActor::SetSkinnedMeshComponent(USkinnedMeshComponent* InComp)
{
    SkinnedMeshComponent = InComp;
}

void ASkinnedMeshActor::SetSkeletalMesh(const FString& PathFileName)
{
    if (SkinnedMeshComponent)
    {
        SkinnedMeshComponent->SetSkeletalMesh(PathFileName);
    }
}

void ASkinnedMeshActor::RebuildBoneLines(int32 SelectedBoneIndex)
{
    if (!BoneLineComponent)
    {
        return;
    }

    BoneLineComponent->ClearLines();

    if (!SkinnedMeshComponent)
    {
        return;
    }

    USkeletalMesh* SkeletalMesh = SkinnedMeshComponent->GetSkeletalMesh();
    if (!SkeletalMesh)
    {
        return;
    }

    const FSkeletalMeshData* Data = SkeletalMesh->GetSkeletalMeshData();
    if (!Data)
    {
        return;
    }

    const auto& Bones = Data->Skeleton.Bones;
    const int32 BoneCount = static_cast<int32>(Bones.size());
    if (BoneCount <= 0)
    {
        return;
    }

    TArray<FVector> JointPos; JointPos.resize(BoneCount);
    const FVector4 Origin(0, 0, 0, 1);
    for (int32 i = 0; i < BoneCount; ++i)
    {
        const FMatrix& Bind = Bones[i].BindPose;
        const FVector4 P = Origin * Bind; // row-vector convention
        JointPos[i] = FVector(P.X, P.Y, P.Z);
    }

    for (int32 i = 0; i < BoneCount; ++i)
    {
        int32 parent = Bones[i].ParentIndex;
        if (parent >= 0 && parent < BoneCount)
        {
            FVector4 color = (SelectedBoneIndex == i || SelectedBoneIndex == parent)
                ? FVector4(1, 0, 0, 1)
                : FVector4(0, 1, 0, 1);
            BoneLineComponent->AddLine(JointPos[parent], JointPos[i], color);
        }
    }
}

void ASkinnedMeshActor::DuplicateSubObjects()
{
    Super::DuplicateSubObjects();
    for (UActorComponent* Component : OwnedComponents)
    {
        if (auto* Comp = Cast<USkinnedMeshComponent>(Component))
        {
            SkinnedMeshComponent = Comp;
            break;
        }
    }
    for (UActorComponent* Component : OwnedComponents)
    {
        if (auto* Comp = Cast<ULineComponent>(Component))
        {
            BoneLineComponent = Comp;
            break;
        }
    }
}

void ASkinnedMeshActor::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
    Super::Serialize(bInIsLoading, InOutHandle);

    if (bInIsLoading)
    {
        SkinnedMeshComponent = Cast<USkinnedMeshComponent>(RootComponent);
    }
}

