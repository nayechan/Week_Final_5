#include "pch.h"
#include "PhysicsAsset.h"
#include "JsonSerializer.h"

IMPLEMENT_CLASS(UPhysicsAsset)

// ────────────────────────────────────────────────────────────────
// Body 관리 메서드
// ────────────────────────────────────────────────────────────────

int32 UPhysicsAsset::AddBody(const FName& BoneName, int32 BoneIndex)
{
	// 이미 존재하는지 확인
	int32 ExistingIndex = FindBodyIndex(BoneName);
	if (ExistingIndex != -1)
	{
		return ExistingIndex;
	}

	// 새 바디 생성
	FBodySetup NewBody(BoneName, BoneIndex);
	int32 NewIndex = static_cast<int32>(BodySetups.size());
	BodySetups.Add(NewBody);

	// 캐시 갱신
	BodySetupIndexMap.Add(BoneName, NewIndex);

	return NewIndex;
}

bool UPhysicsAsset::RemoveBody(int32 BodyIndex)
{
	if (BodyIndex < 0 || BodyIndex >= static_cast<int32>(BodySetups.size()))
	{
		return false;
	}

	// 연결된 제약 조건도 제거
	for (int32 i = static_cast<int32>(ConstraintSetups.size()) - 1; i >= 0; --i)
	{
		FConstraintSetup& Constraint = ConstraintSetups[i];
		if (Constraint.ParentBodyIndex == BodyIndex || Constraint.ChildBodyIndex == BodyIndex)
		{
			ConstraintSetups.RemoveAt(i);
		}
		else
		{
			// 인덱스 조정
			if (Constraint.ParentBodyIndex > BodyIndex)
			{
				Constraint.ParentBodyIndex--;
			}
			if (Constraint.ChildBodyIndex > BodyIndex)
			{
				Constraint.ChildBodyIndex--;
			}
		}
	}

	// 바디 제거
	BodySetups.RemoveAt(BodyIndex);

	// 캐시 재구성
	RebuildIndexMap();

	return true;
}

int32 UPhysicsAsset::FindBodyIndex(const FName& BoneName) const
{
	const int32* Found = BodySetupIndexMap.Find(BoneName);
	return Found ? *Found : -1;
}

int32 UPhysicsAsset::FindBodyIndexByBone(int32 BoneIndex) const
{
	for (int32 i = 0; i < static_cast<int32>(BodySetups.size()); ++i)
	{
		if (BodySetups[i].BoneIndex == BoneIndex)
		{
			return i;
		}
	}
	return -1;
}

// ────────────────────────────────────────────────────────────────
// Constraint 관리 메서드
// ────────────────────────────────────────────────────────────────

int32 UPhysicsAsset::AddConstraint(const FName& JointName, int32 ParentBodyIndex, int32 ChildBodyIndex)
{
	// 유효성 검사
	if (ParentBodyIndex < 0 || ParentBodyIndex >= static_cast<int32>(BodySetups.size()) ||
		ChildBodyIndex < 0 || ChildBodyIndex >= static_cast<int32>(BodySetups.size()))
	{
		return -1;
	}

	// 이미 존재하는지 확인
	int32 ExistingIndex = FindConstraintIndex(ParentBodyIndex, ChildBodyIndex);
	if (ExistingIndex != -1)
	{
		return ExistingIndex;
	}

	// 새 제약 조건 생성
	FConstraintSetup NewConstraint(JointName, ParentBodyIndex, ChildBodyIndex);
	int32 NewIndex = static_cast<int32>(ConstraintSetups.size());
	ConstraintSetups.Add(NewConstraint);

	return NewIndex;
}

bool UPhysicsAsset::RemoveConstraint(int32 ConstraintIndex)
{
	if (ConstraintIndex < 0 || ConstraintIndex >= static_cast<int32>(ConstraintSetups.size()))
	{
		return false;
	}

	ConstraintSetups.RemoveAt(ConstraintIndex);
	return true;
}

int32 UPhysicsAsset::FindConstraintIndex(int32 ParentBodyIndex, int32 ChildBodyIndex) const
{
	for (int32 i = 0; i < static_cast<int32>(ConstraintSetups.size()); ++i)
	{
		const FConstraintSetup& Constraint = ConstraintSetups[i];
		if (Constraint.ParentBodyIndex == ParentBodyIndex && Constraint.ChildBodyIndex == ChildBodyIndex)
		{
			return i;
		}
	}
	return -1;
}

// ────────────────────────────────────────────────────────────────
// 유틸리티
// ────────────────────────────────────────────────────────────────

void UPhysicsAsset::ClearBodySelection()
{
	for (FBodySetup& Body : BodySetups)
	{
		Body.bSelected = false;
	}
}

void UPhysicsAsset::ClearConstraintSelection()
{
	for (FConstraintSetup& Constraint : ConstraintSetups)
	{
		Constraint.bSelected = false;
	}
}

bool UPhysicsAsset::IsValid() const
{
	// 최소한 하나의 바디가 있어야 함
	return !BodySetups.IsEmpty();
}

void UPhysicsAsset::RebuildIndexMap()
{
	BodySetupIndexMap.Empty();
	for (int32 i = 0; i < static_cast<int32>(BodySetups.size()); ++i)
	{
		BodySetupIndexMap.Add(BodySetups[i].BoneName, i);
	}
}

// ────────────────────────────────────────────────────────────────
// 직렬화
// ────────────────────────────────────────────────────────────────

void UPhysicsAsset::Load(const FString& InFilePath, ID3D11Device* InDevice)
{
	FWideString WidePath(InFilePath.begin(), InFilePath.end());

	JSON JsonHandle;
	if (!FJsonSerializer::LoadJsonFromFile(JsonHandle, WidePath))
	{
		UE_LOG("[UPhysicsAsset] Load 실패: %s", InFilePath.c_str());
		return;
	}

	Serialize(true, JsonHandle);
	UE_LOG("[UPhysicsAsset] Load 성공: %s", InFilePath.c_str());
}

void UPhysicsAsset::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	// UPROPERTY 자동 직렬화 (SkeletalMeshPath, BodySetups, ConstraintSetups)
	UObject::Serialize(bInIsLoading, InOutHandle);

	// 로드 후 인덱스 캐시 재구성
	if (bInIsLoading)
	{
		RebuildIndexMap();
	}
}

void UPhysicsAsset::DuplicateSubObjects()
{
	UObject::DuplicateSubObjects();

	// FBodySetup과 FConstraintSetup은 값 타입이므로
	// TArray 복사 시 자동으로 복제됨
	// 추가 처리 불필요
}
