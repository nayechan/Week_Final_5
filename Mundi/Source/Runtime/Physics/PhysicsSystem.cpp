#include "pch.h"
#include "PhysicsSystem.h"

// 싱글톤 인스턴스 반환
FPhysicsSystem& FPhysicsSystem::Get()
{
    static FPhysicsSystem instance;
    return instance;
}

FPhysicsSystem::FPhysicsSystem() = default;
FPhysicsSystem::~FPhysicsSystem() = default;

void FPhysicsSystem::CreateTestObjects()
{
    if (!mPhysics || !mScene) return;

    // 1. 바닥 만들기 (Static Plane)
    // 노멀(0,1,0), 거리 0 -> XZ 평면
    PxRigidStatic* ground = PxCreatePlane(*mPhysics, PxPlane(0, 1, 0, 0), *mMaterial);
    mScene->addActor(*ground);

    // 2. 박스 만들기 (Dynamic RigidBody)
    // 공중 10미터 위 (0, 10, 0)
    PxTransform t(PxVec3(0, 10, 0));
    PxRigidDynamic* box = mPhysics->createRigidDynamic(t);
    
    // 모양: 가로세로 1.0 박스
    PxShape* shape = mPhysics->createShape(PxBoxGeometry(1.0f, 1.0f, 1.0f), *mMaterial);
    box->attachShape(*shape);
    shape->release(); // 연결했으니 포인터 해제

    // 질량 계산 (이거 안하면 물리 이상하게 튐)
    PxRigidBodyExt::updateMassAndInertia(*box, 10.0f);

    // 씬에 추가 (★이걸 해야 PVD에 보임)
    mScene->addActor(*box);
}

void FPhysicsSystem::Initialize()
{
    // Foundation 생성
    mFoundation = PxCreateFoundation(PX_PHYSICS_VERSION, mAllocator, mErrorCallback);
    if (!mFoundation) {
        UE_LOG("[FPhysicsSystem]: PxCreateFoundation Error");
        return;
    }

    // PVD 연결
    mPvd = PxCreatePvd(*mFoundation);
    PxPvdTransport* transport = PxDefaultPvdSocketTransportCreate("127.0.0.1", 5425, 10);
    mPvd->connect(*transport, PxPvdInstrumentationFlag::eALL);

    // Physics 메인 객체 생성
    mPhysics = PxCreatePhysics(PX_PHYSICS_VERSION, *mFoundation, PxTolerancesScale(), true, mPvd);

    // Scene(물리 월드) 설정
    PxSceneDesc sceneDesc(mPhysics->getTolerancesScale());
    sceneDesc.gravity = PxVec3(0.0f, -9.81f, 0.0f); // 중력: Y축 아래로
    
    // CPU 쓰레드 디스패처 (2개의 작업 스레드 사용)
    mDispatcher = PxDefaultCpuDispatcherCreate(2);
    sceneDesc.cpuDispatcher = mDispatcher;
    sceneDesc.filterShader = PxDefaultSimulationFilterShader; // 기본 충돌 필터

    // 성능 및 디버깅 플래그
    sceneDesc.flags |= PxSceneFlag::eENABLE_PCM; // 정확도 향상
    sceneDesc.flags |= PxSceneFlag::eENABLE_CCD; // 총알 같은 빠른 물체 뚫림 방지

    // Scene 생성
    mScene = mPhysics->createScene(sceneDesc);

    // PVD에 씬 정보 보내기
    PxPvdSceneClient* pvdClient = mScene->getScenePvdClient();
    if (pvdClient)
    {
        pvdClient->setScenePvdFlag(PxPvdSceneFlag::eTRANSMIT_CONSTRAINTS, true);
        pvdClient->setScenePvdFlag(PxPvdSceneFlag::eTRANSMIT_SCENEQUERIES, true);
        pvdClient->setScenePvdFlag(PxPvdSceneFlag::eTRANSMIT_CONTACTS, true);
    }

    // 기본 재질 생성 (마찰력 0.5, 반발력 0.5)
    mMaterial = mPhysics->createMaterial(0.5f, 0.5f, 0.6f);

    if (mScene->getScenePvdClient()) {
        mScene->getScenePvdClient()->setScenePvdFlag(PxPvdSceneFlag::eTRANSMIT_SCENEQUERIES, true);
        mScene->getScenePvdClient()->setScenePvdFlag(PxPvdSceneFlag::eTRANSMIT_CONTACTS, true);
        mScene->getScenePvdClient()->setScenePvdFlag(PxPvdSceneFlag::eTRANSMIT_CONSTRAINTS, true);
    }

    // FOR TEST
    CreateTestObjects();
}

void FPhysicsSystem::UpdateSimulation(float DeltaTime)
{
    if (!mScene) return;

    // 시뮬레이션 시작
    mScene->simulate(DeltaTime);

    // 결과 대기 (true = 끝날 때까지 멈춤. 간단한 구현용)
    // 나중에는 이걸 false로 하고 렌더링 중에 물리 계산 돌리는 최적화
    mScene->fetchResults(true);
}

void FPhysicsSystem::Shutdown()
{
    // 생성의 역순으로 해제    
    if (mScene)         mScene->release();
    if (mDispatcher)    mDispatcher->release();
    if (mPhysics)       mPhysics->release();
    
    if (mPvd) 
    {
        PxPvdTransport* transport = mPvd->getTransport();
        mPvd->release();
        transport->release();
    }
    
    if (mFoundation)    mFoundation->release();
}