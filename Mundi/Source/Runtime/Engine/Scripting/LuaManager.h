#pragma once
#include "LuaCoroutineScheduler.h"
#include <sol/sol.hpp>

namespace sol { class state; }
using state = sol::state;

class FGameObject;

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

    // ═══════════════════════════════════════════════════════════════════════════
    // 지연 삭제 시스템 (FGameObject 메모리 누수 방지)
    // ═══════════════════════════════════════════════════════════════════════════

    /** 파괴된 FGameObject를 삭제 큐에 추가 (Actor::EndPlay에서 호출) */
    void QueueGameObjectForDestruction(FGameObject* GameObject);

    /** 삭제 큐를 비우고 좀비 리스트로 이동 (Tick 끝에서 호출) */
    void FlushPendingDestroyGameObjects();

private:
    sol::state* Lua = nullptr;
    sol::table SharedLib;                         // 공용 유틸 테이블

    FLuaCoroutineScheduler CoroutineSchedular;    // 씬 단위 Coroutine Manager

    /** 다음 프레임에서 처리될 FGameObject 목록 */
    TArray<FGameObject*> PendingDestroyGameObjects;

    /** 파괴되었지만 Lua 참조 가능성 때문에 메모리 유지하는 FGameObject 목록 (PIE 종료 시 삭제) */
    TArray<FGameObject*> ZombieGameObjects;
};

// Helper function to wrap C++ object pointers in LuaComponentProxy for Lua
sol::object MakeCompProxy(sol::state_view SolState, UObject* Instance, UClass* Class);
