#include "pch.h"
#include "PhysicalMaterial.h"
#include "PhysicsSystem.h"
using namespace physx;

void UPhysicalMaterial::CreateMaterial()
{
    if (MatHandle) { return; }

    // PhysX 재질 생성
    MatHandle = FPhysicsSystem::Get().GetPhysics()->createMaterial(StaticFriction, DynamicFriction, Restitution);

    // UserData 연결
    MatHandle->userData = static_cast<void*>(this);

    // 합산 방식 설정 (일단은 평균)
    MatHandle->setFrictionCombineMode(PxCombineMode::eAVERAGE);
    // 반발력은 더 큰 쪽으로
    MatHandle->setRestitutionCombineMode(PxCombineMode::eMAX);
}

void UPhysicalMaterial::Release()
{
    if (MatHandle)
    {
        MatHandle->release();
        MatHandle = nullptr;
    }
}
