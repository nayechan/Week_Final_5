#include "pch.h"
#include "PhysicsAsset.h"
#include "JsonSerializer.h"

// --- 생성자/소멸자 ---

UPhysicsAsset::UPhysicsAsset()
    : DefaultPhysMaterial(nullptr)
    //, bEnablePhysicsSimulation(true)
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

    if (bInIsLoading)  // 로드
    {
        // BodySetups 로드
        JSON BodySetupsJson;
        if (FJsonSerializer::ReadArray(InOutHandle, "BodySetups", BodySetupsJson))
        {
            SkeletalBodySetups.Empty();
            for (size_t i = 0; i < BodySetupsJson.size(); ++i)
            {
                JSON& BodyJson = BodySetupsJson.at(i);
                USkeletalBodySetup* BodySetup = NewObject<USkeletalBodySetup>();
                BodySetup->Serialize(true, BodyJson);
                AddBodySetup(BodySetup);
            }
        }

        // Constraints 로드
        JSON ConstraintsJson;
        if (FJsonSerializer::ReadArray(InOutHandle, "Constraints", ConstraintsJson))
        {
            ConstraintSetup.Empty();
            for (size_t i = 0; i < ConstraintsJson.size(); ++i)
            {
                JSON& ConstraintJson = ConstraintsJson.at(i);
                UPhysicsConstraintTemplate* Constraint = NewObject<UPhysicsConstraintTemplate>();
                Constraint->Serialize(true, ConstraintJson);
                AddConstraint(Constraint);
            }
        }

        // CollisionDisableTable 로드
        JSON CollisionJson;
        if (FJsonSerializer::ReadArray(InOutHandle, "CollisionDisableTable", CollisionJson))
        {
            CollisionDisableTable.clear();
            for (size_t i = 0; i < CollisionJson.size(); ++i)
            {
                JSON& Entry = CollisionJson.at(i);
                int32 Idx1 = static_cast<int32>(Entry["Index1"].ToInt());
                int32 Idx2 = static_cast<int32>(Entry["Index2"].ToInt());
                DisableCollision(Idx1, Idx2);
            }
        }
    }
    else  // 저장
    {
        // BodySetups 저장
        JSON BodySetupsJson = JSON::Make(JSON::Class::Array);
        for (USkeletalBodySetup* BodySetup : SkeletalBodySetups)
        {
            if (BodySetup)
            {
                JSON BodyJson = JSON::Make(JSON::Class::Object);
                BodySetup->Serialize(false, BodyJson);
                BodySetupsJson.append(BodyJson);
            }
        }
        InOutHandle["BodySetups"] = BodySetupsJson;

        // Constraints 저장
        JSON ConstraintsJson = JSON::Make(JSON::Class::Array);
        for (UPhysicsConstraintTemplate* Constraint : ConstraintSetup)
        {
            if (Constraint)
            {
                JSON ConstraintJson = JSON::Make(JSON::Class::Object);
                Constraint->Serialize(false, ConstraintJson);
                ConstraintsJson.append(ConstraintJson);
            }
        }
        InOutHandle["Constraints"] = ConstraintsJson;

        // CollisionDisableTable 저장
        JSON CollisionJson = JSON::Make(JSON::Class::Array);
        for (const auto& Pair : CollisionDisableTable)
        {
            JSON Entry = JSON::Make(JSON::Class::Object);
            Entry["Index1"] = Pair.first.BodyIndex1;
            Entry["Index2"] = Pair.first.BodyIndex2;
            CollisionJson.append(Entry);
        }
        InOutHandle["CollisionDisableTable"] = CollisionJson;
    }
}

// --- 리소스 로드/저장 ---

void UPhysicsAsset::Load(const FString& InFilePath, ID3D11Device* InDevice)
{
    // FString을 FWideString으로 변환
    FWideString WidePath(InFilePath.begin(), InFilePath.end());

    // 파일에서 JSON 로드
    JSON JsonHandle;
    if (!FJsonSerializer::LoadJsonFromFile(JsonHandle, WidePath))
    {
        UE_LOG("[UPhysicsAsset] Load 실패: %s", InFilePath.c_str());
        return;
    }

    // 역직렬화
    Serialize(true, JsonHandle);
    SetFilePath(InFilePath);

    UE_LOG("[UPhysicsAsset] Load 성공: %s (%d bodies, %d constraints)",
        InFilePath.c_str(), GetBodySetupCount(), GetConstraintCount());
}

void UPhysicsAsset::Save(const FString& InFilePath)
{
    // FString을 FWideString으로 변환
    FWideString WidePath(InFilePath.begin(), InFilePath.end());

    JSON JsonData = JSON::Make(JSON::Class::Object);
    Serialize(false, JsonData);

    // 파일에 JSON 저장
    if (!FJsonSerializer::SaveJsonToFile(JsonData, WidePath))
    {
        UE_LOG("[UPhysicsAsset] Save 실패: %s", InFilePath.c_str());
        return;
    }

    SetFilePath(InFilePath);

    UE_LOG("[UPhysicsAsset] Saved: %s", InFilePath.c_str());
}
