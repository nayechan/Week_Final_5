#pragma once

#include "Widget.h"

class UWorld;

/**
 * @class UActorSpawnWidget
 * @brief 월드에 다양한 종류의 액터를 스폰하기 위한 UI 위젯입니다.
 * UPrimitiveSpawnWidget의 기능을 참고하여 랜덤 트랜스폼과 다중 스폰을 지원합니다.
 */
class UActorSpawnWidget : public UWidget
{
public:
	DECLARE_CLASS(UActorSpawnWidget, UWidget)

	// UWidget 인터페이스
	void Initialize() override;
	void Update() override;
	void RenderWidget() override;
	void PostProcess() override;

	// 생성자 및 소멸자
	UActorSpawnWidget();
	~UActorSpawnWidget() override;

	void HandleActorSelection(AActor* Actor);

private:
	/**
	 * @brief 지정된 클래스의 액터 하나를 월드에 스폰합니다.
	 * @param ActorClass 스폰할 액터의 UClass
	 * @return 스폰 성공 여부
	 */
	bool TrySpawnActor(UClass* ActorClass);

	/**
	 * @brief 현재 설정된 범위 내에서 랜덤 위치를 생성합니다.
	 * @return FVector - 랜덤 위치 벡터
	 */
	FVector GenerateRandomLocation() const;

	// 더 이상 UI에서 직접 제어하지 않지만, 내부 로직에서 사용할 수 있는 변수들
	// 필요 없다면 이 변수들을 제거하고 TrySpawnActor에서 고정값을 사용해도 됩니다.
	float SpawnRangeMin = -2.0f;
	float SpawnRangeMax = 2.0f;

	// UIManager 참조 캐시
	UUIManager* UIManager = nullptr;
};