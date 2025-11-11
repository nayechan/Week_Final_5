#include "pch.h"
#include "SkeletalMeshActor.h"
#include "BoneAnchorComponent.h"

ASkeletalMeshActor::ASkeletalMeshActor()
{
    ObjectName = "Skeletal Mesh Actor";

    // 스킨드 메시 렌더용 컴포넌트 생성 및 루트로 설정
    // - 프리뷰 장면에서 메시를 표시하는 실제 렌더링 컴포넌트
    SkeletalMeshComponent = CreateDefaultSubobject<USkeletalMeshComponent>("SkeletalMeshComponent");
    RootComponent = SkeletalMeshComponent;

    // 뼈 라인 오버레이용 컴포넌트 생성 후 루트에 부착
    // - 이 컴포넌트는 "라인 데이터"(시작/끝점, 색상)를 모아 렌더러에 배치합니다.
    // - 액터의 로컬 공간으로 선을 추가하면, 액터의 트랜스폼에 따라 선도 함께 변환됩니다.
    BoneLineComponent = CreateDefaultSubobject<ULineComponent>("BoneLines");
    if (BoneLineComponent && RootComponent)
    {
        // 부모 트랜스폼을 유지하면서(=로컬 좌표 유지) 루트에 붙입니다.
        BoneLineComponent->SetupAttachment(RootComponent, EAttachmentRule::KeepRelative);
        // Render skeleton overlay always on top of geometry
        BoneLineComponent->SetAlwaysOnTop(true);
    }

    // Hidden anchor for gizmo placement on selected bone
    BoneAnchor = CreateDefaultSubobject<UBoneAnchorComponent>("BoneAnchor");
    if (BoneAnchor && RootComponent)
    {
        BoneAnchor->SetupAttachment(RootComponent, EAttachmentRule::KeepRelative);
        BoneAnchor->SetVisibility(false); // not rendered; used only for selection/gizmo
    }
}

ASkeletalMeshActor::~ASkeletalMeshActor() = default;

void ASkeletalMeshActor::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
}

FAABB ASkeletalMeshActor::GetBounds() const
{
    // Be robust to component replacement: query current root
    if (auto* Current = Cast<USkeletalMeshComponent>(RootComponent))
    {
        return Current->GetWorldAABB();
    }
    return FAABB();
}

void ASkeletalMeshActor::SetSkeletalMeshComponent(USkeletalMeshComponent* InComp)
{
    SkeletalMeshComponent = InComp;
}

void ASkeletalMeshActor::SetSkeletalMesh(const FString& PathFileName)
{
    if (SkeletalMeshComponent)
    {
        SkeletalMeshComponent->SetSkeletalMesh(PathFileName);
    }
}

void ASkeletalMeshActor::RebuildBoneLines(int32 SelectedBoneIndex)
{
    if (!BoneLineComponent || !SkeletalMeshComponent)
    {
        return;
    }

    USkeletalMesh* SkeletalMesh = SkeletalMeshComponent->GetSkeletalMesh();
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

    // Initialize cache once per mesh
    if (!bBoneLinesInitialized || BoneLinesCache.Num() != BoneCount)
    {
        BoneLineComponent->ClearLines();
        BuildBoneLinesCache();
        bBoneLinesInitialized = true;
        CachedSelected = -1;
    }

    // Update selection highlight only when changed
    if (CachedSelected != SelectedBoneIndex)
    {
        UpdateBoneSelectionHighlight(SelectedBoneIndex);
        CachedSelected = SelectedBoneIndex;
    }

    // Update transforms only for the selected bone subtree
    if (SelectedBoneIndex >= 0 && SelectedBoneIndex < BoneCount)
    {
        UpdateBoneSubtreeTransforms(SelectedBoneIndex);
    }
}

