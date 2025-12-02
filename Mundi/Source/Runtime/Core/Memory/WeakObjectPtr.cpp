#include "pch.h"

#include "WeakObjectPtr.h"

FWeakObjectPtr::FWeakObjectPtr()
    : InternalIndex(INDEX_NONE)
{
}

FWeakObjectPtr::FWeakObjectPtr(UObject* InObject)
    : InternalIndex(INDEX_NONE)
{
    if (InObject)
    {
        InternalIndex = InObject->InternalIndex;
    }
}

UObject* FWeakObjectPtr::Get() const
{
    if (InternalIndex == INDEX_NONE)
    {
        return nullptr;
    }

    if (InternalIndex < GUObjectArray.size())
    {
        return GUObjectArray[InternalIndex];
    }

    return nullptr;
}

bool FWeakObjectPtr::IsValid() const
{
    return Get() != nullptr;
}

/*-----------------------------------------------------------------------------
    연산자 오버로딩
 -----------------------------------------------------------------------------*/

FWeakObjectPtr::operator bool() const
{
    return IsValid();    
}

UObject* FWeakObjectPtr::operator->() const
{
    return Get();
}

UObject& FWeakObjectPtr::operator*() const
{
    return *Get();
}

bool FWeakObjectPtr::operator==(const FWeakObjectPtr& Other) const
{
    return Get() == Other.Get();
}

bool FWeakObjectPtr::operator!=(const FWeakObjectPtr& Other) const
{
    return Get() != Other.Get();
}

bool FWeakObjectPtr::operator==(const UObject* InObject) const
{
    return Get() == InObject;
}

bool FWeakObjectPtr::operator!=(const UObject* InObject) const
{
    return Get() != InObject;
}



