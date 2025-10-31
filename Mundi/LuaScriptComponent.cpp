#include "pch.h"
#include "LuaScriptComponent.h"
#include <sol/state.hpp>

IMPLEMENT_CLASS(ULuaScriptComponent)

BEGIN_PROPERTIES(ULuaScriptComponent)
	MARK_AS_COMPONENT("Lua 스크립트 컴포넌트", "Lua 스크립트 컴포넌트입니다.")
	ADD_PROPERTY_SCRIPT(FString, ScriptFilePath, "Script", ".lua", true, "Lua Script 파일 경로")
END_PROPERTIES()

ULuaScriptComponent::ULuaScriptComponent()
{
}

ULuaScriptComponent::~ULuaScriptComponent()
{
	if (Lua)
	{
		delete Lua;
		Lua = nullptr;
	}
}

void ULuaScriptComponent::BeginPlay()
{
	if (ScriptFilePath.empty())
	{
		return;
	}

	Lua = new sol::state();

	Lua->open_libraries(sol::lib::base);

	Lua->set_function("print", [](const std::string& msg)
	{
		UE_LOG("[Lua] %s\n", msg.c_str());
	});
		
	Lua->new_usertype<FVector>("Vector",
		sol::constructors<FVector(), FVector(float, float, float)>(),
		"X", &FVector::X,
		"Y", &FVector::Y,
		"Z", &FVector::Z,
		sol::meta_function::addition, [](const FVector& a, const FVector& b) { return FVector(a.X + b.X, a.Y + b.Y, a.Z + b.Z); },
		sol::meta_function::multiplication, [](const FVector& v, float f) { return v * f; }
	);

	Lua->new_usertype<FGameObject>("GameObject",
		"UUID", &FGameObject::UUID,
		"Location", &FGameObject::Location,
		"Velocity", &FGameObject::Velocity,
		"PrintLocation", &FGameObject::PrintLocation
	);
	
	FGameObject* Obj = Owner->GetGameObject();
	(*Lua)["Obj"] = Obj;

	Lua->script_file(ScriptFilePath);
	// Lua->script_file("Data/Scripts/coroutineTest.lua");

	/*Lua->script(R"(
	  function Tick(dt)
	      obj.Location = obj.Location + obj.Velocity * dt
	  end
	)");*/

	(*Lua)["BeginPlay"]();
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
	if (Lua)
	{
		(*Lua)["Tick"](DeltaTime);
	}
}

void ULuaScriptComponent::EndPlay(EEndPlayReason Reason)
{
	if (Lua)
	{
		(*Lua)["EndPlay"]();
	}
}
