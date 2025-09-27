#pragma once
#include "GizmoActor.h"
#include "Object.h"
#include "GridActor.h"
#include "Occlusion.h"
#include "BVHierachy.h"
#include "Frustum.h"
#include "Enums.h"
// forward declare to avoid heavy include

// Forward Declarations
class UResourceManager;
class UUIManager;
class UInputManager;
class USelectionManager;
class AActor;
class URenderer;
class ACameraActor;
class AGizmoActor;
class FViewport;
class SMultiViewportWindow;
struct FTransform;
struct FPrimitiveData;
class SViewportWindow;
class UWorldPartitionManager;
class AStaticMeshActor;

/**
 * UWorld
 * - 월드 단위의 액터/타임/매니저 관리 클래스
 */


class UWorld final : public UObject
{
public:
    DECLARE_CLASS(UWorld, UObject)
    UWorld();
    ~UWorld() override;
    static UWorld& GetInstance();
    
protected:


public:
    /** 초기화 */
    void Initialize();
    void InitializeMainCamera();
    void InitializeGrid();
    void InitializeGizmo();
    
    // 액터 인터페이스 관리
    void SetupActorReferences();
    
    // 선택 및 피킹 처리
    void ProcessActorSelection();

    void ProcessViewportInput();

    void SetRenderer(URenderer* InRenderer);
    URenderer* GetRenderer() { return Renderer; }

    void SetMainViewport(SViewportWindow* InViewport) { MainViewport = InViewport; }
    SViewportWindow* GetMainViewport() const { return MainViewport; }

    void SetMultiViewportWindow(SMultiViewportWindow* InMultiViewport) { MultiViewport = InMultiViewport; }
    SMultiViewportWindow* GetMultiViewportWindow() const { return MultiViewport; }

    template<class T>
    T* SpawnActor();

    template<class T>
    T* SpawnActor(const FTransform& Transform);
    
    // 벌크 액터 스폰 - 대량 액터 생성 시 사용
    template<class T>
    TArray<T*> BulkSpawnActors(const TArray<FTransform>& Transforms);

    bool DestroyActor(AActor* Actor);

    // Partial hooks
    void OnActorSpawned(AActor* Actor);
    void OnActorDestroyed(AActor* Actor);

    void CreateNewScene();
    void LoadScene(const FString& SceneName);
    void SaveScene(const FString& SceneName);
    ACameraActor* GetCameraActor() { return MainCameraActor; }

    EViewModeIndex GetViewModeIndex() { return ViewModeIndex; }
    void SetViewModeIndex(EViewModeIndex InViewModeIndex) { ViewModeIndex = InViewModeIndex; }

    /** === Show Flag 시스템 === */
    EEngineShowFlags GetShowFlags() const { return ShowFlags; }
    void SetShowFlags(EEngineShowFlags InShowFlags) { ShowFlags = InShowFlags; }
    void EnableShowFlag(EEngineShowFlags Flag) { ShowFlags |= Flag; }
    void DisableShowFlag(EEngineShowFlags Flag) { ShowFlags &= ~Flag; }
    void ToggleShowFlag(EEngineShowFlags Flag) { ShowFlags = HasShowFlag(ShowFlags, Flag) ? (ShowFlags & ~Flag) : (ShowFlags | Flag); }
    bool IsShowFlagEnabled(EEngineShowFlags Flag) const { return HasShowFlag(ShowFlags, Flag); }
    
    /** Generate unique name for actor based on type */
    FString GenerateUniqueActorName(const FString& ActorType);

    /** === 타임 / 틱 === */
    virtual void Tick(float DeltaSeconds);
    float GetTimeSeconds() const;

    /** === 렌더 === */
    void Render();
    void RenderSingleViewport();
    void RenderViewports(ACameraActor* Camera, FViewport* Viewport);
    //void GameRender(ACameraActor* Camera, FViewport* Viewport);

    // Partition manager
    //UWorldPartitionManager* GetPartitionManager() const { return PartitionManager; }


