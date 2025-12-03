#pragma once

#include <mutex>

#include "PhysXSupport.h"

class FPhysXSimEventCallback;

class FPhysScene
{
public:
    FPhysScene(UWorld* InOwningWorld);
    
    ~FPhysScene();

    // ==================================================================================
    // Physics Interface
    // ==================================================================================

    /** 프레임을 시작한다 (시뮬레이션 시작). */
    void StartFrame();

    /** 프레임을 종료한다 (시뮬레이션 완료 후 동기화). */
    void EndFrame(ULineComponent* InLineComponent);
    
    void SetOwningWorld(UWorld* InOwningWorld) { OwningWorld = InOwningWorld; }
    
    UWorld* GetOwningWorld() { return OwningWorld; }

    const UWorld* GetOwningWorld() const { return OwningWorld; }

    void AddPendingCollisionNotify(const FCollisionNotifyInfo& NotifyInfo);

    void AddPendingCollisionNotify(FCollisionNotifyInfo&& NotifyInfo);

    // ==================================================================================
    // PhysScene Interface
    // ==================================================================================
    
    physx::PxScene* GetPxScene() const { return PhysXScene; }

    /** @note 액터의 추가는 시뮬레이션 시작 직전에 일괄적으로 이루어진다. */
    void DeferAddActor(PxActor* InActor);

    /** 대기중인 액터 추가를 일괄적으로 처리한다. */
    void FlushDeferredAdds();

    /** @note 액터의 제거는 시뮬레이션 종료 직후에 일괄적으로 이루어진다. */
    void DeferReleaseActor(PxActor* InActor);

    /** 대기중인 액터 해제를 일괄적으로 처리한다. */
    void FlushDeferredReleases();
    
    // ==================================================================================

    /** PhysX Scene 초기화 */
    void InitPhysScene();

    /** PhysX Scene 종료 */
    void TermPhysScene();

    /** 시뮬레이션 종료를 대기한다. (BodyInstance::TermBody에서 호출) */
    void WaitPhysScene();

    // ==================================================================================
    // Raycast Interface
    // ==================================================================================

    /**
     * @brief 레이캐스트를 수행하여 첫 번째 히트를 반환
     * @param Origin 레이 시작점
     * @param Direction 레이 방향 (정규화됨)
     * @param MaxDistance 최대 거리
     * @param OutHitLocation 히트 위치 (출력)
     * @param OutHitNormal 히트 노멀 (출력)
     * @param OutHitDistance 히트 거리 (출력)
     * @param IgnoreActor 무시할 액터 (nullptr이면 무시 안 함)
     * @return 히트 여부
     */
    bool Raycast(const FVector& Origin, const FVector& Direction, float MaxDistance,
                 FVector& OutHitLocation, FVector& OutHitNormal, float& OutHitDistance,
                 AActor* IgnoreActor = nullptr) const;

    // ==================================================================================
    // Sweep Interface (Shape Cast)
    // ==================================================================================

    /**
     * @brief 캡슐 형태로 스윕하여 첫 번째 히트를 반환
     * @param Start 시작 위치
     * @param End 끝 위치
     * @param Radius 캡슐 반경
     * @param HalfHeight 캡슐 반높이 (헤미스피어 제외)
     * @param OutHit 히트 결과 (출력)
     * @param IgnoreActor 무시할 액터 (nullptr이면 무시 안 함)
     * @return 히트 여부
     */
    bool SweepSingleCapsule(const FVector& Start, const FVector& End,
                            float Radius, float HalfHeight,
                            FHitResult& OutHit,
                            AActor* IgnoreActor = nullptr) const;