void ASkeletalMeshActor::MoveGizmoToBone(int32 BoneIndex)
{
    if (!SkeletalMeshComponent || !BoneAnchor)
        return;

    USkeletalMesh* SkeletalMesh = SkeletalMeshComponent->GetSkeletalMesh();
    if (!SkeletalMesh)
        return;

    const FSkeletalMeshData* Data = SkeletalMesh->GetSkeletalMeshData();
    if (!Data)
        return;

    const auto& Bones = Data->Skeleton.Bones;
    if (BoneIndex < 0 || BoneIndex >= (int32)Bones.size())
        return;

    // Wire target/index first, then place anchor without writeback
    BoneAnchor->BeginSuppressWriteback();
    BoneAnchor->SetTarget(SkeletalMeshComponent, BoneIndex);
    BoneAnchor->EndSuppressWriteback();

    BoneAnchor->SetEditability(true);
    BoneAnchor->SetVisibility(true);
}

void ASkeletalMeshActor::DuplicateSubObjects()
{
    Super::DuplicateSubObjects();
    for (UActorComponent* Component : OwnedComponents)
    {
        if (auto* Comp = Cast<USkeletalMeshComponent>(Component))
        {
            SkeletalMeshComponent = Comp;
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

void ASkeletalMeshActor::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
    Super::Serialize(bInIsLoading, InOutHandle);

    if (bInIsLoading)
    {
        SkeletalMeshComponent = Cast<USkeletalMeshComponent>(RootComponent);
    }
}

void ASkeletalMeshActor::BuildBoneLinesCache()
{
    if (!SkeletalMeshComponent || !BoneLineComponent)
    {
        return;
    }
    
    USkeletalMesh* SkeletalMesh = SkeletalMeshComponent->GetSkeletalMesh();
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

    BoneLinesCache.Empty();
    BoneLinesCache.resize(BoneCount);
    BoneChildren.Empty();
    BoneChildren.resize(BoneCount);

    for (int32 i = 0; i < BoneCount; ++i)
    {
        const int32 p = Bones[i].ParentIndex;
        if (p >= 0 && p < BoneCount)
        {
            BoneChildren[p].Add(i);
        }
    }

    // Initial centers from bind pose (object space)
    TArray<FVector> JointPos;
    JointPos.resize(BoneCount);
    const FVector4 Origin(0,0,0,1);
    
    for (int32 i = 0; i < BoneCount; ++i)
    {
        const FVector4 P = Origin * Bones[i].BindPose;
        JointPos[i] = FVector(P.X, P.Y, P.Z);
    }

    const float SmallRadius = 0.1f;
    const int NumSegments = CachedSegments;
    
    for (int32 i = 0; i < BoneCount; ++i)
    {
        FBoneDebugLines& BL = BoneLinesCache[i];
        const FVector Center = JointPos[i];
        const int32 parent = Bones[i].ParentIndex;
        
        if (parent >= 0)
        {
            BL.ParentLine = BoneLineComponent->AddLine(JointPos[parent], Center, FVector4(0,1,0,1));
        }
        
        BL.Rings.reserve(NumSegments*3);
        
        for (int k = 0; k < NumSegments; ++k)
        {
            const float a0 = (static_cast<float>(k) / NumSegments) * TWO_PI;
            const float a1 = (static_cast<float>((k + 1) % NumSegments) / NumSegments) * TWO_PI;
            BL.Rings.Add(BoneLineComponent->AddLine(
                Center + FVector(SmallRadius * std::cos(a0), SmallRadius * std::sin(a0), 0.0f),
                Center + FVector(SmallRadius * std::cos(a1), SmallRadius * std::sin(a1), 0.0f),
                FVector4(0.8f,0.8f,0.8f,1.0f)));
            BL.Rings.Add(BoneLineComponent->AddLine(
                Center + FVector(SmallRadius * std::cos(a0), 0.0f, SmallRadius * std::sin(a0)),
                Center + FVector(SmallRadius * std::cos(a1), 0.0f, SmallRadius * std::sin(a1)),
                FVector4(0.8f,0.8f,0.8f,1.0f)));
            BL.Rings.Add(BoneLineComponent->AddLine(
                Center + FVector(0.0f, SmallRadius * std::cos(a0), SmallRadius * std::sin(a0)),
                Center + FVector(0.0f, SmallRadius * std::cos(a1), SmallRadius * std::sin(a1)),
                FVector4(0.8f,0.8f,0.8f,1.0f)));
        }
    }
}

void ASkeletalMeshActor::UpdateBoneSelectionHighlight(int32 SelectedBoneIndex)
{
    if (!SkeletalMeshComponent)
    {
        return;
    }
    
    USkeletalMesh* SkeletalMesh = SkeletalMeshComponent->GetSkeletalMesh();
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

    const FVector4 SelRing(1.0f, 0.85f, 0.2f, 1.0f);
    const FVector4 NormalRing(0.8f, 0.8f, 0.8f, 1.0f);
    
    for (int32 i = 0; i < BoneCount; ++i)
    {
        const bool bSelected = (i == SelectedBoneIndex);
        const FVector4 RingColor = bSelected ? SelRing : NormalRing;
        FBoneDebugLines& BL = BoneLinesCache[i];
        
        for (ULine* L : BL.Rings)
        {
            if (L) L->SetColor(RingColor);
        }
        
        if (BL.ParentLine)
        {
            const int32 parent = Bones[i].ParentIndex;
            const bool bEdgeSel = (i == SelectedBoneIndex || parent == SelectedBoneIndex);
            BL.ParentLine->SetColor(bEdgeSel ? FVector4(1,0,0,1) : FVector4(0,1,0,1));
        }
    }
}

void ASkeletalMeshActor::UpdateBoneSubtreeTransforms(int32 BoneIndex)
{
    if (!SkeletalMeshComponent)
    {
        return;
    }
    
    USkeletalMesh* SkeletalMesh = SkeletalMeshComponent->GetSkeletalMesh();
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
    if (BoneIndex < 0 || BoneIndex >= BoneCount)
    {
        return;
    }

    TArray<int32> Stack; Stack.Add(BoneIndex);
    TArray<int32> ToUpdate;
    while (!Stack.IsEmpty())
    {
        int32 b = Stack.Last(); Stack.Pop();
        ToUpdate.Add(b);
        for (int32 c : BoneChildren[b]) Stack.Add(c);
    }

    const FMatrix WorldInv = GetWorldMatrix().InverseAffine();
    TArray<FVector> Centers; Centers.resize(BoneCount);
    
    for (int32 b : ToUpdate)
    {
        const FMatrix W = SkeletalMeshComponent->GetBoneWorldTransform(b).ToMatrix();
        const FMatrix O = W * WorldInv;
        Centers[b] = FVector(O.M[3][0], O.M[3][1], O.M[3][2]);
    }

    const float SmallRadius = 0.1f;
    const int NumSegments = CachedSegments;
    for (int32 b : ToUpdate)
    {
        FBoneDebugLines& BL = BoneLinesCache[b];
        const int32 parent = Bones[b].ParentIndex;
        
        if (parent >= 0 && BL.ParentLine)
        {
            const FMatrix ParentW = SkeletalMeshComponent->GetBoneWorldTransform(parent).ToMatrix();
            const FMatrix ParentO = ParentW * WorldInv;
            const FVector ParentCenter(ParentO.M[3][0], ParentO.M[3][1], ParentO.M[3][2]);
            BL.ParentLine->SetLine(ParentCenter, Centers[b]);
        }
        
        const FVector Center = Centers[b];
        
        for (int k = 0; k < NumSegments; ++k)
        {
            const float a0 = (static_cast<float>(k) / NumSegments) * TWO_PI;
            const float a1 = (static_cast<float>((k + 1) % NumSegments) / NumSegments) * TWO_PI;
            const int base = k * 3;
            if (BL.Rings.IsEmpty() || base + 2 >= BL.Rings.Num()) break;
            BL.Rings[base+0]->SetLine(
                Center + FVector(SmallRadius * std::cos(a0), SmallRadius * std::sin(a0), 0.0f),
                Center + FVector(SmallRadius * std::cos(a1), SmallRadius * std::sin(a1), 0.0f));
            BL.Rings[base+1]->SetLine(
                Center + FVector(SmallRadius * std::cos(a0), 0.0f, SmallRadius * std::sin(a0)),
                Center + FVector(SmallRadius * std::cos(a1), 0.0f, SmallRadius * std::sin(a1)));
            BL.Rings[base+2]->SetLine(
                Center + FVector(0.0f, SmallRadius * std::cos(a0), SmallRadius * std::sin(a0)),
                Center + FVector(0.0f, SmallRadius * std::cos(a1), SmallRadius * std::sin(a1)));
        }
    }
}
