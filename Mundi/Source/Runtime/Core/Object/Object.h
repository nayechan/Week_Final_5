#pragma once
#include "UEContainer.h"
#include "ObjectFactory.h"
#include "MemoryManager.h"
#include "Name.h"
#include "nlohmann/json.hpp"
//#include "GlobalConsole.h"

namespace json { class JSON; }
using JSON = json::JSON;

// 전방 선언/외부 심볼 (네 프로젝트 환경 유지)
class UObject;
class UWorld;
// ── UClass: 간단한 타입 디스크립터 ─────────────────────────────
struct UClass
{
    const char* Name = nullptr;
    const UClass* Super = nullptr;   // 루트(UObject)는 nullptr
    std::size_t   Size = 0;

    constexpr UClass() = default;
    constexpr UClass(const char* n, const UClass* s, std::size_t z)//언리얼도 런타임 시간에 관리해주기 때문에 문제가 없습니다.
        :Name(n), Super(s), Size(z) {
    }
    bool IsChildOf(const UClass* Base) const noexcept
    {
        if (!Base) return false;
        for (auto c = this; c; c = c->Super)
            if (c == Base) return true;
        return false;
    }

    static TArray<UClass*>& GetAllClasses()
    {
        // 이 함수가 최초로 호출될 때 단 한 번만 안전하게 초기화됩니다.
        static TArray<UClass*> AllClasses;
        return AllClasses;
    }

    static void SignUpClass(UClass* InClass)
    {
        if (InClass)
        {
            GetAllClasses().emplace_back(InClass);
            //UE_LOG("UClass: Class registered: %s (Total: %llu)", InClass->Name, GetAllClasses().size());
        }
    }
    static UClass* FindClass(const FName& InClassName)
    {
        for (UClass* Class : GetAllClasses())
        {
            if (Class && Class->Name == InClassName)
            {
                return Class;
            }
        }

        return nullptr;
    }

    
};

class UObject
{
public:
    using ThisClass_t = UObject;

public:
    UObject() : UUID(GenerateUUID()), InternalIndex(UINT32_MAX), ObjectName("UObject") {}
    UObject(const UObject&) = default;

protected:
    virtual ~UObject() = default;
    // Centralized deletion entry accessible to ObjectFactory only
    void DestroyInternal() { delete this; }
    friend void ObjectFactory::DeleteObject(UObject* Obj);

public:
    // UObject-scoped allocation only
    static void* operator new(std::size_t size) { return CMemoryManager::Allocate(size); }
    static void  operator delete(void* ptr) noexcept { CMemoryManager::Deallocate(ptr); }
    static void  operator delete(void* ptr, std::size_t) noexcept { CMemoryManager::Deallocate(ptr); }

    FString GetName();    // 원문
    FString GetComparisonName(); // lower-case

    virtual void Serialize(const bool bInIsLoading, JSON& InOutHandle);
public:
    // GenerateUUID()에 의해 자동 발급
    uint32_t UUID;

    // 팩토리 함수에 의해 자동 발급
    uint32_t InternalIndex;
    FName    ObjectName;   // ← 객체 개별 이름 추가

    // 정적: 타입 메타 반환 (이름을 StaticClass로!)
    static UClass* StaticClass()
    {
        static UClass Cls{ "UObject", nullptr, sizeof(UObject) };
        return &Cls;
    }

    // 가상: 인스턴스의 실제 타입 메타
    virtual UClass* GetClass() const { return StaticClass(); }

    // IsA 헬퍼
    bool IsA(const UClass* C) const noexcept { return GetClass()->IsChildOf(C); }
    template<class T> bool IsA() const noexcept { return IsA(T::StaticClass()); }

    // 다음으로 발급될 UUID를 조회 (증가 없음)
    static uint32 PeekNextUUID() { return GUUIDCounter; }

    // 다음으로 발급될 UUID를 설정 (예: 씬 로드시 메타와 동기화)
    static void SetNextUUID(uint32 Next) { GUUIDCounter = Next; }

    // UUID 발급기: 현재 카운터를 반환하고 1 증가
    static uint32 GenerateUUID() { return GUUIDCounter++; }

    // ───── 복사 관련 ────────────────────────────
    virtual void DuplicateSubObjects(); // Super::DuplicateSubObjects() 호출 -> 얕은 복사한 멤버들에 대해 메뉴얼하게 깊은 복사 수행(특히, Uobject 계열 멤버들에 대해서는 Duplicate() 호출)
    virtual UObject* Duplicate() const; // 자기 자신 깊은 복사(+모든 멤버들 얕은 복사) -> DuplicateSubObjects 호출

    // 자기 자신 깊은 복사(+모든 멤버들 얕은 복사) -> DuplicateSubObjects 호출
    //template<class T>
    //T* Duplicate()
    //{
    //    T* NewObject = ObjectFactory::DuplicateObject<T>(this); // 모든 멤버 얕은 복사

    //    NewObject->DuplicateSubObjects(); // Sub UObject 멤버 있을 경우, 해당 멤버 복사

    //    return NewObject;
    //}

    //virtual ThisClass_t* Duplicate()
    //{
    //    ThisClass_t* NewObject = ObjectFactory::DuplicateObject<ThisClass_t>(this); // 모든 멤버 얕은 복사

    //    NewObject->DuplicateSubObjects(); // Sub UObject 멤버 있을 경우, 해당 멤버 복사

    //    return NewObject;
    //}

private:
    // 전역 UUID 카운터(초기값 1)
    inline static uint32 GUUIDCounter = 1;
};

// ── Cast 헬퍼 (UE Cast<> 와 동일 UX) ────────────────────────────
template<class T>
T* Cast(UObject* Obj) noexcept
{
    return (Obj && Obj->IsA<T>()) ? static_cast<T*>(Obj) : nullptr;
}
template<class T>
const T* Cast(const UObject* Obj) noexcept
{
    return (Obj && Obj->IsA<T>()) ? static_cast<const T*>(Obj) : nullptr;
}

// ── 파생 타입에 붙일 매크로 ─────────────────────────────────────
#define DECLARE_CLASS(ThisClass, SuperClass)                                  \
public:                                                                       \
    using Super_t = SuperClass;                                               \
    using Super   = SuperClass; /* Unreal 스타일 Super 매크로 대응 */        \
    using ThisClass_t = ThisClass;                                            \
    static UClass* StaticClass()                                              \
    {                                                                         \
        static UClass Cls{ #ThisClass, SuperClass::StaticClass(),             \
                            sizeof(ThisClass) };                              \
        UClass::SignUpClass(&Cls);                                              \
        return &Cls;                                                          \
    }                                                                         \
    virtual UClass* GetClass() const override { return ThisClass::StaticClass(); } \

// 각 파생 타입에 맞는 Duplicate() 자동 생성 매크로
#define DECLARE_DUPLICATE(ThisClass)                                          \
public:                                                                       \
    ThisClass(const ThisClass&) = default; /* 디폴트 복사 생성자 명시: 아래 DuplicateObject 호출이 모든 멤버에 대해 단순 = 대입을 수행한다고 가정하기 때문.*/ \
    ThisClass* Duplicate() const override                                         \
    {                                                                         \
        ThisClass* NewObject = ObjectFactory::DuplicateObject<ThisClass>(this); /*모든 멤버 단순 = 대입 수행(즉, 포인터 멤버들은 얕은복사)*/ \
        NewObject->DuplicateSubObjects(); /*메뉴얼한 복사 수행 로직(ex: 포인터 멤버 깊은 복사, 독립적인 값 생성, Uobject계열 Duplicate 재호출)*/ \
        return NewObject;                                                     \
    }


