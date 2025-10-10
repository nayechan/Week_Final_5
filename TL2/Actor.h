#pragma once
#include "Object.h"
#include "Vector.h"
#include "AABoundingBoxComponent.h"
class UWorld;
class USceneComponent;
class UAABoundingBoxComponent;
class UShapeComponent;
class UTextRenderComponent;
class UBillboardComponent;

class AActor : public UObject
{
public:
    DECLARE_CLASS(AActor, UObject)
    AActor(); 

protected:
    ~AActor() override;

public:
    // 수명
    virtual void BeginPlay();
    virtual void Tick(float DeltaSeconds);
    virtual void EndPlay(EEndPlayReason Reason);
    virtual void Destroy();

    // 이름
    void SetName(const FString& InName) { Name = InName; }
    const FName& GetName() { return Name; }

    // 월드/표시
    void SetWorld(UWorld* InWorld) { World = InWorld; }
    UWorld* GetWorld() const { return World; }

    // 루트/컴포넌트
    void SetRootComponent(USceneComponent* InRoot);
    USceneComponent* GetRootComponent() const { return RootComponent; }

    // 소유 컴포넌트(모든 타입)
    void AddOwnedComponent(UActorComponent* Component);
    void RemoveOwnedComponent(UActorComponent* Component);

    // 씬 컴포넌트(트리/렌더용)
    const TArray<USceneComponent*>& GetSceneComponents() const { return SceneComponents; }
    
    // 컴포넌트 생성 (템플릿)
    template<typename T>
    T* CreateDefaultSubobject(const FName& SubobjectName)
    {
        T* Comp = ObjectFactory::NewObject<T>();
        Comp->SetOwner(this);
        // Comp->SetName(SubobjectName);  //나중에 추가 구현
        AddOwnedComponent(Comp); // 새 모델로 합류
        return Comp;
    }

    // ===== 월드가 파괴 경로에서 호출할 "좁은 공개 API" =====
    void UnregisterAllComponents(bool bCallEndPlayOnBegun = true);
    void DestroyAllComponents();   // Unregister 이후 최종 파괴
    void ClearSceneComponentCaches(); // SceneComponents, Root 등 정리

    // ===== 파괴 재진입 가드 =====
    bool IsPendingDestroy() const { return bPendingDestroy; }
    void MarkPendingDestroy() { bPendingDestroy = true; }

    // ───────────────
    // Transform API
    // ───────────────
    void SetActorTransform(const FTransform& NewTransform);
    FTransform GetActorTransform() const;

    void SetActorLocation(const FVector& NewLocation);
    FVector GetActorLocation() const;

    void SetActorRotation(const FVector& EulerDegree);
    void SetActorRotation(const FQuat& InQuat);
    FQuat GetActorRotation() const;

    void SetActorScale(const FVector& NewScale);
    FVector GetActorScale() const;

    FMatrix GetWorldMatrix() const;

    FVector GetActorForward() const { return GetActorRotation().RotateVector(FVector(0, 1, 0)); }
    FVector GetActorRight()   const { return GetActorRotation().RotateVector(FVector(1, 0, 0)); }
    FVector GetActorUp()      const { return GetActorRotation().RotateVector(FVector(0, 0, 1)); }

    void AddActorWorldRotation(const FQuat& DeltaRotation);
    void AddActorWorldRotation(const FVector& DeltaEuler);
    void AddActorWorldLocation(const FVector& DeltaRot);
    void AddActorLocalRotation(const FVector& DeltaEuler);
    void AddActorLocalRotation(const FQuat& DeltaRotation);
    void AddActorLocalLocation(const FVector& DeltaRot);

    // 파티션
    void MarkPartitionDirty();

    // 틱 플래그
    void SetTickInEditor(bool b) { bTickInEditor = b; }
    bool GetTickInEditor() const { return bTickInEditor; }

    // 바운드 및 피킹
    virtual FAABB GetBounds() const { return FAABB(); }
    void SetIsPicked(bool picked) { bIsPicked = picked; }
    bool GetIsPicked() { return bIsPicked; }
    void SetCulled(bool InCulled) 
    { 
        bIsCulled = InCulled;
        if (SceneComponents.empty())
        {
            return;
        }
    }
    bool GetCulled() { return bIsCulled; }

    // 가시성
    void SetActorHiddenInGame(bool bNewHidden) { bHiddenInGame = bNewHidden; }
    bool GetActorHiddenInGame() const { return bHiddenInGame; }
    bool IsActorVisible() const { return !bHiddenInGame; }

    bool CanTickInEditor() const
    {
        return bTickInEditor;
    }

    // ───── 복사 관련 ────────────────────────────
    void DuplicateSubObjects() override;
    DECLARE_DUPLICATE(AActor)

public:
    FName Name;

    UWorld* World = nullptr;
    USceneComponent* RootComponent = nullptr;
    UAABoundingBoxComponent* CollisionComponent = nullptr;
    UTextRenderComponent* TextComp = nullptr;

protected:
    TSet<UActorComponent*> OwnedComponents;   // 모든 컴포넌트 (씬/비씬)
    TArray<USceneComponent*> SceneComponents; // 씬 컴포넌트들만 별도 캐시(트리/렌더/ImGui용)
    bool bTickInEditor = false; // 에디터에서도 틱 허용
    bool bHiddenInGame = false;
    bool bPendingDestroy = false;

    bool bIsPicked = false;
    bool bCanEverTick = true;
    bool bIsCulled = false;


private:
    // 내부 헬퍼
    void RegisterComponentTree(USceneComponent* SceneComp);
    void UnregisterComponentTree(USceneComponent* SceneComp);
};
