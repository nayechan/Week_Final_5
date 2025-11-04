#include "pch.h"
#include "MemoryManager.h"
#include <cstddef>
#include <malloc.h>
#include <algorithm>

uint32 FMemoryManager::TotalAllocationBytes = 0;
uint32 FMemoryManager::TotalAllocationCount = 0;

void* FMemoryManager::Allocate(SIZE_T Size, SIZE_T Alignment)
{
	SIZE_T TotalSize = Size + sizeof(SIZE_T);
	SIZE_T FinalAlignment = std::max(Alignment, alignof(SIZE_T));

#if defined(_MSC_VER) && defined(_DEBUG)
	void* Raw = _aligned_malloc_dbg(TotalSize, FinalAlignment, nullptr, 0);
#else
	void* Raw = _aligned_malloc(TotalSize, FinalAlignment);
#endif
	if (!Raw)
		return nullptr;

	*reinterpret_cast<SIZE_T*>(Raw) = Size;
	TotalAllocationBytes += static_cast<uint32>(Size);
	TotalAllocationCount++;

	return static_cast<void*>(static_cast<unsigned char*>(Raw) + sizeof(SIZE_T));
}

void FMemoryManager::Deallocate(void* Ptr)
{
	if (!Ptr)
		return;

	unsigned char* UserPtr = static_cast<unsigned char*>(Ptr);
	unsigned char* Raw = UserPtr - sizeof(SIZE_T);
	SIZE_T Size = *reinterpret_cast<SIZE_T*>(Raw);

	TotalAllocationBytes -= static_cast<uint32>(Size);
	TotalAllocationCount--;

#if defined(_MSC_VER) && defined(_DEBUG)
	_aligned_free_dbg(Raw);
#else
	_aligned_free(Raw);
#endif
}