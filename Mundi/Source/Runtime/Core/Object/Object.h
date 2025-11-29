#pragma once
#include "UEContainer.h"
#include "ObjectFactory.h"
#include "MemoryManager.h"
#include "Name.h"
#include "Property.h"
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
    SIZE_T   Size = 0;

    // 리플렉션 시스템 확장
    TArray<FProperty> Properties;              // 프로퍼티 목록
    bool bIsSpawnable = false;                 // ActorSpawnWidget에 표시 여부
    bool bIsComponent = false;                 // 컴포넌트 여부
    const char* DisplayName = nullptr;         // UI 표시 이름
    const char* Description = nullptr;         // 툴팁 설명
    mutable TArray<FProperty> CachedAllProperties;  // GetAllProperties() 캐시 (성능 최적화)
    mutable bool bAllPropertiesCached = false;      // 캐시 유효성 플래그

    constexpr UClass() = default;
    constexpr UClass(const char* n, const UClass* s, SIZE_T z)
        :Name(n), Super(s), Size(z)
    {
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
        static TArray<UClass*> AllClasses;
        return AllClasses;
    }

    static void SignUpClass(UClass* InClass)
    {
        if (InClass)
        {
            GetAllClasses().emplace_back(InClass);
        }
    }
    // const char* 버전 (기본)
    static UClass* FindClass(const char* InClassName)
    {
        for (UClass* Class : GetAllClasses())
        {
            if (Class && Class->Name && strcmp(Class->Name, InClassName) == 0)
            {
                return Class;
            }
        }

        return nullptr;
    }

    // FName 버전 (호환성 유지)
    static UClass* FindClass(const FName& InClassName)
    {
        return FindClass(InClassName.ToString().c_str());
    }

    // 리플렉션 시스템 메서드
    // 주의: 프로퍼티는 static 초기화 시점에만 등록되며, 런타임 중 추가/삭제 불가
    void AddProperty(const FProperty& Property)
    {
        Properties.Add(Property);
        // 프로퍼티가 추가되면 이 클래스와 모든 자식 클래스의 캐시를 무효화
        InvalidateAllPropertiesCache();
    }

private:
    // 이 클래스와 모든 자식 클래스의 프로퍼티 캐시를 무효화
    void InvalidateAllPropertiesCache()
    {
        bAllPropertiesCached = false;
        CachedAllProperties.clear();

        // 모든 자식 클래스의 캐시도 무효화
        for (UClass* DerivedClass : GetAllClasses())
        {
            if (DerivedClass && DerivedClass->IsChildOf(this))
            {
                DerivedClass->bAllPropertiesCached = false;
                DerivedClass->CachedAllProperties.clear();
            }
        }
    }

public:

    const TArray<FProperty>& GetProperties() const
    {
        return Properties;
    }

    // 모든 프로퍼티 가져오기 (부모 클래스 포함, 캐싱됨)
    const TArray<FProperty>& GetAllProperties() const
    {
        if (!bAllPropertiesCached)
        {
            CachedAllProperties.clear();
            if (Super)
            {
                const TArray<FProperty>& ParentProps = Super->GetAllProperties();
                for (const FProperty& Prop : ParentProps)
                {
                    CachedAllProperties.Add(Prop);
                }
            }
            for (const FProperty& Prop : Properties)
            {
                CachedAllProperties.Add(Prop);
            }
            bAllPropertiesCached = true;
        }
        return CachedAllProperties;
    }

    static TArray<UClass*> GetAllSpawnableActors()
    {
        TArray<UClass*> Result;
        for (UClass* Class : GetAllClasses())
        {
            if (Class && Class->bIsSpawnable)
            {
                Result.Add(Class);
            }
        }
        return Result;
    }

    static TArray<UClass*> GetAllComponents()
    {
        TArray<UClass*> Result;
        for (UClass* Class : GetAllClasses())
        {
            if (Class && Class->bIsComponent)
            {
                Result.Add(Class);
            }
        }
        return Result;
    }
};

// ===== UStruct: 구조체 리플렉션 메타데이터 =====
// USTRUCT() 매크로로 선언된 구조체의 런타임 타입 정보를 저장합니다.
//
// UClass와의 차이점:
// - 상속 관계 없음 (Super 포인터 없음)
// - 동적 생성 불가 (ObjectFactory 등록 없음)
// - 런타임 특성 없음 (bIsSpawnable, bIsComponent 등 없음)
// - 순수 데이터 컨테이너로서의 리플렉션만 지원
//
// UClass와의 공통점 (인터페이스 호환성):
// - AddProperty(), GetAllProperties() 등 동일한 메서드 시그니처
// - Properties 배열 동일한 타입
// - BEGIN_STRUCT_PROPERTIES 매크로에서 모든 ADD_PROPERTY 매크로 재사용 가능
//
// 사용 예시:
//   USTRUCT(DisplayName="트랜스폼")
//   struct FTransform {
//       GENERATED_REFLECTION_BODY()
//       UPROPERTY(EditAnywhere) FVector Location;
//   };
struct UStruct
{
	const char* Name = nullptr;
	SIZE_T Size = 0;

	// 리플렉션 시스템
	TArray<FProperty> Properties;
	const char* DisplayName = nullptr;
	const char* Description = nullptr;