    /**
     * @brief 스피어 형태로 스윕하여 첫 번째 히트를 반환
     * @param Start 시작 위치
     * @param End 끝 위치
     * @param Radius 스피어 반경
     * @param OutHit 히트 결과 (출력)
     * @param IgnoreActor 무시할 액터 (nullptr이면 무시 안 함)
     * @return 히트 여부
     */
    bool SweepSingleSphere(const FVector& Start, const FVector& End,
                           float Radius,
                           FHitResult& OutHit,
                           AActor* IgnoreActor = nullptr) const;

    /**
     * @brief 박스 형태로 스윕하여 첫 번째 히트를 반환
     * @param Start 시작 위치
     * @param End 끝 위치
     * @param HalfExtent 박스 반크기
     * @param Rotation 박스 회전
     * @param OutHit 히트 결과 (출력)
     * @param IgnoreActor 무시할 액터 (nullptr이면 무시 안 함)
     * @return 히트 여부
     */
    bool SweepSingleBox(const FVector& Start, const FVector& End,
                        const FVector& HalfExtent, const FQuat& Rotation,
                        FHitResult& OutHit,
                        AActor* IgnoreActor = nullptr) const;

    // ==================================================================================
    // Overlap / Penetration Interface
    // ==================================================================================

    /**
     * @brief 캡슐이 다른 물체와 겹치는지 확인하고 MTD(최소 이동 거리) 반환
     * @param Position 캡슐 위치
     * @param Radius 캡슐 반경
     * @param HalfHeight 캡슐 반높이
     * @param OutMTD 최소 이동 방향 (정규화됨, 출력)
     * @param OutPenetrationDepth 침투 깊이 (출력)
     * @param IgnoreActor 무시할 액터
     * @return 겹침이 있으면 true
     */
    bool ComputePenetrationCapsule(const FVector& Position,
                                   float Radius, float HalfHeight,
                                   FVector& OutMTD, float& OutPenetrationDepth,
                                   AActor* IgnoreActor = nullptr) const;

    /** 실제 시뮬레이션 로직을 수행한다 (에디터에서 직접 호출 가능). */
    void TickPhysScene(float DeltaTime);

    /** 시뮬레이션 결과를 처리하고 동기화한다. */
    void ProcessPhysScene();

private:
    /**
     * @brief 내부 스윕 헬퍼 함수 (모든 지오메트리 공용)
     */
    bool SweepSingleInternal(const PxGeometry& Geometry, const FVector& Start, const FVector& End,
                             const FQuat& Rotation, FHitResult& OutHit, AActor* IgnoreActor) const;

    /** 컴포넌트의 트랜스폼에 시뮬레이션 결과를 동기화 */
    void SyncComponentsToBodies();

    /** 큐에 쌓인 충돌 이벤트를 메인 스레드에서 처리 */
    void DispatchPhysNotifications_AssumesLocked();

    /** PhysX Scene */
    PxScene* PhysXScene;

    /** PhysX Scene을 소유하고 있는 월드 */
    UWorld* OwningWorld;

    /** PhysX 이벤트 콜백 */
    FPhysXSimEventCallback* SimEventCallback;
    
    /** 삽입 대기 중인 PhysX 액터 목록 */
    TArray<PxActor*> DeferredAddQueue;
    
    /** 시뮬레이션 중 액터 추가 큐 접근용 뮤텍스 */
    std::mutex DeferredAddMutex;

    /** 해제 대기 중인 PhysX 액터 목록 */
    TArray<PxActor*> DeferredReleaseQueue;
    
    /** 시뮬레이션 중 액터 해제 큐 접근용 뮤텍스 */
    std::mutex DeferredReleaseMutex;

    /** 시뮬레이션이 종료되고 처리될 충돌 정보 큐 */
    TArray<FCollisionNotifyInfo> PendingCollisionNotifies;

    /** 시뮬레이션 중 충돌 정보 큐 접근용 뮤텍스 */
    std::mutex NotifyMutex;

    /** PhysX Scene 시뮬레이션 실행 여부 (실행 시점과 동기화 시점 사이) */
    bool bPhysXSceneExecuting;
};
