#pragma once

#include "SceneComponent.h"
#include "Material.h"
#include "Source/Runtime/Engine/PhysicsEngine/BodyInstance.h"
#include "UPrimitiveComponent.generated.h"

// 전방 선언
struct FSceneCompData;

class URenderer;
struct FMeshBatchElement;
class FSceneView;

struct FOverlapInfo
{
    AActor* OtherActor = nullptr;
    UPrimitiveComponent* Other = nullptr;
};

// @todo 델리게이트 매크로가 type alias 선언이 아닌 변수 선언을 하고 있어서, 직접 type alias 선언
DECLARE_DELEGATE_TYPE(FComponentHitSignature, UPrimitiveComponent*, AActor*, UPrimitiveComponent*, FVector, const FHitResult&)
DECLARE_DELEGATE_TYPE(FComponentBeginOverlapSignature, UPrimitiveComponent*, AActor*, UPrimitiveComponent*, int32)
DECLARE_DELEGATE_TYPE(FComponentEndOverlapSignature, UPrimitiveComponent*, AActor*, UPrimitiveComponent*, int32)
DECLARE_DELEGATE_TYPE(FComponentWakeSignature, UPrimitiveComponent*, FName)
DECLARE_DELEGATE_TYPE(FComponentSleepSignature, UPrimitiveComponent*, FName)

UCLASS(DisplayName="프리미티브 컴포넌트", Description="렌더링 가능한 기본 컴포넌트입니다")
class UPrimitiveComponent :public USceneComponent
{
public:
    GENERATED_REFLECTION_BODY();
    
public:
    // ===== Lua-Bindable Properties (Auto-moved from protected/private) =====

    UBodySetup* BoxBodySetup;

    UPROPERTY(EditAnywhere, Category="Shape")
    bool bGenerateOverlapEvents;

    UPROPERTY(EditAnywhere, Category="Shape")
    bool bBlockComponent;

    UPROPERTY(EditAnywhere, Category="Physics")
    bool bSimulatePhysics;

    UPROPERTY(EditAnywhere, Category="Physics")
    UPhysicalMaterial* PhysicalMaterial;

    FComponentHitSignature OnComponentHit;
    FComponentBeginOverlapSignature OnComponentBeginOverlap;
    FComponentEndOverlapSignature OnComponentEndOverlap;
    FComponentWakeSignature OnComponentWake;
    FComponentSleepSignature OnComponentSleep;

    UPrimitiveComponent();
    virtual ~UPrimitiveComponent();

    void OnPropertyChanged(const FProperty& Prop) override;
    void OnRegister(UWorld* InWorld) override;
    void OnUnregister() override;

    void OnUpdateTransform(EUpdateTransformFlags UpdateTransformFlags, ETeleportType Teleport = ETeleportType::None) override;

    virtual FAABB GetWorldAABB() const { return FAABB(); }

    // 이 프리미티브를 렌더링하는 데 필요한 FMeshBatchElement를 수집합니다.
    virtual void CollectMeshBatches(TArray<FMeshBatchElement>& OutMeshBatchElements, const FSceneView* View) {}

    virtual UMaterialInterface* GetMaterial(uint32 InElementIndex) const
    {
        // 기본 구현: UPrimitiveComponent 자체는 머티리얼을 소유하지 않으므로 nullptr 반환
        return nullptr;
    }
    virtual void SetMaterial(uint32 InElementIndex, UMaterialInterface* InNewMaterial)
    {
        // 기본 구현: 아무것도 하지 않음 (머티리얼을 지원하지 않거나 설정 불가)
    }

    // 내부적으로 ResourceManager를 통해 UMaterial*를 찾아 SetMaterial을 호출합니다.
    void SetMaterialByName(uint32 InElementIndex, const FString& MaterialName);

    void SetCulled(bool InCulled)
    {
        bIsCulled = InCulled;
    }

    bool GetCulled() const
    {
        return bIsCulled;
    }
    
    // ───── 물리 관련 ────────────────────────────
    virtual void OnCreatePhysicsState();
    virtual void OnDestroyPhysicsState();

    virtual UBodySetup* GetBodySetup() { return BoxBodySetup; }

    virtual FBodyInstance* GetBodyInstance(FName BoneName = FName()) { return &BodyInstance; }

    virtual void SetSimulatePhysics(bool bInSimulatePhysics);

    // ───── 충돌 관련 ──────────────────────────── 
    bool IsOverlappingActor(const AActor* Other) const;
    virtual const TArray<FOverlapInfo>& GetOverlapInfos() const { static TArray<FOverlapInfo> Empty; return Empty; }

    void DispatchBlockingHit(AActor* OtherActor, UPrimitiveComponent* OtherComp, const FVector& NormalImpulse, const FHitResult& Hit);
    
    void DispatchWakeEvents(bool bWake, FName BoneName);

    void DispatchBeginOverlap(AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);
    
    void DispatchEndOverlap(AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);
    
    // ───── 복사 관련 ────────────────────────────
    void DuplicateSubObjects() override;

    // Overlap event generation toggle API
    void SetGenerateOverlapEvents(bool bEnable) { bGenerateOverlapEvents = bEnable; }
    bool GetGenerateOverlapEvents() const { return bGenerateOverlapEvents; }

    // ───── 직렬화 ────────────────────────────
    void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;

protected:
    bool bIsCulled = false;

    // ───── 충돌 관련 ────────────────────────────
    FBodyInstance BodyInstance;

private:
    // ───── 디버그용 ────────────────────────────
    void OnHitDebug(UPrimitiveComponent* Comp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
    {
        UE_LOG("Hit!");        
    }
};
