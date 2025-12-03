#include "pch.h"
#include "PhysScene.h"

#include "BodyInstance.h"
#include "PhysXSimEventCallback.h"
#include "PrimitiveComponent.h"

// 커스텀 Simulation Filter Shader
// FilterData 구조:
// word0: 충돌 채널 비트 (이 바디의 타입)
// word1: 충돌 마스크 (어떤 채널과 충돌할지)
// word2: 초기 겹침으로 인한 충돌 무시 마스크 (비트 플래그)
// word3: 바디 인덱스 (word2 비트 체크용)
PxFilterFlags CustomFilterShader(
    PxFilterObjectAttributes attributes0, PxFilterData filterData0,
    PxFilterObjectAttributes attributes1, PxFilterData filterData1,
    PxPairFlags& pairFlags, const void* constantBlock, PxU32 constantBlockSize)
{
    // 트리거 처리
    if (PxFilterObjectIsTrigger(attributes0) || PxFilterObjectIsTrigger(attributes1))
    {
        pairFlags = PxPairFlag::eTRIGGER_DEFAULT;
        return PxFilterFlag::eDEFAULT;
    }

    // 충돌 채널/마스크 기반 필터링
    // word0: 자신의 채널 비트, word1: 충돌할 채널 마스크
    // 양쪽 모두 상대방의 채널이 자신의 마스크에 포함되어야 충돌
    PxU32 channel0 = filterData0.word0;
    PxU32 mask0 = filterData0.word1;
    PxU32 channel1 = filterData1.word0;
    PxU32 mask1 = filterData1.word1;

    // 양방향 체크: 0이 1과 충돌하려면 mask0에 channel1이 포함되어야 하고,
    //              1이 0과 충돌하려면 mask1에 channel0이 포함되어야 함
    if (!(mask0 & channel1) || !(mask1 & channel0))
    {
        return PxFilterFlag::eSUPPRESS;  // 충돌 채널이 맞지 않음
    }

    // 초기 겹침으로 인한 충돌 무시 체크
    // word2: 무시할 바디 인덱스 비트, word3: 자신의 바디 인덱스
    PxU32 index0 = filterData0.word3;
    PxU32 index1 = filterData1.word3;

    // 0번이 1번을 무시하거나, 1번이 0번을 무시하는 경우
    if ((filterData0.word2 & (1 << index1)) || (filterData1.word2 & (1 << index0)))
    {
        return PxFilterFlag::eSUPPRESS;  // 충돌 무시
    }

    // 기본 충돌 처리
    pairFlags = PxPairFlag::eCONTACT_DEFAULT;

    // 충돌 이벤트(Hit) 알림 활성화
    pairFlags |= PxPairFlag::eNOTIFY_TOUCH_FOUND;

    // CCD 활성화
    pairFlags |= PxPairFlag::eDETECT_CCD_CONTACT;

    return PxFilterFlag::eDEFAULT;
}

FPhysScene::FPhysScene(UWorld* InOwningWorld)
    : PhysXScene(nullptr)
    , OwningWorld(InOwningWorld)
    , bPhysXSceneExecuting(false)
{
}

FPhysScene::~FPhysScene()
{
    TermPhysScene();
}

void FPhysScene::StartFrame()
{
    if (OwningWorld)
    {
        FlushDeferredAdds();
        float DeltaTime = OwningWorld->GetDeltaTime(EDeltaTime::Game);
        TickPhysScene(DeltaTime);
    }
}

void FPhysScene::EndFrame(ULineComponent* InLineComponent)
{
    WaitPhysScene();
    ProcessPhysScene();

    if (InLineComponent)
    {
        // @todo 디버그 라인 렌더링
    }
}

void FPhysScene::AddPendingCollisionNotify(const FCollisionNotifyInfo& NotifyInfo)
{
    std::lock_guard<std::mutex> Lock(NotifyMutex);
    PendingCollisionNotifies.push_back(NotifyInfo);
}

void FPhysScene::AddPendingCollisionNotify(FCollisionNotifyInfo&& NotifyInfo)
{
    std::lock_guard<std::mutex> Lock(NotifyMutex);
    PendingCollisionNotifies.push_back(std::move(NotifyInfo));
}

