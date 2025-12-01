#include "pch.h"
#include "PhysicsSystem.h"
#include <thread>

FPhysicsSystem::FPhysicsSystem() = default;
FPhysicsSystem::~FPhysicsSystem() = default;

void FPhysicsSystem::Initialize()
{
    mFoundation = PxCreateFoundation(PX_PHYSICS_VERSION, mAllocator, mErrorCallback);
    if (!mFoundation) {
        UE_LOG("[FPhysicsSystem]: PxCreateFoundation Error");
        return;
    }

    mPvd = PxCreatePvd(*mFoundation);
    PxPvdTransport* transport = PxDefaultPvdSocketTransportCreate("127.0.0.1", 5425, 10);
    mPvd->connect(*transport, PxPvdInstrumentationFlag::eALL);

    PxTolerancesScale Scale;
    mPhysics = PxCreatePhysics(PX_PHYSICS_VERSION, *mFoundation, Scale, true, mPvd);
    if (!mPhysics)
    {
        UE_LOG("[FPhysicsSystem]: PxCreatePhysics Error");
        return;
    }

    // Vehicle SDK 초기화
    if (!PxInitVehicleSDK(*mPhysics))
    {
        UE_LOG("[error][FPhysicsSystem]: PxInitVehicleSDK Error");
        return;
    }

    // Extensions 초기화 (Joint, Serialization 등 확장 기능 사용을 위해 필수)
    if (!PxInitExtensions(*mPhysics, mPvd))
    {
        UE_LOG("[FPhysicsSystem]: PxInitExtensions Error");
    }
    
    PxCookingParams Params(Scale);
    Params.meshWeldTolerance = 0.1f; 
    Params.meshPreprocessParams |= PxMeshPreprocessingFlag::eWELD_VERTICES;
    Params.midphaseDesc.setToDefault(PxMeshMidPhase::eBVH34);
    mCooking = PxCreateCooking(PX_PHYSICS_VERSION, *mFoundation, Params);
    if (!mCooking)
    {
        UE_LOG("[FPhysicsSystem]: PxCreateCooking Error");
        return;
    }

    // 전체 논리 프로세서 개수
    uint32 TotalThreads = std::thread::hardware_concurrency();
    
    // 인식 실패 시 안전하게 4코어로 가정 (구형)
    if (TotalThreads == 0) { TotalThreads = 4; }

    // 다른 작업을 위해 비워둘 스레드 개수
    constexpr uint32 ReservedThreads = 4; 
    uint32 WorkerCount;
    if (TotalThreads > ReservedThreads)
    {
        WorkerCount = TotalThreads - ReservedThreads;
    }
    else
    {
        WorkerCount = 1;
    }

    // 스레드가 너무 많으면 스케줄링 및 동기화 비용이 더 큼
    // 우리정도 엔진에서는 4개만 사용
    constexpr uint32 MaxWorkerThreads = 4;
    WorkerCount = std::min(WorkerCount, MaxWorkerThreads);

    mDispatcher = PxDefaultCpuDispatcherCreate(WorkerCount);
    UE_LOG("[FPhysicsSystem] CPU Logic Cores: %d, PhysX Workers Created: %d", TotalThreads, WorkerCount);

    // 기본 재질 생성 (마찰력 0.5, 반발력 0.6)
    mMaterial = mPhysics->createMaterial(0.5f, 0.5f, 0.6f);

    UE_LOG("[FPhysicsSystem] Initialized Successfully");
}

void FPhysicsSystem::Shutdown()
{
    // 생성의 역순으로 해제
    
    // 재질 해제
    if (mMaterial)      mMaterial->release();

    if (mCooking)       mCooking->release();

    // 디스패처 해제
    if (mDispatcher)    mDispatcher->release();

    // Vehicle SDK 해제
    PxCloseVehicleSDK();
    
    // Extensions 해제 (Physics 해제 전에 호출해야 함)
    PxCloseExtensions();

    // Physics 해제
    if (mPhysics)       mPhysics->release();

    // PVD 해제
    if (mPvd)
    {
        PxPvdTransport* transport = mPvd->getTransport();
        mPvd->release();
        if (transport)  transport->release();
    }

    // Foundation 해제 (가장 마지막)
    if (mFoundation)    mFoundation->release();

    UE_LOG("[FPhysicsSystem] Shutdown Complete");
}

void FPhysicsSystem::ReconnectPVD()
{
    if (!mPvd) return;

    if (mPvd->isConnected())
    {
        mPvd->disconnect();
    }

    PxPvdTransport* transport = mPvd->getTransport();
    
    if (!transport || !transport->isConnected())
    {
        if (transport) transport->release();
        transport = PxDefaultPvdSocketTransportCreate("127.0.0.1", 5425, 10);
    }

    if (transport)
    {
        mPvd->connect(*transport, PxPvdInstrumentationFlag::eALL);
    }
}
