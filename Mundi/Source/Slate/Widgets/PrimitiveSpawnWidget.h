#pragma once
#include "Widget.h"
#include "Vector.h"

class UUIManager;
class UWorld;

class UPrimitiveSpawnWidget
	:public UWidget
{
public:
	DECLARE_CLASS(UPrimitiveSpawnWidget, UWidget)

	void Initialize() override;
	void Update() override;
	void RenderWidget() override;
	void SpawnActors() const;

	// Special Member Function
	UPrimitiveSpawnWidget();
	~UPrimitiveSpawnWidget() override;

private:
	UUIManager* UIManager = nullptr;
	
	// Spawn 설정
	int32 SelectedPrimitiveType = 0;
	int32 NumberOfSpawn = 1;
	float SpawnRangeMin = -2.0f;
	float SpawnRangeMax = 2.0f;
	
	// 추가 옵션
	bool bRandomRotation = true;
	bool bRandomScale = true;
	float MinScale = 0.5f;
	float MaxScale = 1.5f;

	// 메시 선택
	mutable int32 SelectedMeshIndex = -1;
	mutable TArray<FString> CachedMeshFilePaths;
	
	// 헬퍼 메서드
	UWorld* GetCurrentWorld() const;
	FVector GenerateRandomLocation() const;
	float GenerateRandomScale() const;
	FQuat GenerateRandomRotation() const;
	const char* GetPrimitiveTypeName(int32 TypeIndex) const;
};