void FPhysScene::DeferAddActor(PxActor* InActor)
{
    if (!InActor) { return; }

    std::lock_guard<std::mutex> Lock(DeferredReleaseMutex);
    DeferredAddQueue.Add(InActor);
}

void FPhysScene::FlushDeferredAdds()
{
    if (!PhysXScene) { return; }
    
    std::lock_guard<std::mutex> Lock(DeferredAddMutex);

    {
        SCOPED_SCENE_WRITE_LOCK(PhysXScene);

        for (PxActor* Actor : DeferredAddQueue)
        {
            if (Actor)
            {
                PhysXScene->addActor(*Actor);
            }
        }
    }

    DeferredAddQueue.Empty();
}

void FPhysScene::DeferReleaseActor(PxActor* InActor)
{
    if (!InActor) { return; }

    InActor->userData = nullptr;

    std::lock_guard<std::mutex> Lock(DeferredReleaseMutex);
    DeferredReleaseQueue.Add(InActor);
}

void FPhysScene::FlushDeferredReleases()
{
    if (!PhysXScene) { return; }
    
    std::lock_guard<std::mutex> Lock(DeferredReleaseMutex);

    for (PxActor* Actor : DeferredReleaseQueue)
    {
        if (Actor)
        {
            Actor->release();
        }
    }
    DeferredReleaseQueue.Empty();
}

void FPhysScene::InitPhysScene()
{
    if (PhysXScene) { return; }

    if (!GPhysXSDK)
    {
        UE_LOG("[PhysX Error] PhysX SDK가 초기화되지 않았습니다.");
        return;
    }

    PxSceneDesc SceneDesc(GPhysXSDK->getTolerancesScale());

    if (!GPhysXDispatcher)
    {
        UE_LOG("[PhysX Error] PhysX Dispatcher가 초기화되지 않았습니다.");
    }
    SceneDesc.cpuDispatcher = GPhysXDispatcher;
    SceneDesc.filterShader = CustomFilterShader;

    SceneDesc.flags |= PxSceneFlag::eENABLE_PCM;
    SceneDesc.flags |= PxSceneFlag::eENABLE_STABILIZATION;
    SceneDesc.flags |= PxSceneFlag::eENABLE_ACTIVE_ACTORS;
    SceneDesc.flags |= PxSceneFlag::eENABLE_CCD;

    SceneDesc.gravity = PxVec3(0, 0, -9.81);

    SimEventCallback = new FPhysXSimEventCallback(this);
    SceneDesc.simulationEventCallback = SimEventCallback;

    PhysXScene = GPhysXSDK->createScene(SceneDesc);

    if (PhysXScene)
    {
        if (PxPvdSceneClient* PVDClient = PhysXScene->getScenePvdClient())
        {
            PVDClient->setScenePvdFlags(PxPvdSceneFlag::eTRANSMIT_CONSTRAINTS |
                                        PxPvdSceneFlag::eTRANSMIT_CONTACTS    |
                                        PxPvdSceneFlag::eTRANSMIT_SCENEQUERIES);
        }
        UE_LOG("[PhysX] PhysX Scene 생성에 성공했습니다.");
    }
    else
    {
        UE_LOG("[PhysX Error] PhysX Scene 생성에 실패했습니다.");
    }
}

void FPhysScene::TermPhysScene()
{
    if (PhysXScene)
    {
        WaitPhysScene();

        PhysXScene->release();
        PhysXScene = nullptr;
    }

    if (SimEventCallback)
    {
        delete SimEventCallback;
        SimEventCallback = nullptr;
    }
}

void FPhysScene::TickPhysScene(float DeltaTime)
{
    if (!PhysXScene)          { return; }

    if (bPhysXSceneExecuting) { return; }

    if (DeltaTime <= 0.0f)    { return; }

    PhysXScene->simulate(DeltaTime);
    bPhysXSceneExecuting = true;
}

void FPhysScene::WaitPhysScene()
{
    if (!PhysXScene) { return; }

    if (bPhysXSceneExecuting)
    {
        PhysXScene->fetchResults(true);
        bPhysXSceneExecuting = false;
    }
}

void FPhysScene::ProcessPhysScene()
{
    if (!PhysXScene) { return; }

    SyncComponentsToBodies();

    DispatchPhysNotifications_AssumesLocked();

    FlushDeferredReleases();
}

