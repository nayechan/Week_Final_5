#include "pch.h"
#include "LuaComponentProxy.h"

sol::object LuaComponentProxy::Index(sol::this_state LuaState, LuaComponentProxy& Self, const char* Key)
{
    if (!Self.Instance)
    {
        UE_LOG("[LuaProxy] Index: Instance is null for key '%s'\n", Key);
        return sol::nil;
    }

    sol::state_view LuaView(LuaState);

    // 이 클래스에 등록된 Lua 테이블 가져오기 (프로퍼티와 메서드 모두 포함)
    sol::table& BindTable = FLuaBindRegistry::Get().EnsureTable(LuaView, Self.Class);

    if (!BindTable.valid())
    {
        UE_LOG("[LuaProxy] Index: BindTable invalid for class %p, key '%s'\n",
            Self.Class, Key);
        return sol::nil;
    }

    sol::object Result = BindTable[Key];

    if (!Result.valid())
    {
        UE_LOG("[LuaProxy] Index: Key '%s' not found in BindTable for class %p\n",
            Key, Self.Class);
        return sol::nil;
    }

    // 프로퍼티 descriptor 테이블인지 확인
    if (Result.is<sol::table>())
    {
        sol::table propDesc = Result.as<sol::table>();
        sol::optional<bool> isProperty = propDesc["is_property"];

        if (isProperty && *isProperty)
        {
            UE_LOG("[LuaProxy] Index: Property '%s' found for class %p\n",
                Key, Self.Class);

            // 프로퍼티 - getter 함수 호출
            sol::object getterObj = propDesc["get"];
            if (getterObj.valid())
            {
                sol::protected_function getter = getterObj.as<sol::protected_function>();
                auto pfr = getter(Self);

                if (!pfr.valid())
                {
                    sol::error err = pfr;
                    UE_LOG("[LuaProxy] Index: Property '%s' getter error: %s\n", Key, err.what());
                    return sol::nil;
                }

                UE_LOG("[LuaProxy] Index: Property '%s' getter succeeded\n", Key);
                // 첫 번째 반환값을 sol::object로 가져오기
                return pfr.get<sol::object>();
            }
            else
            {
                UE_LOG("[LuaProxy] Index: Property '%s' has no getter\n", Key);
            }
            return sol::nil;
        }
    }

    // 함수면 그대로 반환 (메서드용)
    UE_LOG("[LuaProxy] Index: Returning method '%s' for class %p\n",
        Key, Self.Class);
    return Result;
}

void LuaComponentProxy::NewIndex(LuaComponentProxy& Self, const char* Key, sol::object Obj)
{
    if (!Self.Instance || !Self.Class) return;

    sol::state_view LuaView = Obj.lua_state();
    sol::table& BindTable = FLuaBindRegistry::Get().EnsureTable(LuaView, Self.Class);

    if (!BindTable.valid()) return;

    sol::object Property = BindTable[Key];

    if (!Property.valid())
        return; // 프로퍼티가 존재하지 않음

    // 프로퍼티 descriptor 테이블인지 확인
    if (Property.is<sol::table>())
    {
        sol::table propDesc = Property.as<sol::table>();
        sol::optional<bool> isProperty = propDesc["is_property"];

        if (isProperty && *isProperty)
        {
            // 읽기 전용인지 확인
            sol::optional<bool> readOnly = propDesc["read_only"];
            if (readOnly && *readOnly)
            {
                UE_LOG("[LuaProxy] Attempted to set read-only property: %s\n", Key);
                return;
            }

            // 쓰기 가능한 프로퍼티 - setter 호출
            sol::optional<sol::function> setter = propDesc["set"];
            if (setter)
            {
                auto result = (*setter)(Self, Obj);
                if (!result.valid())
                {
                    sol::error err = result;
                    UE_LOG("[LuaProxy] Error setting property %s: %s\n", Key, err.what());
                }
            }
            return;
        }
    }

    // 프로퍼티가 아니면 아무것도 하지 않음 (메서드는 설정 불가)
}
