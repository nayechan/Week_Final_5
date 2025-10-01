#include "pch.h"
#include "ActorComponent.h"
#include "Actor.h"
#include "World.h"

UActorComponent::UActorComponent()
    : Owner(nullptr)
    , bIsActive(true)
    , bCanEverTick(false)
    , bTickEnabled(false)
    , bRegistered(false)
    , bHasBegunPlay(false)
    , bPendingDestroy(false)
{
}

UActorComponent::~UActorComponent()
{
    // 안전장치: 등록 상태면 해제
    if (bRegistered) UnregisterComponent();
}

UWorld* UActorComponent::GetWorld() const
{
    return Owner ? Owner->GetWorld() : nullptr;
}

// ─────────────── Registration

void UActorComponent::RegisterComponent()
{
    if (bRegistered) return;
    bRegistered = true;
    OnRegister();

    // 여기서는 게임 수명 훅을 직접 부르지 않음.
    // BeginPlay/InitializeComponent는 보통 Actor/World 타이밍에서 호출.
    // 다만 에디터 유틸 컴포넌트라면 필요에 따라 여기서 InitializeComponent를 호출해도 됨.
}

void UActorComponent::UnregisterComponent()
{
    if (!bRegistered) return;

    // 게임 수명 종료가 필요하면 EndPlay 보장
    if (bHasBegunPlay)
    {
        EndPlay(EEndPlayReason::RemovedFromWorld);
        bHasBegunPlay = false;
    }

    OnUnregister();
    bRegistered = false;
}

void UActorComponent::OnRegister()
{
    // 리소스/핸들 생성, 메시/버퍼 생성 등(프레임 비의존)
}

void UActorComponent::OnUnregister()
{
    // 리소스/핸들 반납
}

void UActorComponent::DestroyComponent()
{
    if (bPendingDestroy) return;
    bPendingDestroy = true;

    // 등록 중이면 우선 해제(EndPlay 포함)
    if (bRegistered) UnregisterComponent();

    // Owner 참조 끊기
    Owner = nullptr;
}

// ─────────────── Lifecycle (게임 수명)

void UActorComponent::InitializeComponent()
{
    // BeginPlay 이전 초기화(게임 수명 관련 초기화)
}

void UActorComponent::BeginPlay()
{
    // 월드 진입 시 1회
    bHasBegunPlay = true;
}

void UActorComponent::TickComponent(float DeltaTime)
{
    if (!IsComponentTickEnabled()) return;
    // 매 프레임 처리
}

void UActorComponent::EndPlay(EEndPlayReason Reason)
{
    // 파괴 시
    // 필요하다면 Override
        // 게임 수명 종료
    bHasBegunPlay = false;
}

void UActorComponent::DuplicateSubObjects()
{
    Super::DuplicateSubObjects();

    bCanEverTick = true; // 매 프레임 Tick 가능 여부
    Owner = nullptr; // Actor에서 이거 설정해 줌
}