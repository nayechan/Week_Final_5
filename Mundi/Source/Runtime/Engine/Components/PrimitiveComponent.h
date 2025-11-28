#pragma once
#include "SceneComponent.h"
#include "Material.h"
#include "../Physics/BodyInstance.h"
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

UCLASS(DisplayName="프리미티브 컴포넌트", Description="렌더링 가능한 기본 컴포넌트입니다")
class UPrimitiveComponent :public USceneComponent
{
public:
    GENERATED_REFLECTION_BODY()
    
    UPrimitiveComponent();
    virtual ~UPrimitiveComponent() = default;

    void BeginPlay() override;
    void EndPlay() override;

    void OnRegister(UWorld* InWorld) override;
    void OnUnregister() override;

    virtual FAABB GetWorldAABB() const { return FAABB(); }

    // 이 프리미티브를 렌더링하는 데 필요한 FMeshBatchElement를 수집합니다.
    virtual void CollectMeshBatches(TArray<FMeshBatchElement>& OutMeshBatchElements, const FSceneView* View) {}

    virtual UMaterialInterface* GetMaterial(uint32 InElementIndex) const { return nullptr; }
    virtual void SetMaterial(uint32 InElementIndex, UMaterialInterface* InNewMaterial) {}

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
    bool IsOverlappingActor(const AActor* Other) const;
    virtual const TArray<FOverlapInfo>& GetOverlapInfos() const { static TArray<FOverlapInfo> Empty; return Empty; }
    
    UPROPERTY(EditAnywhere, Category="Shape")
    bool bGenerateOverlapEvents;

    UPROPERTY(EditAnywhere, Category="Shape")
    bool bBlockComponent;
    
    UPROPERTY(EditAnywhere, Category="Physics")
    bool bSimulatePhysics = false;
    UPROPERTY(EditAnywhere, Category="Physics")
    float Mass = 10.0f;

protected:
    void CreatePhysicsState();
    virtual physx::PxGeometry* GetPhysicsGeometry() { return nullptr; }
    FBodyInstance* BodyInstance = nullptr;

public:
    // ───── 복사 관련 ────────────────────────────
    void DuplicateSubObjects() override;

    // Overlap event generation toggle API
    void SetGenerateOverlapEvents(bool bEnable) { bGenerateOverlapEvents = bEnable; }
    bool GetGenerateOverlapEvents() const { return bGenerateOverlapEvents; }

    // ───── 직렬화 ────────────────────────────
    void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;

protected:
    bool bIsCulled = false;
};
