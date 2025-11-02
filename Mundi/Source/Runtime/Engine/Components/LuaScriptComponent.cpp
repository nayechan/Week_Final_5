#include "pch.h"
#include "LuaScriptComponent.h"
#include "PrimitiveComponent.h"
#include <sol/state.hpp>
#include <sol/coroutine.hpp>
#include "../Scripting/LuaManager.h"
#include "../Scripting/GameObject.h"

IMPLEMENT_CLASS(ULuaScriptComponent)

BEGIN_PROPERTIES(ULuaScriptComponent)
	MARK_AS_COMPONENT("Lua 스크립트 컴포넌트", "Lua 스크립트 컴포넌트입니다.")
	ADD_PROPERTY_SCRIPT(FString, ScriptFilePath, "Script", ".lua", true, "Lua Script 파일 경로")
END_PROPERTIES()

ULuaScriptComponent::ULuaScriptComponent()
{
	bCanEverTick = true;	// tick 지원 여부
}

ULuaScriptComponent::~ULuaScriptComponent() { Lua = nullptr; }

void ULuaScriptComponent::BeginPlay()
{
	// 델리게이트 등록
	if (AActor* Owner = GetOwner())
	{
		FDelegateHandle BeginHandle = Owner->OnComponentBeginOverlap.AddDynamic(this, &ULuaScriptComponent::OnOverlap);
		//FDelegateHandle EndHandle = Owner->OnComponentEndOverlap.AddDynamic(Owner, &AActor::OnEndOverlap);
		//FDelegateHandle HitHandle = Owner->OnComponentHit.AddDynamic(Owner, &AActor::OnHit);
	}

	auto LuaVM = GetWorld()->GetLuaManager();
	Lua  = &(LuaVM->GetState());

	// 독립된 환경 생성, Engine Object&Util 주입
	Env = LuaVM->CreateEnvironment();

	FGameObject* Obj = Owner->GetGameObject();
	Env["Obj"] = Obj;

	Env["StartCoroutine"] = [LuaVM, this](sol::function f) {
		sol::state_view L = LuaVM->GetState();
		
		sol::thread Thread = sol::thread::create(L); // Coroutine 관리할 Thread 생성
		sol::state_view ThreadState = Thread.state();
		
		sol::coroutine Coroutine(ThreadState.lua_state(), f);                // 스레드에 함수 올리기
		return LuaVM->GetScheduler().Register(std::move(Thread), std::move(Coroutine), this);
	};
	
	if(ScriptFilePath.empty())
	{
		return;
	}

	if (!LuaVM->LoadScriptInto(Env, ScriptFilePath)) {
		UE_LOG("[Lua][error] failed to run: %s\n", ScriptFilePath.c_str());
		GEngine.EndPIE();
		return;
	}

	// InputManger 주입
	(*Lua)["InputManager"] = &UInputManager::GetInstance(); 
	// 함수 캐시
	FuncBeginPlay = FLuaManager::GetFunc(Env, "BeginPlay");
	FuncTick      = FLuaManager::GetFunc(Env, "Tick");
	FuncOnOverlap = FLuaManager::GetFunc(Env, "OnOverlap");
	FuncEndPlay		  =	FLuaManager::GetFunc(Env, "EndPlay");
	
	if (FuncBeginPlay.valid()) {
		auto Result = FuncBeginPlay();
		if (!Result.valid())
		{
			sol::error Err = Result; UE_LOG("[Lua][error] %s\n", Err.what());
			GEngine.EndPIE();
		}
	}
}

void ULuaScriptComponent::OnOverlap(UPrimitiveComponent* MyComp, UPrimitiveComponent* OtherComp)
{
	if (FuncOnOverlap.valid())
	{
		FGameObject* OtherGameObject = nullptr;
		if (OtherComp)
		{
			if (AActor* OtherActor = OtherComp->GetOwner())
			{
				OtherGameObject = OtherActor->GetGameObject();
			}
		}

		if (OtherGameObject)
		{
			auto Result = FuncOnOverlap(OtherGameObject);
			if (!Result.valid())
			{
				sol::error Err = Result; UE_LOG("[Lua][error] %s\n", Err.what());
				GEngine.EndPIE();
			}
		}
	}
}

void ULuaScriptComponent::TickComponent(float DeltaTime)
{
	if (FuncTick.valid()) {
		auto Result = FuncTick(DeltaTime);
		if (!Result.valid()) { sol::error Err = Result; UE_LOG("[Lua][error] %s\n", Err.what()); }
	}
}

void ULuaScriptComponent::EndPlay()
{
	if (FuncEndPlay.valid()) {
		auto Result = FuncEndPlay();
		if (!Result.valid())
		{
			sol::error Err = Result; UE_LOG("[Lua][error] %s\n", Err.what());
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
