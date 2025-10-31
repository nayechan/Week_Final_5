#include "pch.h"
#include "Name.h"

namespace
{
    // NameMap을 안전하게 가져오는 getter
    static TMap<FString, uint32>& GetNameMap()
    {
        // 함수 내의 static 변수는 처음 호출될 때 스레드에 안전하게
        // 단 한 번만 초기화됩니다.
        static TMap<FString, uint32> GNameMap;
        return GNameMap;
    }

    // Entries를 안전하게 가져오는 getter
    static TArray<FNameEntry>& GetEntries()
    {
        static TArray<FNameEntry> GEntries;
        return GEntries;
    }
}

uint32 FNamePool::Add(const FString& InStr)
{
    FString Lower = InStr;
    std::transform(Lower.begin(), Lower.end(), Lower.begin(), ::tolower);

    // 전역 변수 대신 getter를 통해 접근
    TMap<FString, uint32>& NameMap = GetNameMap();
    TArray<FNameEntry>& Entries = GetEntries();

    auto it = NameMap.find(Lower);
    if (it != NameMap.end())
        return it->second;

    uint32 NewIndex = (uint32)Entries.size();
    Entries.push_back({ InStr, Lower });
    NameMap[Lower] = NewIndex;
    return NewIndex;
}

const FNameEntry& FNamePool::Get(uint32 Index)
{
    // 전역 변수 대신 getter를 통해 접근
    TArray<FNameEntry>& Entries = GetEntries();

    // (안전성 강화) 경계 검사 추가
    if (Index >= Entries.size())
    {
        static FNameEntry InvalidEntry = { "Invalid", "invalid" };
        return InvalidEntry;
    }
    return Entries[Index];
}