	// 동적 배열 조작 함수 포인터 (TArray<ThisStruct> 조작용)
	// PropertyRenderer에서 TArray<Struct>를 타입 안전하게 조작하기 위해 사용
	void (*ArrayAdd)(void* ArrayPtr) = nullptr;           // 배열에 기본 요소 추가
	void (*ArrayRemoveAt)(void* ArrayPtr, int32 Index) = nullptr;  // 인덱스로 요소 삭제
	void (*ArrayInsertAt)(void* ArrayPtr, int32 Index) = nullptr;  // 특정 인덱스에 요소 삽입
	void (*ArrayDuplicateAt)(void* ArrayPtr, int32 Index) = nullptr;  // 특정 인덱스 요소 복제
	int32 (*ArrayNum)(void* ArrayPtr) = nullptr;          // 배열 크기 반환
	void* (*ArrayGetData)(void* ArrayPtr) = nullptr;      // 데이터 포인터 반환
	void (*ArrayClear)(void* ArrayPtr) = nullptr;         // 배열 전체 비우기 (O(1) or O(n))

	constexpr UStruct() = default;
	constexpr UStruct(const char* n, SIZE_T z) : Name(n), Size(z) {}

	// ===== 전역 구조체 레지스트리 =====
	static TArray<UStruct*>& GetAllStructs()
	{
		static TArray<UStruct*> AllStructs;
		return AllStructs;
	}

	static void SignUpStruct(UStruct* InStruct)
	{
		if (InStruct)
		{
			GetAllStructs().emplace_back(InStruct);
		}
	}

	static UStruct* FindStruct(const char* InStructName)
	{
		for (UStruct* Struct : GetAllStructs())
		{
			if (Struct && Struct->Name && strcmp(Struct->Name, InStructName) == 0)
			{
				return Struct;
			}
		}
		return nullptr;
	}

	// ===== 프로퍼티 관리 =====
	// 주의: UClass와 동일한 시그니처 유지 (매크로 재사용을 위해)
	void AddProperty(const FProperty& Property)
	{
		Properties.Add(Property);
	}

	const TArray<FProperty>& GetProperties() const
	{
		return Properties;
	}

	// 구조체는 상속이 없으므로 GetAllProperties()는 GetProperties()와 동일
	const TArray<FProperty>& GetAllProperties() const
	{
		return Properties;
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
    static void* operator new(SIZE_T Size)
    {
        return FMemoryManager::Allocate(Size, alignof(std::max_align_t));
    }
    static void* operator new(SIZE_T Size, std::align_val_t Alignment)
    {
        return FMemoryManager::Allocate(Size, static_cast<size_t>(Alignment));
    }
    static void operator delete(void* Ptr) noexcept
    {
        FMemoryManager::Deallocate(Ptr);
    }
    static void operator delete(void* Ptr, SIZE_T Size) noexcept
    {
        FMemoryManager::Deallocate(Ptr);
    }
    static void operator delete(void* Ptr, std::align_val_t Alignment) noexcept
    {
        FMemoryManager::Deallocate(Ptr);
    }
    static void operator delete(void* Ptr, SIZE_T Size, std::align_val_t Alignment) noexcept
    {
        FMemoryManager::Deallocate(Ptr);
    }

    FString GetName();    // 원문
    FString GetComparisonName(); // lower-case

    // 리플렉션 기반 자동 직렬화 (현재 클래스의 프로퍼티만 처리)
    virtual void Serialize(const bool bInIsLoading, JSON& InOutHandle);
public:
    // GenerateUUID()에 의해 자동 발급
    uint32_t UUID;

    // 팩토리 함수에 의해 자동 발급
    uint32_t InternalIndex;

    FName    ObjectName;   // 이 프로젝트에서는 고유하지 않는 라벨로 사용

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
    virtual void PostDuplicate() {};

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

// Array 직렬화 헬퍼 함수
template<typename T>
static void SerializePrimitiveArray(TArray<T>* ArrayPtr, bool bIsLoading, JSON& ArrayJson)
{
    if (bIsLoading)
    {
        ArrayPtr->clear();
        for (uint32 i = 0; i < static_cast<uint32>(ArrayJson.size()); ++i)
        {
            if constexpr (std::is_same_v<T, int32>)
            {
                ArrayPtr->Add(ArrayJson[i].ToInt());
            }
            else if constexpr (std::is_same_v<T, float>)
            {
                ArrayPtr->Add(static_cast<float>(ArrayJson[i].ToFloat()));
            }
            else if constexpr (std::is_same_v<T, bool>)
            {
                ArrayPtr->Add(ArrayJson[i].ToBool());
            }
            else if constexpr (std::is_same_v<T, FString>)
            {
                ArrayPtr->Add(ArrayJson[i].ToString());
            }
        }
    }
    else // Saving
    {
        for (const T& Element : *ArrayPtr)
        {
            if constexpr (std::is_same_v<T, FString>)
            {
                ArrayJson.append(Element.c_str());
            }
            else
            {
                ArrayJson.append(Element);
            }
        }
    }
}

// ── 파생 타입에 붙일 매크로 ─────────────────────────────────────
#define DECLARE_CLASS(ThisClass, SuperClass)                                  \
public:                                                                       \
    using Super   = SuperClass; /* Unreal 스타일 Super 매크로 대응 */        \
    using ThisClass_t = ThisClass;                                            \
    static UClass* StaticClass()                                              \
    {                                                                         \
        static UClass Cls{ #ThisClass, SuperClass::StaticClass(),             \
                            sizeof(ThisClass) };                              \
        static bool bRegistered = []() {                                      \
            UClass::SignUpClass(&Cls);                                        \
            return true;                                                      \
        }();                                                                  \
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
        NewObject->PostDuplicate();                                           \
        return NewObject;                                                     \
    }


