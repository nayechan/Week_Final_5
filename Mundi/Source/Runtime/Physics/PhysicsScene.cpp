#include "pch.h"
#include "PhysicsScene.h"
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

    // 2. 중력 설정
    // 언리얼(Z-Up) 기준: PxVec3(0.0f, 0.0f, -980.0f) (단위가 cm라면)
    // 일반적인 Y-Up 기준: PxVec3(0.0f, -9.81f, 0.0f)
    // 일단 기본값으로 Y축 중력을 설정 (필요에 따라 변경하세요)
    SceneDesc.gravity = PxVec3(0.0f, -9.81f, 0.0f);

    // 3. ★ 핵심: 멀티스레드 디스패처 공유 ★
    // 시스템이 만들어둔 스레드 풀을 가져와서 씁니다.
    // 이걸 안 넣으면 시뮬레이션이 동작하지 않거나 크래시가 납니다.
    if (PxDefaultCpuDispatcher* Dispatcher = System->GetCpuDispatcher())
    {
        SceneDesc.cpuDispatcher = Dispatcher;
    }
    else
    {
        // 로그: 디스패처가 없음. 싱글 스레드로 돌아가거나 실패할 수 있음.
        return; 
    }

    // 4. 충돌 필터 셰이더 설정
    // 기본 제공되는 셰이더를 쓰거나, 아까 논의한 'CustomSimulationFilterShader'를 넣으면 됨
    SceneDesc.filterShader = PxDefaultSimulationFilterShader;

    // 5. (선택) PVD(Visual Debugger) 연결 설정
    // PVD를 연결해서 보고 싶다면 이 플래그가 필요함
    // if (Physics->getPvd())
    // {
    //     SceneDesc.flags |= PxSceneFlag::eENABLE_PCM; // Persistent Contact Manifold (성능 향상)
    //     SceneDesc.flags |= PxSceneFlag::eENABLE_CCD; // CCD 사용할 거면 켜야 함
    // }

    // 6. 진짜 씬 생성
    mScene = Physics->createScene(SceneDesc);
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