#include "pch.h"
#include "PhysicsScene.h"
#include "BodyInstance.h"
#include "PhysicsSystem.h"
#include "PhysXSimEventCallback.h"
#include "PlatformTime.h"
#include "PrimitiveComponent.h"
#include "VehicleComponent.h"

#define SCOPED_READ_LOCK(Scene) PxSceneReadLock ScopedReadLock(Scene);
#define SCOPED_WRITE_LOCK(Scene) PxSceneWriteLock ScopedWriteLock(Scene);

FPhysicsScene::FPhysicsScene() : mScene(nullptr) {}
FPhysicsScene::~FPhysicsScene()
{
    Shutdown();
}

static PxFilterFlags SimpleSimulationFilterShader(
    PxFilterObjectAttributes attributes0, PxFilterData filterData0,
    PxFilterObjectAttributes attributes1, PxFilterData filterData1,
    PxPairFlags& pairFlags, const void* constantBlock, PxU32 constantBlockSize)
{
    pairFlags = PxPairFlag::eCONTACT_DEFAULT;

    // Trigger인지 확인
    if (PxFilterObjectIsTrigger(attributes0) || PxFilterObjectIsTrigger(attributes1))
    {
        // Trigger면 뚫고 지나가야 하니 반발력(Solve) 끄기
        pairFlags &= ~PxPairFlag::eSOLVE_CONTACT;
        
        // Trigger 진입/이탈 알림 켜기
        pairFlags |= PxPairFlag::eNOTIFY_TOUCH_FOUND;
        pairFlags |= PxPairFlag::eNOTIFY_TOUCH_LOST;
        
        return PxFilterFlag::eDEFAULT;
    }
    // 충돌 시작(Hit) 알림을 받기 위해 플래그 추가
    pairFlags |= PxPairFlag::eNOTIFY_TOUCH_FOUND;
    pairFlags |= PxPairFlag::eNOTIFY_CONTACT_POINTS;
    
    return PxFilterFlag::eDEFAULT;
}

void FPhysicsScene::Initialize(FPhysicsSystem* System)
{
    if (!System) { return; }

    PxPhysics* Physics = System->GetPhysics();
    if (!Physics) { return; }

    PxSceneDesc SceneDesc(Physics->getTolerancesScale());
    SceneDesc.gravity = PxVec3(0.0f, -9.81f, 0.0f);

    // 멀티스레드 디스패처 공유
    if (PxDefaultCpuDispatcher* Dispatcher = System->GetCpuDispatcher())
    {
        SceneDesc.cpuDispatcher = Dispatcher;
    }
    else
    {
        return; 
    }

    SceneDesc.filterShader = SimpleSimulationFilterShader;
    mSimulationEventCallback = new FPhysXSimEventCallback();
    SceneDesc.simulationEventCallback = mSimulationEventCallback;
    SceneDesc.flags |= PxSceneFlag::eENABLE_ACTIVE_ACTORS; // 활성화된 액터 목록만 가져올 수 있게
    SceneDesc.flags |= PxSceneFlag::eENABLE_CCD; // CCD 허용
    SceneDesc.flags |= PxSceneFlag::eENABLE_PCM; // 충돌 접점 관리...?
    
    mScene = Physics->createScene(SceneDesc);
    PxPvdSceneClient* PvdClient = mScene->getScenePvdClient();
    if (PvdClient)
    {
        PvdClient->setScenePvdFlag(PxPvdSceneFlag::eTRANSMIT_CONSTRAINTS, true); // 제약 조건(Joint 등) 보이기
        PvdClient->setScenePvdFlag(PxPvdSceneFlag::eTRANSMIT_SCENEQUERIES, true); // 씬 쿼리(Raycast 등) 보이기
        PvdClient->setScenePvdFlag(PxPvdSceneFlag::eTRANSMIT_CONTACTS, true); // 충돌 접점(Contact Point) 보이기
    }
}

void FPhysicsScene::EnqueueCommand(PhysicsCommand Command)
{
    CommandQueue.push_back(Command);
}

void FPhysicsScene::ProcessCommandQueue()
{
    SCOPED_WRITE_LOCK(*mScene)

    for (const auto& Cmd : CommandQueue)
    {
        Cmd(); // 명령 실행
    }
    CommandQueue.clear();
}