    /** === 필요한 엑터 게터 === */
    const TArray<AActor*>& GetActors() { return Actors; }
    AGizmoActor* GetGizmoActor();
    AGridActor* GetGridActor() { return GridActor; }

    void PushBackToStaticMeshActors(AStaticMeshActor* InStaticMeshActor) { StaticMeshActors.push_back(InStaticMeshActor); }

    
    /** === 레벨 / 월드 구성 === */
    // TArray<ULevel*> Levels;

    /** === 플레이어 / 컨트롤러 === */
    // APlayerController* GetFirstPlayerController() const;
    // TArray<APlayerController*> GetPlayerControllerIterator() const;

private:
    // 싱글톤 매니저 참조
    UResourceManager& ResourceManager;
    UUIManager& UIManager;
    UInputManager& InputManager;
    USelectionManager& SelectionManager;

    // World partition/spatial indexing //Non Single Ton
    //UWorldPartitionManager* PartitionManager = nullptr;

   
    // 메인 카메라
    ACameraActor* MainCameraActor = nullptr;

    AGridActor* GridActor = nullptr;
    // 렌더러 (월드가 소유)
    URenderer* Renderer;

    // 메인 뷰포트
    SViewportWindow* MainViewport = nullptr;
    // 멀티 뷰포트 윈도우
    SMultiViewportWindow* MultiViewport = nullptr;

    TArray<FPrimitiveData> Primitives;

    /** === 액터 관리 === */
    TArray<AActor*> EngineActors;
    /** === 액터 관리 === */
    TArray<AActor*> Actors;

    /** A dedicated array for static mesh actors to optimize culling. */
    TArray<class AStaticMeshActor*> StaticMeshActors;
    
    // Object naming system
    std::map<FString, int32> ObjectTypeCounts;
    
    /** == 기즈모 == */
    AGizmoActor* GizmoActor;

    /** === Show Flag 시스템 === */
    EEngineShowFlags ShowFlags = EEngineShowFlags::SF_DefaultEnabled;
    
    EViewModeIndex ViewModeIndex = EViewModeIndex::VMI_Unlit;


    // ================ 오클루전 관련 멤버들 ==================
    /*
    FOcclusionRing Occlusion;          // 2프레임 지연 링 버퍼
    uint32         CandidateBudget = 5000;

    FBVHierachy* BVHRoot = nullptr;  // 이미 있다면 연결만

    struct FBVHNodeCandidate
    {
        FBVHierachy* Node = nullptr;
        FOcclusionId Id = 0;
        FBound       Bounds;
    };

    static inline FOcclusionId MakeNodeId(FBVHierachy* N)
    {
        // 포인터 하위 32비트 사용(간단/안전). 원하면 FName으로 매핑 가능.
        return static_cast<FOcclusionId>(reinterpret_cast<uintptr_t>(N) & 0xffffffffu);
    }

    void GatherBVHCandidates(FBVHierachy* Root, const Frustum& ViewFrustum,
        uint32 Budget, TArray<FBVHNodeCandidate>& Out);

    void DrawBVHWithPredication(FBVHierachy* Node, URenderer* Renderer,
        const FMatrix& View, const FMatrix& Proj);
    void DrawBVHWithPredication(FBVHierachy* Node, URenderer* Renderer,
        const FMatrix& View, const FMatrix& Proj,
        const Frustum& ViewFrustum);
        */
	// ====================================================
};
template<class T>
inline T* UWorld::SpawnActor()
{
    return SpawnActor<T>(FTransform());
}

template<class T>
inline T* UWorld::SpawnActor(const FTransform& Transform)
{
    static_assert(std::is_base_of<AActor, T>::value, "T must be derived from AActor");

    // 새 액터 생성
    T* NewActor = NewObject<T>();

    // 초기 트랜스폼 적용
    NewActor->SetActorTransform(Transform);

    //  월드 등록
    NewActor->SetWorld(this);

    // 월드에 등록
    Actors.Add(NewActor);

    return NewActor;
}
