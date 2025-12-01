// FPhysicsSystem.h
#pragma once
#include <PxPhysicsAPI.h>

using namespace physx;

class FPhysicsSystem
{
public:
    FPhysicsSystem();  // public 생성자
    ~FPhysicsSystem(); // public 소멸자

    void Initialize();
    void Shutdown();
    void ReconnectPVD();

    // 공용 자원 접근자 (Getter)
    PxPhysics* GetPhysics() const { return mPhysics; }
    PxDefaultCpuDispatcher* GetCpuDispatcher() const { return mDispatcher; }
    PxMaterial* GetDefaultMaterial() const { return mMaterial; }

private:
    // 복사 방지는 여전히 해두는 게 안전함
    FPhysicsSystem(const FPhysicsSystem&) = delete;
    FPhysicsSystem& operator=(const FPhysicsSystem&) = delete;

private:
    PxDefaultAllocator      mAllocator;
    PxDefaultErrorCallback  mErrorCallback;
    
    PxFoundation* mFoundation = nullptr;
    PxPhysics* mPhysics = nullptr;
    PxDefaultCpuDispatcher* mDispatcher = nullptr;
    PxMaterial* mMaterial = nullptr;
    PxPvd* mPvd = nullptr;
};