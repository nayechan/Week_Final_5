#pragma once
#include <PxPhysicsAPI.h>

using namespace physx;

class FPhysicsSystem; // 전방 선언

class FPhysicsScene
{
public:
    FPhysicsScene();
    ~FPhysicsScene();

    // 초기화: 시스템에서 Physics랑 Dispatcher를 빌려옴
    void Initialize(FPhysicsSystem* System);
    
    // 매 프레임 시뮬레이션 (원래 System에 있던 거)
    void Tick(float DeltaTime);
    
    // 정리
    void Shutdown();

    // 액터 관리 (이제 Scene이 액터를 가짐)
    void AddActor(FBodyInstance* Body);
    void RemoveActor(FBodyInstance* Body);

    PxScene* GetPxScene() const { return mScene; }

private:
    PxScene* mScene = nullptr;
};