#include "pch.h"
#include "PhysicsScene.h"
#include "BodyInstance.h"
#include "PhysicsSystem.h"
#include "PlatformTime.h"
#include "PrimitiveComponent.h"

#define SCOPED_READ_LOCK(Scene) PxSceneReadLock ScopedReadLock(Scene);
#define SCOPED_WRITE_LOCK(Scene) PxSceneWriteLock ScopedWriteLock(Scene);

FPhysicsScene::FPhysicsScene() : mScene(nullptr) {}
FPhysicsScene::~FPhysicsScene()
{
    Shutdown();
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

    SceneDesc.filterShader = PxDefaultSimulationFilterShader;
    SceneDesc.flags |= PxSceneFlag::eENABLE_ACTIVE_ACTORS; // 활성화된 액터 목록만 가져올 수 있게
    SceneDesc.flags |= PxSceneFlag::eENABLE_CCD; // CCD 허용
    SceneDesc.flags |= PxSceneFlag::eENABLE_PCM; // 충돌 접점 관리...?
    
    mScene = Physics->createScene(SceneDesc);
    PxPvdSceneClient* PvdClient = mScene->getScenePvdClient();
    if (PvdClient)
    {
        // 제약 조건(Joint 등) 보이기
        PvdClient->setScenePvdFlag(PxPvdSceneFlag::eTRANSMIT_CONSTRAINTS, true);
        
        // 씬 쿼리(Raycast 등) 보이기
        PvdClient->setScenePvdFlag(PxPvdSceneFlag::eTRANSMIT_SCENEQUERIES, true);
        
        // 충돌 접점(Contact Point) 보이기
        PvdClient->setScenePvdFlag(PxPvdSceneFlag::eTRANSMIT_CONTACTS, true);
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

void FPhysicsScene::Simulation(float DeltaTime)
{
    // 비동기 시뮬레이션
    mScene->simulate(DeltaTime);
}

void FPhysicsScene::FetchAndSync()
{
    {
        TIME_PROFILE(PhysicsSyncTime)
        // 시뮬레이션 완료될 때까지 기다림
        mScene->fetchResults(true);
    }

    {
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
    if (mScene && Body)
    {
        // Add나 Remove는 Tick 도중 가능하기 때문에 Lock
        SCOPED_WRITE_LOCK(*mScene)
        mScene->removeActor(*Body->RigidActor);
        Body->RigidActor->userData = nullptr;
    }
}