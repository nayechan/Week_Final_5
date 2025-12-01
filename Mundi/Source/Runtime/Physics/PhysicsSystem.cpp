#include "pch.h"
#include "PhysicsSystem.h" // 헤더 파일명 확인 필요

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

    mPhysics = PxCreatePhysics(PX_PHYSICS_VERSION, *mFoundation, PxTolerancesScale(), true, mPvd);
    if (!mPhysics) {
        UE_LOG("[FPhysicsSystem]: PxCreatePhysics Error");
        return;
    }

    // 2개의 워커 스레드를 생성하여 모든 씬이 공유함
    mDispatcher = PxDefaultCpuDispatcherCreate(2);

    // 기본 재질 생성 (마찰력 0.5, 반발력 0.6)
    mMaterial = mPhysics->createMaterial(0.5f, 0.5f, 0.6f);

    UE_LOG("[FPhysicsSystem] Initialized Successfully");
}

void FPhysicsSystem::Shutdown()
{
    // 생성의 역순으로 해제
    
    // 재질 해제
    if (mMaterial)      mMaterial->release();

    // 디스패처 해제
    if (mDispatcher)    mDispatcher->release();

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

    if (mPvd->isConnected()) { mPvd->disconnect(); }

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