// 비동기 시뮬레이션
void FPhysicsScene::Simulation(float DeltaTime)
{
    LeftoverTime += DeltaTime;

    // 고정 시간 시뮬레이션
    while (FixedDeltaTime <= LeftoverTime)
    {
        LeftoverTime -= FixedDeltaTime;

        mScene->simulate(FixedDeltaTime);

        for (UVehicleComponent* Vehicle : Vehicles)
        {
            if (Vehicle && Vehicle->bIsActive)  // NOTE: bIsActive 말고 검사가 더 필요한가?
            {
                Vehicle->Simulate(FixedDeltaTime);
            }
        }

        bIsSimulated = true;

        if (FixedDeltaTime <= LeftoverTime)
        {
            FetchAndSync();
        }
    }
}

void FPhysicsScene::FetchAndSync()
{
    if (!bIsSimulated)
    {
        return;
    }

    bIsSimulated = false;

    {
        TIME_PROFILE(PhysicsSyncTime)
        // 시뮬레이션 완료될 때까지 기다림
        mScene->fetchResults(true);
        
        // 멀티스레드 렌더링 환경 등을 고려하여 락을 거는 것이 안전
        SCOPED_READ_LOCK(*mScene)

        uint32 ActiveCount = 0;
        PxActor** ActiveActors = mScene->getActiveActors(ActiveCount);

        for (uint32 i = 0; i < ActiveCount; ++i)
        {
            PxActor* Actor = ActiveActors[i];
            if (FBodyInstance* BodyInstance = static_cast<FBodyInstance*>(Actor->userData))
            {
                if (BodyInstance->OwnerComponent)
                {
                    FTransform PxWorldTransform = BodyInstance->GetWorldTransform();
                    BodyInstance->OwnerComponent->SyncByPhysics(PxWorldTransform);
                }
            }
        }
    }
}

void FPhysicsScene::Shutdown()
{
    if (mScene)
    {
        // 씬 안에 있는 액터들은 World가 정리되며 자동으로 정리
        mScene->release();
        mScene = nullptr;
    }
    if (mSimulationEventCallback)
    {
        delete mSimulationEventCallback;
        mSimulationEventCallback = nullptr;
    }
}

void FPhysicsScene::AddActor(FBodyInstance* Body)
{
    if (mScene && Body)
    {
        // Add나 Remove는 Tick 도중 가능하기 때문에 Lock
        SCOPED_WRITE_LOCK(*mScene)
        Body->RigidActor->userData = Body;
        mScene->addActor(*Body->RigidActor);
    }
}

void FPhysicsScene::RemoveActor(FBodyInstance* Body)
{
    if (mScene && Body && Body->RigidActor)
    {
        // Add나 Remove는 Tick 도중 가능하기 때문에 Lock
        SCOPED_WRITE_LOCK(*mScene)
        mScene->removeActor(*Body->RigidActor);
        Body->RigidActor->userData = nullptr;
    }
}

void FPhysicsScene::AddVehicle(UVehicleComponent* Vehicle)
{
    Vehicles.Add(Vehicle);
}

void FPhysicsScene::RemoveVehicle(UVehicleComponent* Vehicle)
{
    Vehicles.Remove(Vehicle);
}

uint32 FPhysicsScene::GetTotalActorCount() const
{
    if (!mScene) { return 0; }
    SCOPED_READ_LOCK(*mScene)
    return mScene->getNbActors(PxActorTypeFlag::eRIGID_DYNAMIC | PxActorTypeFlag::eRIGID_STATIC);
}

uint32 FPhysicsScene::GetActiveActorCount() const
{
    if (!mScene) { return 0; }
    SCOPED_READ_LOCK(*mScene)
    uint32 ActiveCount = 0;
    mScene->getActiveActors(ActiveCount);
    return ActiveCount;
}

uint32 FPhysicsScene::GetSleepingActorCount() const
{
    if (!mScene) { return 0; }
    SCOPED_READ_LOCK(*mScene)

    uint32 SleepingCount = 0;
    uint32 DynamicCount = mScene->getNbActors(PxActorTypeFlag::eRIGID_DYNAMIC);
    
    if (DynamicCount == 0) { return 0; }

    PxActor** DynamicActors = new PxActor*[DynamicCount];
    mScene->getActors(PxActorTypeFlag::eRIGID_DYNAMIC, DynamicActors, DynamicCount);

    for (uint32 i = 0; i < DynamicCount; ++i)
    {
        if (PxRigidDynamic* RigidDynamic = DynamicActors[i]->is<PxRigidDynamic>())
        {
            if (RigidDynamic->isSleeping())
            {
                SleepingCount++;
            }
        }
    }

    delete[] DynamicActors;
    return SleepingCount;
}
