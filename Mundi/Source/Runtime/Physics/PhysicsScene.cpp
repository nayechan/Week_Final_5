#include "pch.h"
#include "PhysicsScene.h"
#include "BodyInstance.h"
#include "PhysicsSystem.h"

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

void FPhysicsScene::Tick(float DeltaTime)
{
    if (!mScene) return;

    // 1. 시뮬레이션 단계 진행 (비동기 시작)
    // DeltaTime이 너무 크면 불안정할 수 있으므로, 보통은 고정 타임스텝(Fixed Timestep)을 쓰거나
    // 최대값을 클램핑(Clamp)해주는 것이 좋음.
    mScene->simulate(DeltaTime);

    // 2. 결과가 나올 때까지 대기 (Blocking)
    // fetchResults(true)는 계산이 끝날 때까지 여기서 멈춰있음.
    // (나중에 최적화하려면 이걸 false로 하고 다른 작업을 하다가 돌아올 수도 있음)
    mScene->fetchResults(true);
}

void FPhysicsScene::Shutdown()
{
    if (mScene)
    {
        // 씬 안에 있는 모든 액터를 강제로 비우고 싶다면 여기서 처리 가능
        // 하지만 보통은 Actor들이 소멸될 때 RemoveActor를 호출하거나
        // Scene release 시 자동 정리됨 (단, Actor 객체 메모리 관리는 별도 주의)
        
        mScene->release();
        mScene = nullptr;
    }
}

void FPhysicsScene::AddActor(FBodyInstance* Body)
{
    if (mScene && Body)
    {
        mScene->addActor(*Body->RigidActor);
    }
}

void FPhysicsScene::RemoveActor(FBodyInstance* Body)
{
    if (mScene && Body)
    {
        mScene->removeActor(*Body->RigidActor);
    }
}