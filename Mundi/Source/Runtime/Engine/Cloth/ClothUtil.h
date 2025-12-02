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
#include "DxContextManagerCallbackImpl.h"

using namespace nv::cloth;
using namespace physx;


class ClothUtil
{
public:
	static void Temp();

};