#pragma once

using namespace physx;

class FPhysXSimEventCallback : public PxSimulationEventCallback
{
public:
    // 조인트에 설정해둔 Break Force 이상의 충격으로 인해 조인트가 끊어졌을 때
    void onConstraintBreak(PxConstraintInfo* Constraints, PxU32 Count) override {}
    // 물리적으로 Sleep 모드에 있다가 깨어날 때
    void onWake(PxActor** Actors, PxU32 Count) override {}
    // 물리적으로 Sleep 모드로 들어갈 때
    void onSleep(PxActor** Actors, PxU32 Count) override {}
    // 두 물체가 물리적으로 부딪혔을 때 (Blocking Collision)
    void onContact(const PxContactPairHeader& PairHeader, const PxContactPair* Pairs, PxU32 NbPairs) override;
    // 물체 중 하나가 Trigger일 때 Overlap되었을 때
    void onTrigger(PxTriggerPair* Pairs, PxU32 Count) override;
    // 시뮬레이션 도중에 미리 변경된 Transform을 보고 싶을 때..? 초고급기능이라 잘 안씀
    void onAdvance(const PxRigidBody* const* BodyBuffer, const PxTransform* PoseBuffer, const PxU32 Count) override {}

private:
    UPrimitiveComponent* GetCompFromPxActor(physx::PxRigidActor* InActor);
    FName GetBoneNameFromShape(const physx::PxShape* Shape);
};
