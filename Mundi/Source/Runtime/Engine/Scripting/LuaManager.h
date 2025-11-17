#pragma once
#include "LuaCoroutineScheduler.h"
#include <sol/sol.hpp>

namespace sol { class state; }
using state = sol::state;

class FLuaManager
{
public:
    FLuaManager();
    ~FLuaManager();

    sol::state& GetState() { return *Lua; }
    sol::environment CreateEnvironment();

    void RegisterComponentProxy(sol::state& Lua);
    void ExposeAllComponentsToLua();
    void ExposeGlobalFunctions();

    bool LoadScriptInto(sol::environment& Env, const FString& Path);
    
    // Env 테이블에서 Name(함수 이름) 키를 조회해서 함수로 캐스팅
    static sol::protected_function GetFunc(sol::environment& Env, const char* Name);
    
    void Tick(double DeltaSeconds);            // 내부에서 누적 TotalTime 관리
    void ShutdownBeforeLuaClose();             // 코루틴 abandon -> Tasks 비우기
    
    class FLuaCoroutineScheduler& GetScheduler() { return CoroutineSchedular; }

private:
    sol::state* Lua = nullptr;
    sol::table SharedLib;                         // 공용 유틸 테이블

    FLuaCoroutineScheduler CoroutineSchedular;    // 씬 단위 Coroutine Manager
};

// Helper function to wrap C++ object pointers in LuaComponentProxy for Lua
sol::object MakeCompProxy(sol::state_view SolState, void* Instance, UClass* Class);
