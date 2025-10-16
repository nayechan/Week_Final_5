#include "pch.h"

FString UObject::GetName()
{
    return ObjectName.ToString();
}

FString UObject::GetComparisonName()
{
    return FString();
}

void UObject::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{

}


void UObject::DuplicateSubObjects()
{
    UUID = GenerateUUID(); // UUID는 고유값이므로 새로 생성
}

UObject* UObject::Duplicate() const
{
    UObject* NewObject = ObjectFactory::DuplicateObject<UObject>(this); // 모든 멤버 얕은 복사

    NewObject->DuplicateSubObjects(); // 얕은 복사한 멤버들에 대해 메뉴얼하게 깊은 복사 수행

    return NewObject;
}


