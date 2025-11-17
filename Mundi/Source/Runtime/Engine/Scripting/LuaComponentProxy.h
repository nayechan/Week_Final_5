#pragma once

#include <functional>
#include <sol/sol.hpp>

#include "LuaBindingRegistry.h"

/**
 * Proxy object that bridges C++ component instances to Lua
 * Properties and methods are registered via LuaBindHelpers (AddProperty, AddMethod, etc.)
 */
struct LuaComponentProxy
{
    void* Instance = nullptr;
    UClass* Class = nullptr;

    static sol::object Index(sol::this_state LuaState, LuaComponentProxy& Self, const char* Key);
    static void        NewIndex(LuaComponentProxy& Self, const char* Key, sol::object Obj);
};
