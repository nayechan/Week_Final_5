#pragma once
#include "d3d11.h"
#include <NvCloth/Factory.h>
#include <NvCloth/Solver.h>
#include <NvCloth/Cloth.h>
#include <NvCloth/Fabric.h>
#include <foundation/PxVec3.h>
#include <foundation/PxVec4.h>
#include <NvCloth/extensions/ClothFabricCooker.h> 
#include <NvCloth/DxContextManagerCallback.h>

using namespace nv::cloth;
using namespace physx;

class ClothUtil
{
public:
	static void CopySettings(Cloth* Source, Cloth* Target)
    {
        if (!Source || !Target) return;

        // ===== Stiffness 설정 =====
        // PhaseConfig 직접 생성
        Fabric& Fabric = Source->getFabric();
        int numPhases = Fabric.getNumPhases();
        std::vector<nv::cloth::PhaseConfig> phaseConfigs(numPhases);

        for (int i = 0; i < numPhases; i++)
        {
            phaseConfigs[i].mPhaseIndex = i;
            phaseConfigs[i].mStiffness = 0.7f;           // 0.0 ~ 1.0
            phaseConfigs[i].mStiffnessMultiplier = 1.0f;
            phaseConfigs[i].mCompressionLimit = 1.0f;
            phaseConfigs[i].mStretchLimit = 1.3f;        // 늘어남 제한
        }

        nv::cloth::Range<nv::cloth::PhaseConfig> range(phaseConfigs.data(), phaseConfigs.data() + numPhases);
        Target->setPhaseConfig(range);
        // 기본 물리 설정
        Target->setGravity(Source->getGravity());
        Target->setDamping(Source->getDamping());
        Target->setLinearDrag(Source->getLinearDrag());
        Target->setAngularDrag(Source->getAngularDrag());
        Target->setLinearInertia(Source->getLinearInertia());
        Target->setAngularInertia(Source->getAngularInertia());
        Target->setCentrifugalInertia(Source->getCentrifugalInertia());
        Target->setFriction(Source->getFriction());
        Target->setCollisionMassScale(Source->getCollisionMassScale());

        // Solver 설정
        Target->setSolverFrequency(Source->getSolverFrequency());
        Target->setStiffnessFrequency(Source->getStiffnessFrequency());

        // Constraint 설정
        Target->setTetherConstraintScale(Source->getTetherConstraintScale());
        Target->setTetherConstraintStiffness(Source->getTetherConstraintStiffness());

        // Self Collision
        Target->setSelfCollisionDistance(Source->getSelfCollisionDistance());
        Target->setSelfCollisionStiffness(Source->getSelfCollisionStiffness());

        // Wind 설정
        Target->setWindVelocity(Source->getWindVelocity());
        Target->setDragCoefficient(0.2f);   // wind drag
        Target->setLiftCoefficient(0.1f);   // upward force due to wind

        // Sleep 설정
        Target->setSleepThreshold(Source->getSleepThreshold());
    }


};

inline PxVec3 ToPxVec(const FVector& Vt3)
{
    return PxVec3(Vt3.X, Vt3.Y, Vt3.Z);
}