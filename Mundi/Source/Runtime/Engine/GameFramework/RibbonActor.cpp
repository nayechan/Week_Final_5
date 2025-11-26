#include "pch.h"
#include "RibbonActor.h"

ARibbonActor::ARibbonActor()
{
    bTickInEditor = true;
}

ARibbonActor::~ARibbonActor()
{
}

void ARibbonActor::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    TestTime += DeltaSeconds;

    float Radius = 10.0f;   // 반지름
    float Speed = 1.0f;      // 회전 속도
    float Height = 6.0f;    // 상하 진폭

    FVector NewPosition(
        cos(TestTime * Speed) * Radius,
        sin(TestTime * Speed) * Radius,
        sin(TestTime * Speed * 2.0f) * Height
    );

    SetActorLocation(NewPosition);
}