#include "pch.h"
#include "LuaScriptComponent.h"
#include <sol/state.hpp>
#include <sol/coroutine.hpp>
#include "../Scripting/LuaManager.h"

IMPLEMENT_CLASS(ULuaScriptComponent)

BEGIN_PROPERTIES(ULuaScriptComponent)
	MARK_AS_COMPONENT("Lua 스크립트 컴포넌트", "Lua 스크립트 컴포넌트입니다.")
	ADD_PROPERTY_SCRIPT(FString, ScriptFilePath, "Script", ".lua", true, "Lua Script 파일 경로")
END_PROPERTIES()

ULuaScriptComponent::ULuaScriptComponent() {}
ULuaScriptComponent::~ULuaScriptComponent() { Lua = nullptr; }

void ULuaScriptComponent::BeginPlay()
{
	auto LuaVM = GetWorld()->GetLuaManager();
	Lua  = &(LuaVM->GetState());

	// 독립된 환경 생성, Engine Object&Util 주입
	Env = LuaVM->CreateEnvironment();
	Env["Obj"] = Owner->GetGameObject();

	Env["StartCoroutine"] = [LuaVM, this, L=Lua](sol::function Func)
	{
		sol::coroutine co(*L, Func);
		LuaVM->GetScheduler().Register(std::move(co), this);
	};
	
	if(ScriptFilePath.empty())
	{
		return;
	}

	if (!LuaVM->LoadScriptInto(Env, ScriptFilePath)) {
		UE_LOG("[Lua] failed to run: %s\n", ScriptFilePath.c_str());
		GEngine.EndPIE();
		return;
	}
	
	// 함수 캐시
	FuncBeginPlay = FLuaManager::GetFunc(Env, "BeginPlay");
	FuncTick      = FLuaManager::GetFunc(Env, "Tick");
	FuncOnOverlap = FLuaManager::GetFunc(Env, "OnOverlap");
	FuncEndPlay		  =	FLuaManager::GetFunc(Env, "EndPlay");
	
	if (FuncBeginPlay.valid()) {
		auto Result = FuncBeginPlay();
		if (!Result.valid())
		{
			sol::error Err = Result; UE_LOG("[Lua] %s\n", Err.what());
			GEngine.EndPIE();
		}
	}
}

void ULuaScriptComponent::OnOverlap(const AActor* Other)
{
	if (Lua)
	{
		(*Lua)["OnOverlap"](Other->GetGameObject());
	}
}

void ULuaScriptComponent::TickComponent(float DeltaTime)
{
	if (FuncTick.valid()) {
		auto Result = FuncTick(DeltaTime);
		if (!Result.valid()) { sol::error Err = Result; UE_LOG("[Lua] %s\n", Err.what()); }
	}
}

void ULuaScriptComponent::EndPlay(EEndPlayReason Reason)
{
	if (FuncEndPlay.valid()) {
		auto Result = FuncEndPlay();
		if (!Result.valid())
		{
			sol::error Err = Result; UE_LOG("[Lua] %s\n", Err.what());
			GEngine.EndPIE();
		}
	}
	
	auto* LuaVM = GetWorld()->GetLuaManager();
	LuaVM->GetScheduler().CancelByOwner(this);  // Coroutine 일괄 종료

	FuncBeginPlay = sol::nil;
	FuncTick      = sol::nil;
	FuncOnOverlap = sol::nil;
	Env           = sol::nil;
	Lua				 = nullptr;	
}
