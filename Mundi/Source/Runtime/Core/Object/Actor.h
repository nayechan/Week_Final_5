#pragma once
#include "Object.h"
#include "Vector.h"
#include "ActorComponent.h"
#include "AABB.h"
#include "LightManager.h"
#include "Delegates.h"
#include "AActor.generated.h"

class UWorld;
class USceneComponent;
class UPrimitiveComponent;
class UShapeComponent;
class UTextRenderComponent;
class UBillboardComponent;
class FGameObject;

class AActor : public UObject
{
public:
    GENERATED_REFLECTION_BODY()

    DECLARE_DELEGATE(OnComponentBeginOverlap, UPrimitiveComponent*, UPrimitiveComponent*);
    DECLARE_DELEGATE(OnComponentEndOverlap, UPrimitiveComponent*, UPrimitiveComponent*);
    DECLARE_DELEGATE(OnComponentHit, UPrimitiveComponent*, UPrimitiveComponent*);

    AActor(); 

protected:
    ~AActor() override;

public:
    // 수명
    virtual void BeginPlay();   // Override 시 Super::BeginPlay() 권장
    virtual void Tick(float DeltaSeconds);   // Override 시 Super::Tick() 권장
    virtual void EndPlay();   // Override 시 Super::EndPlay() 권장
    virtual void Destroy();

    void SetTag(const FString& InTag) { Tag = InTag; }
    const FString& GetTag() const { return Tag; }

    // 월드/표시
    void SetWorld(UWorld* InWorld) { World = InWorld; }
    UWorld* GetWorld() const { return World; }

    // 루트/컴포넌트
    void SetRootComponent(USceneComponent* InRoot);
    USceneComponent* GetRootComponent() const { return RootComponent; }
   
    // 소유 컴포넌트(모든 타입)
    UActorComponent* AddNewComponent(UClass* ComponentClass, USceneComponent* ParentToAttach = nullptr);
    void AddOwnedComponent(UActorComponent* Component);
    void RemoveOwnedComponent(UActorComponent* Component);

    // 씬 컴포넌트(트리/렌더용)
    const TArray<USceneComponent*>& GetSceneComponents() const { return SceneComponents; }
    const TSet<UActorComponent*>& GetOwnedComponents() const { return OwnedComponents; }
    UActorComponent* GetComponent(UClass* ComponentClass);
    
    // 컴포넌트 생성 (템플릿)
    template<typename T>
    T* CreateDefaultSubobject(const FName& SubobjectName)
    {
        T* Comp = ObjectFactory::NewObject<T>();
        Comp->SetOwner(this);
        Comp->SetNative(true);
        // Comp->SetName(SubobjectName);  //나중에 추가 구현
        AddOwnedComponent(Comp); // 새 모델로 합류
        return Comp;
    }

    void RegisterAllComponents(UWorld* InWorld);
    void RegisterComponentTree(USceneComponent* SceneComp, UWorld* InWorld);
    void UnregisterComponentTree(USceneComponent* SceneComp);

    // ===== 월드가 파괴 경로에서 호출할 "좁은 공개 API" =====
    void DestroyAllComponents();   // Unregister 이후 최종 파괴

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
    
    void SetActorIsVisible(bool bIsActive);
    bool GetActorIsVisible();
    
    void SetActorActive(bool bIsActive) { bActorIsActive = bIsActive; };
    bool IsActorActive() { return bActorIsActive; };

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
    
    float GetCustomTimeDillation();
    void  SetCustomTimeDillation(float Duration, float Dillation);

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
    void SetActorHiddenInEditor(bool bNewHidden);
    bool GetActorHiddenInEditor() const { return bHiddenInEditor; }
    // Visible false인 경우 게임, 에디터 모두 안 보임
    void SetActorHiddenInGame(bool bNewHidden) { bActorHiddenInGame = bNewHidden; }
    bool GetActorHiddenInGame() { return bActorHiddenInGame; }
    bool IsActorVisible() const;

    bool CanEverTick() const { return bCanEverTick; }
	bool CanTickInEditor() const { return bTickInEditor; }
    // ───── 충돌 관련 ─────────────────────────  
    void OnBeginOverlap(UPrimitiveComponent* MyComp, UPrimitiveComponent* OtherComp);
    void OnEndOverlap(UPrimitiveComponent* MyComp, UPrimitiveComponent* OtherComp);
    void OnHit(UPrimitiveComponent* MyComp, UPrimitiveComponent* OtherComp);
     
    bool IsOverlappingActor(const AActor* Other) const;

    // ───── 복사 관련 ────────────────────────────
    void DuplicateSubObjects() override;
    void PostDuplicate() override;
    

    // Serialize
    void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;

    FGameObject* GetGameObject() const
    {
        if (LuaGameObject)
            return LuaGameObject;
        return nullptr;
    }

public:
    UWorld* World = nullptr;
    USceneComponent* RootComponent = nullptr;
    UTextRenderComponent* TextComp = nullptr;

    UPROPERTY(EditAnywhere, Category="[액터]", Tooltip="액터의 태그를 지정합니다.")
    FString Tag;  // for collision check

protected:
    // NOTE: RootComponent, CollisionComponent 등 기본 보호 컴포넌트들도
    // OwnedComponents와 SceneComponents에 포함되어 관리됨.
    TSet<UActorComponent*> OwnedComponents;   // 모든 컴포넌트 (씬/비씬)
    TArray<USceneComponent*> SceneComponents; // 씬 컴포넌트들만 별도 캐시(트리/렌더/ImGui용)
    
    bool bTickInEditor = false; // 에디터에서도 틱 허용

    UPROPERTY(EditAnywhere, Category="[액터]")
    bool bActorHiddenInGame = false;

    UPROPERTY(EditAnywhere, Category="[액터]")
    bool bActorIsActive = true;       // 활성 상태(사용자 on/off), tick 적용

    // Actor의 Visibility는 루트 컴포넌트로 설정
    bool bHiddenInEditor = false;

    bool bPendingDestroy = false;

    bool bIsPicked = false;
    bool bCanEverTick = true;   // Tick을 허용하는 Actor 라는 뜻 (생성자 시점에만 변경해야 됨)
    bool bIsCulled = false;

    float CustomTimeDillation;

private:
    FGameObject* LuaGameObject = nullptr;
};
