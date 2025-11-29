#include "pch.h"
#include "PhysicsAsset.h"

// --- 생성자/소멸자 ---

UPhysicsAsset::UPhysicsAsset()
    : DefaultPhysMaterial(nullptr)
    , bEnablePhysicsSimulation(true)
{
}

UPhysicsAsset::~UPhysicsAsset()
{
    ClearAllBodySetups();
    ClearAllConstraints();
}

// --- BodySetup 관리 ---

USkeletalBodySetup* UPhysicsAsset::FindBodySetup(const FName& BoneName) const
{
    // 캐시된 맵이 있으면 사용
    if (BodySetupIndexMap.Num() > 0)
    {
        const int32* IndexPtr = BodySetupIndexMap.Find(BoneName);
        if (IndexPtr)
        {
            return GetBodySetup(*IndexPtr);
        }
        return nullptr;
    }

    // 선형 검색
    for (int32 i = 0; i < SkeletalBodySetups.Num(); ++i)
    {
        USkeletalBodySetup* Setup = SkeletalBodySetups[i];
        if (Setup && Setup->BoneName == BoneName)
        {
            return Setup;
        }
    }
    return nullptr;
}

USkeletalBodySetup* UPhysicsAsset::GetBodySetup(int32 BodyIndex) const
{
    if (BodyIndex >= 0 && BodyIndex < SkeletalBodySetups.Num())
    {
        return SkeletalBodySetups[BodyIndex];
    }
    return nullptr;
}

int32 UPhysicsAsset::AddBodySetup(USkeletalBodySetup* InBodySetup)
{
    if (!InBodySetup) return -1;

    // 중복 체크
    int32 ExistingIndex = FindBodySetupIndex(InBodySetup->BoneName);
    if (ExistingIndex != -1)
    {
        // 이미 존재하면 교체
        SkeletalBodySetups[ExistingIndex] = InBodySetup;
        return ExistingIndex;
    }

    int32 NewIndex = SkeletalBodySetups.Add(InBodySetup);

    // 맵 갱신
    BodySetupIndexMap.Add(InBodySetup->BoneName, NewIndex);

    return NewIndex;
}

int32 UPhysicsAsset::FindBodySetupIndex(const FName& BoneName) const
{
    // 캐시된 맵이 있으면 사용
    if (BodySetupIndexMap.Num() > 0)
    {
        const int32* IndexPtr = BodySetupIndexMap.Find(BoneName);
        if (IndexPtr)
        {
            return *IndexPtr;
        }
        return -1;
    }

    // 선형 검색
    for (int32 i = 0; i < SkeletalBodySetups.Num(); ++i)
    {
        USkeletalBodySetup* Setup = SkeletalBodySetups[i];
        if (Setup && Setup->BoneName == BoneName)
        {
            return i;
        }
    }
    return -1;
}

void UPhysicsAsset::RemoveBodySetup(int32 Index)
{
    if (Index >= 0 && Index < SkeletalBodySetups.Num())
    {
        SkeletalBodySetups.RemoveAt(Index);
        UpdateBodySetupIndexMap();
    }
}

void UPhysicsAsset::RemoveBodySetup(const FName& BoneName)
{
    int32 Index = FindBodySetupIndex(BoneName);
    if (Index != -1)
    {
        RemoveBodySetup(Index);
    }
}

void UPhysicsAsset::ClearAllBodySetups()
{
    SkeletalBodySetups.Empty();
    BodySetupIndexMap.Empty();
}

// --- Constraint 관리 ---

int32 UPhysicsAsset::AddConstraint(UPhysicsConstraintTemplate* InConstraint)
{
    if (!InConstraint) return -1;

    return ConstraintSetup.Add(InConstraint);
}

UPhysicsConstraintTemplate* UPhysicsAsset::FindConstraint(const FName& JointName) const
{
    int32 Index = FindConstraintIndex(JointName);
    if (Index != -1)
    {
        return ConstraintSetup[Index];
    }
    return nullptr;
}

int32 UPhysicsAsset::FindConstraintIndex(const FName& JointName) const
{
    for (int32 i = 0; i < ConstraintSetup.Num(); ++i)
    {
        UPhysicsConstraintTemplate* Constraint = ConstraintSetup[i];
        if (Constraint && Constraint->GetJointName() == JointName)
        {
            return i;
        }
    }
    return -1;
}

int32 UPhysicsAsset::FindConstraintIndex(const FName& Bone1Name, const FName& Bone2Name) const
{
    for (int32 i = 0; i < ConstraintSetup.Num(); ++i)
    {
        UPhysicsConstraintTemplate* Constraint = ConstraintSetup[i];
        if (Constraint)
        {
            // 양방향 체크
            if ((Constraint->GetBone1Name() == Bone1Name && Constraint->GetBone2Name() == Bone2Name) ||
                (Constraint->GetBone1Name() == Bone2Name && Constraint->GetBone2Name() == Bone1Name))
            {
                return i;
            }
        }
    }
    return -1;
}

void UPhysicsAsset::RemoveConstraint(int32 Index)
{
    if (Index >= 0 && Index < ConstraintSetup.Num())
    {
        ConstraintSetup.RemoveAt(Index);
    }
}

void UPhysicsAsset::ClearAllConstraints()
{
    ConstraintSetup.Empty();
}

// --- 충돌 관리 ---

void UPhysicsAsset::DisableCollision(int32 BodyIndexA, int32 BodyIndexB)
{
    if (BodyIndexA == BodyIndexB) return;

    FRigidBodyIndexPair Pair(BodyIndexA, BodyIndexB);
    CollisionDisableTable[Pair] = true;
}

void UPhysicsAsset::EnableCollision(int32 BodyIndexA, int32 BodyIndexB)
{
    if (BodyIndexA == BodyIndexB) return;

    FRigidBodyIndexPair Pair(BodyIndexA, BodyIndexB);
    CollisionDisableTable.erase(Pair);
}

bool UPhysicsAsset::IsCollisionEnabled(int32 BodyIndexA, int32 BodyIndexB) const
{
    if (BodyIndexA == BodyIndexB) return false;

    FRigidBodyIndexPair Pair(BodyIndexA, BodyIndexB);
    auto It = CollisionDisableTable.find(Pair);
    if (It != CollisionDisableTable.end())
    {
        return !It->second; // 테이블에 있으면 비활성화된 것
    }
    return true; // 기본적으로 충돌 활성화
}

// --- 캐시 관리 ---

void UPhysicsAsset::UpdateBodySetupIndexMap()
{
    BodySetupIndexMap.Empty();

    for (int32 i = 0; i < SkeletalBodySetups.Num(); ++i)
    {
        USkeletalBodySetup* Setup = SkeletalBodySetups[i];
        if (Setup)
        {
            BodySetupIndexMap.Add(Setup->BoneName, i);
        }
    }
}

// --- 유틸리티 ---

int32 UPhysicsAsset::GetTotalShapeCount() const
{
    int32 TotalCount = 0;
    for (int32 i = 0; i < SkeletalBodySetups.Num(); ++i)
    {
        USkeletalBodySetup* Setup = SkeletalBodySetups[i];
        if (Setup)
        {
            TotalCount += Setup->GetShapeCount();
        }
    }
    return TotalCount;
}

// --- 직렬화 ---

void UPhysicsAsset::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
    Super::Serialize(bInIsLoading, InOutHandle);

    // TODO: SkeletalBodySetups 배열 직렬화
    // TODO: ConstraintSetup 배열 직렬화
    // TODO: CollisionDisableTable 직렬화
    // TODO: DefaultPhysMaterial 참조 직렬화
}