void FPhysScene::SyncComponentsToBodies()
{
    if (!PhysXScene) { return; }

    PxU32 NbActiveActors = 0;
    PxActor** ActiveActors = PhysXScene->getActiveActors(NbActiveActors);

    for (PxU32 i = 0; i < NbActiveActors; i++)
    {
        if (!ActiveActors[i]) { continue; }

        PxRigidActor* RigidActor = ActiveActors[i]->is<PxRigidActor>();

        if (!RigidActor) { continue; }

        // userData가 nullptr이면 이미 정리된 바디 (TermBody에서 클리어됨)
        void* UserData = RigidActor->userData;
        if (!UserData) { continue; }

        FBodyInstance* BodyInstance = static_cast<FBodyInstance*>(UserData);

        // BodyInstance 유효성 검사: RigidActor가 일치하는지 확인
        // (정리 중인 바디이거나 잘못된 포인터인 경우 방지)
        if (!BodyInstance->IsValidBodyInstance() || BodyInstance->RigidActor != RigidActor)
        {
            continue;
        }

        // 랙돌 바디는 SyncBonesFromPhysics()에서 별도 처리됨
        if (BodyInstance->bIsRagdollBody)
        {
            continue;
        }

        // OwnerComponent가 유효한지 확인
        UPrimitiveComponent* OwnerComp = BodyInstance->OwnerComponent;
        if (!OwnerComp)
        {
            continue;
        }

        FTransform NewTransform = BodyInstance->GetUnrealWorldTransform();
        NewTransform.Scale3D = OwnerComp->GetWorldScale();
        OwnerComp->SetWorldTransform(NewTransform, EUpdateTransformFlags::SkipPhysicsUpdate, ETeleportType::None);
    }
}

void FPhysScene::DispatchPhysNotifications_AssumesLocked()
{
    // @note 델리게이트 실행 중에 물리 함수를 호출할 시 데드락에 빠질 수 있음
    //       이를 방지하기 위해서 큐의 데이터를 로컬로 이동시킴
    TArray<FCollisionNotifyInfo> LocalNotifies;
    {
        std::lock_guard<std::mutex> Lock(NotifyMutex);
        if (PendingCollisionNotifies.IsEmpty())
        {
            return;
        }
        LocalNotifies = std::move(PendingCollisionNotifies);
        PendingCollisionNotifies.Empty();
    }

    for (const FCollisionNotifyInfo& Notify : LocalNotifies)
    {
        AActor* Actor0 = Notify.Info0.Actor.Get();
        UPrimitiveComponent* Comp0 = Notify.Info0.Component.Get();

        AActor* Actor1 = Notify.Info1.Actor.Get();
        UPrimitiveComponent* Comp1 = Notify.Info1.Component.Get();

        bool bIsValid0 = (Actor0 && Comp0);
        bool bIsValid1 = (Actor1 && Comp1);

        switch (Notify.Type)
        {
        case ECollisionNotifyType::Hit:
        {
            if (!bIsValid0 || !bIsValid1) continue;

            // @todo 노말 방향이 올바르게 시뮬레이션으로부터 전달되었는지 확인해야 함(PhysX의 구현에 따라 반대 방향일 수 있음)

            // @note Actor0의 관점에서 구성한 HitResult
            //       즉, HitResult는 Actor0가 Actor1을 때린 것이므로 법선의 방향은 Shape0 -> Shpae1의 방향
            if (Notify.bCallEvent0)
            {
                FHitResult HitResult0;
                HitResult0.Actor        = Actor1;                      // 부딪힌 대상
                HitResult0.Component    = Comp1;                   // 부딪힌 컴포넌트
                HitResult0.BoneName     = Notify.Info1.BoneName;    // 부딪힌 부위
                HitResult0.ImpactPoint  = Notify.ImpactPoint;
                HitResult0.ImpactNormal = Notify.ImpactNormal;  // Actor0 표면의 법선 
                HitResult0.bBlockingHit = true;
                
                FVector Impulse0 = Notify.ImpactNormal * Notify.TotalImpulse;

                Comp0->DispatchBlockingHit(Actor1, Comp1, Impulse0, HitResult0);
            }

                
            // @note Actor1의 관점에서 구성한 HitResult
            //       즉, HitResult는 Actor1가 Actor0을 때린 것이므로 법선의 방향은 Shape1 -> Shpae0의 방향
            if (Notify.bCallEvent1)
            {
                FHitResult HitResult1;
                HitResult1.Actor        = Actor0;
                HitResult1.Component    = Comp0;
                HitResult1.BoneName     = Notify.Info0.BoneName;
                HitResult1.ImpactPoint  = Notify.ImpactPoint;
                HitResult1.ImpactNormal = -Notify.ImpactNormal; 
                HitResult1.bBlockingHit = true;

                // 충격량 방향: Actor0가 Actor1에게 가한 힘 (반대 방향)
                FVector Impulse1 = -Notify.ImpactNormal * Notify.TotalImpulse;

                Comp1->DispatchBlockingHit(Actor0, Comp0, Impulse1, HitResult1);
            }
            break;
        }

        // @todo [[fallthrough]] 어트리뷰트가 왜 적용이 안될까...
        case ECollisionNotifyType::BeginOverlap: __fallthrough;
        case ECollisionNotifyType::EndOverlap:
        {
            if (!bIsValid0 || !bIsValid1) continue;
            
            if (Notify.Type == ECollisionNotifyType::BeginOverlap)
            {
                if (Notify.bCallEvent0) Comp0->DispatchBeginOverlap(Actor1, Comp1, Notify.Info1.BodyIndex);
                if (Notify.bCallEvent1) Comp1->DispatchBeginOverlap(Actor0, Comp0, Notify.Info0.BodyIndex);
            }
            else 
            {
                if (Notify.bCallEvent0) Comp0->DispatchEndOverlap(Actor1, Comp1, Notify.Info1.BodyIndex);
                if (Notify.bCallEvent1) Comp1->DispatchEndOverlap(Actor0, Comp0, Notify.Info0.BodyIndex);
            }
            break;
        }

        case ECollisionNotifyType::Wake:
        {
            if (bIsValid0 && Notify.bCallEvent0)
            {
                Comp0->DispatchWakeEvents(true, Notify.Info0.BoneName);
            }
            break;
        }

        case ECollisionNotifyType::Sleep:
        {
            if (bIsValid0 && Notify.bCallEvent0)
            {
                Comp0->DispatchWakeEvents(false, Notify.Info0.BoneName);
            }
            break;
        }
        
        default:
            break;
        }
    }
}

