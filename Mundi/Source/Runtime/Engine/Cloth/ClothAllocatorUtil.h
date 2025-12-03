#pragma once
#include <NvCloth/Cloth.h>
#include <foundation/PxAllocatorCallback.h>
#include <foundation/PxErrorCallback.h>

// Allocator - PhysX의 PxAllocatorCallback 상속
class NvClothAllocator : public physx::PxAllocatorCallback
{
public:
    virtual void* allocate(size_t size, const char* typeName, const char* filename, int line) override
    {
        return malloc(size);
    }

    virtual void deallocate(void* ptr) override
    {
        free(ptr);
    }
};

// Error Callback - PhysX의 PxErrorCallback 상속
class NvClothErrorCallback : public physx::PxErrorCallback
{
public:
    virtual void reportError(physx::PxErrorCode::Enum code, const char* message, const char* file, int line) override
    {
        // 에러 로깅
        //printf("NvCloth Error [%d]: %s at %s:%d\n", code, message, file, line);
    }
};

// Assert Handler - NvCloth의 PxAssertHandler 상속
class NvClothAssertHandler : public nv::cloth::PxAssertHandler
{
public:
    virtual void operator()(const char* exp, const char* file, int line, bool& ignore) override
    {
        //printf("NvCloth Assert: %s at %s:%d\n", exp, file, line);
        ignore = false;
    }
};