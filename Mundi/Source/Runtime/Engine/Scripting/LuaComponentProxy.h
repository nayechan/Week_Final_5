#pragma once

#include <functional>
#include <sol/sol.hpp>

#include "LuaBindingRegistry.h"

struct FBoundProp
{
    const FProperty* Property = nullptr;  // TODO: editable/readonly flags... 
};

struct FBoundClassDesc   // Property list per class
{
    UClass* Class = nullptr;
    TMap<FString, FBoundProp> PropsByName;
};

extern TMap<UClass*, FBoundClassDesc> GBoundClasses;

void BuildBoundClass(UClass* Class);

struct LuaComponentProxy
{
    void* Instance = nullptr;
    UClass* Class = nullptr;

    static sol::object Index(sol::this_state LuaState, LuaComponentProxy& Self, const char* Key);
    static void        NewIndex(LuaComponentProxy& Self, const char* Key, sol::object Obj);
};