// 레이캐스트 필터 콜백: 특정 액터 무시
class FIgnoreActorFilterCallback : public PxQueryFilterCallback
{
public:
    AActor* IgnoreActor;

    FIgnoreActorFilterCallback(AActor* InIgnoreActor) : IgnoreActor(InIgnoreActor) {}

    virtual PxQueryHitType::Enum preFilter(
        const PxFilterData& filterData, const PxShape* shape, const PxRigidActor* actor, PxHitFlags& queryFlags) override
    {
        if (!actor || !IgnoreActor)
        {
            return PxQueryHitType::eBLOCK;
        }

        // userData에서 BodyInstance를 가져와서 OwnerComponent의 Owner 확인
        void* UserData = actor->userData;
        if (UserData)
        {
            FBodyInstance* BodyInstance = static_cast<FBodyInstance*>(UserData);
            if (BodyInstance && BodyInstance->OwnerComponent)
            {
                AActor* HitActor = BodyInstance->OwnerComponent->GetOwner();
                if (HitActor == IgnoreActor)
                {
                    return PxQueryHitType::eNONE;  // 무시
                }
            }
        }

        return PxQueryHitType::eBLOCK;
    }

    virtual PxQueryHitType::Enum postFilter(const PxFilterData& filterData, const PxQueryHit& hit) override
    {
        return PxQueryHitType::eBLOCK;
    }
};

