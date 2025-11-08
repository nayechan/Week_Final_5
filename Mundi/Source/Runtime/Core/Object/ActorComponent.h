#pragma once
#include "Object.h"
#include "UActorComponent.generated.h"

class AActor;
class UWorld;

class UActorComponent : public UObject
{
public:
    GENERATED_REFLECTION_BODY()
    UActorComponent();

protected:
    ~UActorComponent() override;

public:
    // ─────────────── Lifecycle (게임 수명)
    virtual void InitializeComponent();                // BeginPlay 전에 1회
    virtual void BeginPlay();                          // PIE 중에 월드에 등록 시 호출됨
    virtual void TickComponent(float DeltaTime);       // 매 프레임
    virtual void EndPlay();                            // PIE 중에 파괴/월드 제거 시

    // ─────────────── Registration (월드/씬 등록 수명)
    // 액터/월드가 소유할 때 호출되는 등록/해제 포인트
    void RegisterComponent(UWorld* InWorld);           // 외부에서 호출 (AActor)
    void UnregisterComponent();                        // 외부에서 호출 (AActor)
    virtual void OnRegister(UWorld* InWorld);          // 내부 훅 (오버라이드 지점)
    virtual void OnUnregister();                       // 내부 훅 (오버라이드 지점)
    void DestroyComponent();                           // 소멸

    // ─────────────── 활성화/틱
    void SetActive(bool bNewActive) { bIsActive = bNewActive; }
    bool IsActive() const { return bIsActive; }

    void SetTickEnabled(bool bEnabled) { bTickEnabled = bEnabled; }
    bool IsTickEnabled() const { return bTickEnabled; }

    void SetEditability(bool InEditable) { bIsEditable = InEditable; }
    bool IsEditable() const { return bIsEditable; }

    void SetHiddenInGame(bool bInHidden) { bHiddenInGame = bInHidden; }
    bool GetHiddenInGame() const { return bHiddenInGame; }

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

    // ─────────────── 컴포넌트 보호
    void SetNative(const bool bValue) { bIsNative = bValue; }
    bool IsNative() const {return bIsNative; }
    
    // 상태 쿼리
    bool IsRegistered()    const { return bRegistered; }
    void SetRegistered(bool bInRegistered) { bRegistered = bInRegistered; }
    bool IsPendingDestroy()const { return bPendingDestroy; }

    // ───── 복사 관련 ────────────────────────────
    void DuplicateSubObjects() override;
    void PostDuplicate() override;
    

    // ───── 직렬화 ────────────────────────────
    void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;

protected:
    AActor* Owner = nullptr;     // 소유 액터

    // 생성 시 고정 데이터
    bool bIsNative = false;      // 액터의 기본 구성 컴포넌트인지 여부. 활성화되면 보호되어 UI에서 삭제 불가 상태가 됨 
    bool bIsEditable = true;    //UI에서 Edit이 가능한가
    bool bCanEverTick = false;   // 컴포넌트 설계상 틱 지원 여부

    // 설정 가능한 데이터
    UPROPERTY(EditAnywhere, Category = "렌더링")
    bool bIsActive = true;       // 활성 상태(사용자 on/off), 물리 적용

    UPROPERTY(EditAnywhere, Category="렌더링")
    bool bHiddenInGame = false; // 게임에서 숨김 여부

    UPROPERTY(EditAnywhere, Category = "렌더링")
    bool bTickEnabled = true;   // 현재 틱 켜짐 여부

    // 저장되지 않는 실시간 상태 변수
    bool bRegistered = false;       // RegisterComponent가 호출됐는가
    bool bPendingDestroy = false;   // DestroyComponent 의도 플래그, NOTE: 현재 작동 안함
};