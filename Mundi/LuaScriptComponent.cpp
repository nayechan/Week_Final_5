#include "pch.h"
#include "LuaScriptComponent.h"
#include "sol/state.hpp"

IMPLEMENT_CLASS(ULuaScriptComponent)

BEGIN_PROPERTIES(ULuaScriptComponent)
	MARK_AS_COMPONENT("Lua 스크립트 컴포넌트", "Lua 스크립트 컴포넌트입니다.")
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
	Lua = new sol::state();

	Lua->open_libraries(sol::lib::base);

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

	// Lua->script_file("Data/Scripts/" + ScriptFilePath);
	Lua->script_file("Data/Scripts/template.lua");

	/*Lua->script(R"(
	  function Tick(dt)
	      obj.Location = obj.Location + obj.Velocity * dt
	  end
	)");*/

	(*Lua)["BeginPlay"]();
}

void ULuaScriptComponent::OnOverlap(const AActor* Other)
{
	(*Lua)["OnOverlap"](Other->GetGameObject());
}

void ULuaScriptComponent::TickComponent(float DeltaTime)
{
	(*Lua)["Tick"](DeltaTime);
}

void ULuaScriptComponent::EndPlay(EEndPlayReason Reason)
{
	(*Lua)["EndPlay"]();
}