bool FPhysScene::Raycast(const FVector& Origin, const FVector& Direction, float MaxDistance,
                         FVector& OutHitLocation, FVector& OutHitNormal, float& OutHitDistance,
                         AActor* IgnoreActor) const
{
    if (!PhysXScene)
    {
        return false;
    }

    PxVec3 PxOrigin = U2PVector(Origin);
    PxVec3 PxDirection = U2PVector(Direction.GetNormalized());

    PxRaycastBuffer Hit;

    bool bHit = false;

    if (IgnoreActor)
    {
        // 필터 콜백을 사용하여 특정 액터 무시
        FIgnoreActorFilterCallback FilterCallback(IgnoreActor);
        PxQueryFilterData FilterData;
        FilterData.flags = PxQueryFlag::eDYNAMIC | PxQueryFlag::eSTATIC | PxQueryFlag::ePREFILTER;

        bHit = PhysXScene->raycast(PxOrigin, PxDirection, MaxDistance, Hit, PxHitFlag::eDEFAULT, FilterData, &FilterCallback);
    }
    else
    {
        bHit = PhysXScene->raycast(PxOrigin, PxDirection, MaxDistance, Hit);
    }

    if (bHit && Hit.hasBlock)
    {
        OutHitLocation = P2UVector(Hit.block.position);
        OutHitNormal = P2UVector(Hit.block.normal);
        OutHitDistance = Hit.block.distance;
        return true;
    }

    return false;
}

// ==================================================================================
// Sweep Interface Implementation
// ==================================================================================

bool FPhysScene::SweepSingleInternal(const PxGeometry& Geometry, const FVector& Start, const FVector& End,
                                      const FQuat& Rotation, FHitResult& OutHit, AActor* IgnoreActor) const
{
    if (!PhysXScene)
    {
        return false;
    }

    OutHit.Init();

    FVector Direction = End - Start;
    float Distance = Direction.Size();

    if (Distance < KINDA_SMALL_NUMBER)
    {
        return false;
    }

    Direction /= Distance;  // Normalize

    PxVec3 PxStart = U2PVector(Start);
    PxVec3 PxDirection = U2PVector(Direction);
    PxQuat PxRotation = U2PQuat(Rotation);
    PxTransform StartPose(PxStart, PxRotation);

    PxSweepBuffer Hit;
    bool bHit = false;

    if (IgnoreActor)
    {
        FIgnoreActorFilterCallback FilterCallback(IgnoreActor);
        PxQueryFilterData FilterData;
        FilterData.flags = PxQueryFlag::eDYNAMIC | PxQueryFlag::eSTATIC | PxQueryFlag::ePREFILTER;

        bHit = PhysXScene->sweep(Geometry, StartPose, PxDirection, Distance, Hit,
                                  PxHitFlag::eDEFAULT | PxHitFlag::eNORMAL | PxHitFlag::ePOSITION,
                                  FilterData, &FilterCallback);
    }
    else
    {
        bHit = PhysXScene->sweep(Geometry, StartPose, PxDirection, Distance, Hit,
                                  PxHitFlag::eDEFAULT | PxHitFlag::eNORMAL | PxHitFlag::ePOSITION);
    }

    if (bHit && Hit.hasBlock)
    {
        OutHit.bBlockingHit = true;
        OutHit.Distance = Hit.block.distance;
        OutHit.ImpactPoint = P2UVector(Hit.block.position);
        OutHit.ImpactNormal = P2UVector(Hit.block.normal);

        // 히트한 액터/컴포넌트 정보 추출
        if (Hit.block.actor && Hit.block.actor->userData)
        {
            FBodyInstance* BodyInstance = static_cast<FBodyInstance*>(Hit.block.actor->userData);
            if (BodyInstance && BodyInstance->OwnerComponent)
            {
                OutHit.Component = BodyInstance->OwnerComponent;
                OutHit.Actor = BodyInstance->OwnerComponent->GetOwner();
            }
        }

        return true;
    }

    return false;
}

bool FPhysScene::SweepSingleCapsule(const FVector& Start, const FVector& End,
                                     float Radius, float HalfHeight,
                                     FHitResult& OutHit,
                                     AActor* IgnoreActor) const
{
    // PhysX 캡슐은 X축 정렬, 언리얼/Mundi는 Z축 정렬
    // 따라서 캡슐을 Z축 정렬로 회전시켜야 함
    PxCapsuleGeometry CapsuleGeom(Radius, HalfHeight);

    // Z축 정렬을 위해 Y축으로 90도 회전
    FQuat CapsuleRotation = FQuat::FromAxisAngle(FVector(0, 1, 0), DegreesToRadians(90.0f));

    return SweepSingleInternal(CapsuleGeom, Start, End, CapsuleRotation, OutHit, IgnoreActor);
}

