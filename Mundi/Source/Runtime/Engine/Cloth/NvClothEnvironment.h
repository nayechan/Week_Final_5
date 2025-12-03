#pragma once
#include "ClothAllocatorUtil.h"

class NvClothEnvironment
{
public:
    static bool AllocateEnv()
    {
        if (sAllocator == nullptr)
        {
            sAllocator = new NvClothAllocator();
        }
        if (sErrorCallback == nullptr)
        {
            sErrorCallback = new NvClothErrorCallback();
        }
        if (sAssertHandler == nullptr)
        {
            sAssertHandler = new NvClothAssertHandler();
        }
        return sAllocator && sErrorCallback && sAssertHandler;
    }

    static void FreeEnv()
    {
        delete sAllocator;
        delete sErrorCallback;
        delete sAssertHandler;
        sAllocator = nullptr;
        sErrorCallback = nullptr;
        sAssertHandler = nullptr;
    }

    static physx::PxAllocatorCallback* GetAllocator() { return sAllocator; }
    static physx::PxErrorCallback* GetErrorCallback() { return sErrorCallback; }
    static nv::cloth::PxAssertHandler* GetAssertHandler() { return sAssertHandler; }

private:
    static physx::PxAllocatorCallback* sAllocator;
    static physx::PxErrorCallback* sErrorCallback;
    static nv::cloth::PxAssertHandler* sAssertHandler;
};
