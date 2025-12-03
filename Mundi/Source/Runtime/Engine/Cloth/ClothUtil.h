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
        //Target->setWindDrag(Source->getWindDrag());
        //Target->setWindLift(Source->getWindLift());

        // Sleep 설정
        Target->setSleepThreshold(Source->getSleepThreshold());
    }


};
