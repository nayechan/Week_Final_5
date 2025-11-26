#pragma once

#include "Actor.h"
#include "ARibbonActor.generated.h"

UCLASS(DisplayName = "리본 액터", Description = "리본 액터입니다")
class ARibbonActor : public AActor
{
	GENERATED_REFLECTION_BODY()

public:
	ARibbonActor();

protected:
	~ARibbonActor() override;

	virtual void Tick(float DeltaSeconds) override;

	void SetRibbonPath(const TArray<FVector>& InPoints, float InSpeed);

private:
	float TestTime = 0.0f;

	TArray<FVector> RibbonPoints;  // 리본 경로 포인트들
	int32 CurrentPointIndex;        // 현재 목표 포인트 인덱스
	float MoveSpeed;                // 이동 속도 (units/sec)
	bool bIsMoving;                 // 이동 중인지 여부
};