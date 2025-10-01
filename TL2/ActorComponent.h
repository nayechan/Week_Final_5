#pragma once
#include "Object.h"

class AActor;
class UWorld;

enum class EEndPlayReason : uint8
{
    Destroyed,
    LevelTransition,
    EndPlayInEditor,
    RemovedFromWorld,
    Quit
};

class UActorComponent : public UObject
{
public:
    DECLARE_CLASS(UActorComponent, UObject)
    UActorComponent();

protected:
    ~UActorComponent() override;

public:
    // ─────────────── Lifecycle (게임 수명)
    virtual void InitializeComponent();                // BeginPlay 전에 1회
    virtual void BeginPlay();                          // 월드 시작 시 1회
    virtual void TickComponent(float DeltaTime);       // 매 프레임
    virtual void EndPlay(EEndPlayReason Reason);       // 파괴/월드 제거 시

    // ─────────────── Registration (월드/씬 등록 수명)
    // 액터/월드가 소유할 때 호출되는 등록/해제 포인트
    void RegisterComponent();                          // 외부에서 호출 (AActor)
    void UnregisterComponent();                        // 외부에서 호출 (AActor)
    virtual void OnRegister();                         // 내부 훅 (오버라이드 지점)
    virtual void OnUnregister();                       // 내부 훅 (오버라이드 지점)
    void DestroyComponent();                           // 소멸(EndPlay 포함)

    // ─────────────── 활성화/틱
    void SetActive(bool bNewActive) { bIsActive = bNewActive; }
    bool IsActive() const { return bIsActive; }

    void SetTickEnabled(bool bEnabled) { bTickEnabled = bEnabled; }
    bool IsTickEnabled() const { return bTickEnabled; }

    void SetCanEverTick(bool b) { bCanEverTick = b; }
    bool CanEverTick() const { return bCanEverTick; }

    bool IsComponentTickEnabled() const
    {
        // 틱을 진짜 돌릴지 최종 판단(액터 Tick에서 이걸로 거른다)
        return bIsActive && bCanEverTick && bTickEnabled && bRegistered;
    }

    // ─────────────── Owner/World
    void   SetOwner(AActor* InOwner) { Owner = InOwner; }
    AActor* GetOwner() const { return Owner; }
    UWorld* GetWorld() const; // 구현은 .cpp에서 Owner->GetWorld()

    // 상태 쿼리
    bool IsRegistered()    const { return bRegistered; }
    bool HasBegunPlay()    const { return bHasBegunPlay; }
    bool IsPendingDestroy()const { return bPendingDestroy; }

    // ───── 복사 관련 ────────────────────────────
    void DuplicateSubObjects() override;
    DECLARE_DUPLICATE(UActorComponent)

protected:
    AActor* Owner = nullptr;     // 소유 액터
    bool bIsActive = true;       // 활성 상태(사용자 on/off)
    bool bCanEverTick = false;   // 컴포넌트 설계상 틱 지원 여부
    bool bTickEnabled = false;   // 현재 틱 켜짐 여부

    bool bRegistered = false;    // RegisterComponent가 호출됐는가
    bool bHasBegunPlay = false;  // BeginPlay가 호출됐는가
    bool bPendingDestroy = false;// DestroyComponent 의도 플래그
};