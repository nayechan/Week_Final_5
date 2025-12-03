#include "pch.h"
#include "NvClothEnvironment.h"

// cpp 파일에서
physx::PxAllocatorCallback* NvClothEnvironment::sAllocator = nullptr;
physx::PxErrorCallback* NvClothEnvironment::sErrorCallback = nullptr;
nv::cloth::PxAssertHandler* NvClothEnvironment::sAssertHandler = nullptr;