bool FPhysScene::SweepSingleSphere(const FVector& Start, const FVector& End,
                                    float Radius,
                                    FHitResult& OutHit,
                                    AActor* IgnoreActor) const
{
    PxSphereGeometry SphereGeom(Radius);

    return SweepSingleInternal(SphereGeom, Start, End, FQuat::Identity(), OutHit, IgnoreActor);
}

bool FPhysScene::SweepSingleBox(const FVector& Start, const FVector& End,
                                 const FVector& HalfExtent, const FQuat& Rotation,
                                 FHitResult& OutHit,
                                 AActor* IgnoreActor) const
{
    PxBoxGeometry BoxGeom(U2PVector(HalfExtent));

    return SweepSingleInternal(BoxGeom, Start, End, Rotation, OutHit, IgnoreActor);
}

bool FPhysScene::ComputePenetrationCapsule(const FVector& Position,
                                            float Radius, float HalfHeight,
                                            FVector& OutMTD, float& OutPenetrationDepth,
                                            AActor* IgnoreActor) const
{
    if (!PhysXScene)
    {
        return false;
    }

    OutMTD = FVector::Zero();
    OutPenetrationDepth = 0.0f;

    // PhysX 캡슐 생성 (Z축 정렬)
    PxCapsuleGeometry CapsuleGeom(Radius, HalfHeight);
    FQuat CapsuleRotation = FQuat::FromAxisAngle(FVector(0, 1, 0), DegreesToRadians(90.0f));
    PxTransform CapsulePose(U2PVector(Position), U2PQuat(CapsuleRotation));

    // Overlap 쿼리로 겹치는 물체들 찾기
    const PxU32 MaxOverlaps = 16;
    PxOverlapHit OverlapHits[MaxOverlaps];
    PxOverlapBuffer OverlapBuffer(OverlapHits, MaxOverlaps);

    PxQueryFilterData FilterData;
    FilterData.flags = PxQueryFlag::eSTATIC;  // 다이나믹 객체는 무시 (PhysX 시뮬레이션에서 처리)

    bool bHasOverlap = PhysXScene->overlap(CapsuleGeom, CapsulePose, OverlapBuffer, FilterData);

    if (!bHasOverlap || OverlapBuffer.getNbAnyHits() == 0)
    {
        return false;
    }

    // 모든 겹침에 대해 MTD 계산하고 가장 큰 침투 해결
    FVector TotalMTD = FVector::Zero();
    float MaxPenetration = 0.0f;

    for (PxU32 i = 0; i < OverlapBuffer.getNbAnyHits(); ++i)
    {
        const PxOverlapHit& Hit = OverlapBuffer.getAnyHit(i);

        if (!Hit.actor || !Hit.shape)
        {
            continue;
        }

        // IgnoreActor 체크
        if (IgnoreActor)
        {
            FBodyInstance* BodyInstance = static_cast<FBodyInstance*>(Hit.actor->userData);
            if (BodyInstance && BodyInstance->OwnerComponent)
            {
                if (BodyInstance->OwnerComponent->GetOwner() == IgnoreActor)
                {
                    continue;
                }
            }
        }

        // MTD 계산
        PxVec3 MtdDirection;
        PxF32 MtdDepth;

        PxTransform ShapePose = PxShapeExt::getGlobalPose(*Hit.shape, *Hit.actor);
        PxGeometryHolder GeomHolder = Hit.shape->getGeometry();

        bool bSuccess = PxGeometryQuery::computePenetration(
            MtdDirection, MtdDepth,
            CapsuleGeom, CapsulePose,
            GeomHolder.any(), ShapePose
        );

        if (bSuccess && MtdDepth > 0.0f)
        {
            FVector ThisMTD = P2UVector(MtdDirection) * MtdDepth;

            // 가장 큰 침투 방향으로 누적
            if (MtdDepth > MaxPenetration)
            {
                MaxPenetration = MtdDepth;
                OutMTD = P2UVector(MtdDirection);
                OutPenetrationDepth = MtdDepth;
            }

            TotalMTD += ThisMTD;
        }
    }

    // 누적된 MTD가 있으면 정규화
    if (TotalMTD.SizeSquared() > KINDA_SMALL_NUMBER)
    {
        OutMTD = TotalMTD.GetNormalized();
        OutPenetrationDepth = TotalMTD.Size();
        return true;
    }

    return MaxPenetration > 0.0f;
}
