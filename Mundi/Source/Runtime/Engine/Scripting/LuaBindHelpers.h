#pragma once
#include "LuaManager.h"
#include "LuaComponentProxy.h"

// 클래스별 바인더 등록 매크로(컴포넌트 cpp에서 사용)
#define LUA_BIND_BEGIN(ClassType) \
static void BuildLua_##ClassType(sol::state_view L, sol::table& T); \
struct FLuaBinder_##ClassType { \
FLuaBinder_##ClassType() { \
FLuaBindRegistry::Get().Register(ClassType::StaticClass(), &BuildLua_##ClassType); \
} \
}; \
static FLuaBinder_##ClassType G_LuaBinder_##ClassType; \
static void BuildLua_##ClassType(sol::state_view L, sol::table& T) \
/**/

#define LUA_BIND_END() /* nothing */

// 멤버 함수 포인터를 Lua 함수로 감싸는 헬퍼(리턴 void 버전)
template<typename C, typename... P>
static void AddMethod(sol::table& T, const char* Name, void(C::*Method)(P...))
{
    T.set_function(Name, [Method](LuaComponentProxy& Proxy, P... Args)
    {
        if (!Proxy.Instance || Proxy.Class != C::StaticClass()) return;
        (static_cast<C*>(Proxy.Instance)->*Method)(std::forward<P>(Args)...);
    });
}

// 리턴값이 있는 멤버 함수용 버전
template<typename R, typename C, typename... P>
static void AddMethodR(sol::table& T, const char* Name, R(C::*Method)(P...))
{
    T.set_function(Name, [Method](LuaComponentProxy& Proxy, P... Args) -> R
    {
        if (!Proxy.Instance || Proxy.Class != C::StaticClass())
        {
            if constexpr (!std::is_void_v<R>) return R{};
        }
        return (static_cast<C*>(Proxy.Instance)->*Method)(std::forward<P>(Args)...);
    });
}

// const 멤버 함수용 오버로드
template<typename R, typename C, typename... P>
static void AddMethodR(sol::table& T, const char* Name, R(C::*Method)(P...) const)
{
    T.set_function(Name, [Method](LuaComponentProxy& Proxy, P... Args) -> R
    {
        if (!Proxy.Instance || Proxy.Class != C::StaticClass())
        {
            if constexpr (!std::is_void_v<R>) return R{};
        }
        return (static_cast<const C*>(Proxy.Instance)->*Method)(std::forward<P>(Args)...);
    });
}

// 친절한 별칭 부여용
template<typename C, typename... P>
static void AddAlias(sol::table& T, const char* Alias, void(C::*Method)(P...))
{
    AddMethod<C, P...>(T, Alias, Method);
}

// ===== 프로퍼티 바인딩 헬퍼 함수들 =====

// 기본 프로퍼티 바인딩 (읽기/쓰기)
template<typename C, typename PropType>
static void AddProperty(sol::table& T, const char* Name, PropType C::*MemberPtr)
{
    sol::state_view L = T.lua_state();
    sol::table PropDesc = L.create_table();

    PropDesc["is_property"] = true;
    PropDesc["read_only"] = false;

    // Getter
    PropDesc["get"] = [MemberPtr](LuaComponentProxy& Proxy) -> PropType
    {
        if (!Proxy.Instance || Proxy.Class != C::StaticClass())
            return PropType{};
        return static_cast<C*>(Proxy.Instance)->*MemberPtr;
    };

    // Setter
    PropDesc["set"] = [MemberPtr](LuaComponentProxy& Proxy, PropType Value)
    {
        if (!Proxy.Instance || Proxy.Class != C::StaticClass())
            return;
        static_cast<C*>(Proxy.Instance)->*MemberPtr = Value;
    };

    T[Name] = PropDesc;
}

// 읽기 전용 프로퍼티 바인딩
template<typename C, typename PropType>
static void AddReadOnlyProperty(sol::table& T, const char* Name, PropType C::*MemberPtr)
{
    sol::state_view L = T.lua_state();
    sol::table PropDesc = L.create_table();

    PropDesc["is_property"] = true;
    PropDesc["read_only"] = true;

    // Getter만 제공
    PropDesc["get"] = [MemberPtr](LuaComponentProxy& Proxy) -> PropType
    {
        if (!Proxy.Instance || Proxy.Class != C::StaticClass())
            return PropType{};
        return static_cast<C*>(Proxy.Instance)->*MemberPtr;
    };

    T[Name] = PropDesc;
}

// 포인터 타입 프로퍼티 바인딩 (UObject* 등)
template<typename C, typename PtrType>
static void AddPropertyPtr(sol::table& T, const char* Name, PtrType* C::*MemberPtr)
{
    sol::state_view L = T.lua_state();
    sol::table PropDesc = L.create_table();

    PropDesc["is_property"] = true;
    PropDesc["read_only"] = false;

    // Getter - 포인터를 그대로 반환
    PropDesc["get"] = [MemberPtr](LuaComponentProxy& Proxy) -> PtrType*
    {
        if (!Proxy.Instance || Proxy.Class != C::StaticClass())
            return nullptr;
        return static_cast<C*>(Proxy.Instance)->*MemberPtr;
    };

    // Setter
    PropDesc["set"] = [MemberPtr](LuaComponentProxy& Proxy, PtrType* Value)
    {
        if (!Proxy.Instance || Proxy.Class != C::StaticClass())
            return;
        static_cast<C*>(Proxy.Instance)->*MemberPtr = Value;
    };

    T[Name] = PropDesc;
}

// 배열 포인터 타입 프로퍼티 바인딩 (TArray<T*>)
template<typename C, typename ElemType>
static void AddPropertyArrayPtr(sol::table& T, const char* Name, TArray<ElemType*> C::*MemberPtr)
{
    sol::state_view L = T.lua_state();
    sol::table PropDesc = L.create_table();

    PropDesc["is_property"] = true;
    PropDesc["read_only"] = false;

    // Getter - TArray를 sol::table로 변환
    PropDesc["get"] = [MemberPtr](sol::this_state s, LuaComponentProxy& Proxy) -> sol::object
    {
        if (!Proxy.Instance || Proxy.Class != C::StaticClass())
            return sol::nil;

        sol::state_view L(s);
        TArray<ElemType*>& Arr = static_cast<C*>(Proxy.Instance)->*MemberPtr;

        sol::table Result = L.create_table();
        for (int32 i = 0; i < Arr.Num(); ++i)
        {
            Result[i + 1] = Arr[i]; // Lua는 1-based 인덱스
        }
        return Result;
    };

    // Setter - sol::table을 TArray로 변환
    PropDesc["set"] = [MemberPtr](LuaComponentProxy& Proxy, sol::table LuaArray)
    {
        if (!Proxy.Instance || Proxy.Class != C::StaticClass())
            return;

        TArray<ElemType*>& Arr = static_cast<C*>(Proxy.Instance)->*MemberPtr;
        Arr.Empty();

        // Lua 테이블을 순회하며 배열 채우기
        for (auto& Pair : LuaArray)
        {
            if (Pair.second.is<ElemType*>())
            {
                Arr.Add(Pair.second.as<ElemType*>());
            }
        }
    };

    T[Name] = PropDesc;
}
