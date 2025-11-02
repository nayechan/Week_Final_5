#include "pch.h"
#include "LuaManager.h"

FLuaManager::FLuaManager()
{
    Lua = new sol::state();

    Lua->open_libraries(sol::lib::base, sol::lib::coroutine);

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
        "Scale", &FGameObject::Scale,
        "PrintLocation", &FGameObject::PrintLocation
    );
    
    Lua->set_function("print", sol::overload(
        [](const FString& msg) {
            UE_LOG("[Lua-Str] %s\n", msg.c_str());
        },
        
        [](int num){
            UE_LOG("[Lua] %d\n", num);
        },
        
        [](double num){
            UE_LOG("[Lua] %d\n", num);
        }
    ));

    SharedLib = Lua->create_table();
    SharedLib["GlobalConfig"] = Lua->create_table();
    // SharedLib["GlobalConfig"]["Gravity"] = 9.8;

    SharedLib.set_function("SpawnPrefab", sol::overload(
        [](const FString& PrefabPath) -> FGameObject*
        {
            FGameObject* NewObject = nullptr;

            AActor* NewActor = GWorld->SpawnPrefabActor(UTF8ToWide(PrefabPath));

            if (NewActor)
            {
                NewObject = NewActor->GetGameObject();
            }

            return NewObject;
        }
    ));

    SharedLib.set_function("AddComponent",
        [this](sol::userdata Obj, const FString& ClassName)
        {
            FGameObject& GameObject = Obj.as<FGameObject&>();
            AActor* Actor = GameObject->
        }
    );
    // Shared lib의 fall back은 G
    sol::table MetaTableShared = Lua->create_table();
    MetaTableShared[sol::meta_function::index] = Lua->globals();
    SharedLib[sol::metatable_key]  = MetaTableShared;
}

FLuaManager::~FLuaManager()
{
    ShutdownBeforeLuaClose();
    
    if (Lua)
    {
        delete Lua;
        Lua = nullptr;
    }
}

sol::environment FLuaManager::CreateEnvironment()
{
    sol::environment Env(*Lua, sol::create);

    // Environment의 Fall back은 SharedLib
    sol::table MetaTable = Lua->create_table();
    MetaTable[sol::meta_function::index] = SharedLib;
    Env[sol::metatable_key] = MetaTable;
    
    return Env;
}

bool FLuaManager::LoadScriptInto(sol::environment& Env, const FString& Path) {
    auto Chunk = Lua->load_file(Path);
    if (!Chunk.valid()) { sol::error Err = Chunk; UE_LOG("[Lua] %s", Err.what()); return false; }
    
    sol::protected_function ProtectedFunc = Chunk;
    sol::set_environment(Env, ProtectedFunc);         
    auto Result = ProtectedFunc();
    if (!Result.valid()) { sol::error Err = Result; UE_LOG("[Lua] %s", Err.what()); return false; }
    return true;
}

void FLuaManager::Tick(double DeltaSeconds)
{
    CoroutineSchedular.Tick(DeltaSeconds);
}

void FLuaManager::ShutdownBeforeLuaClose()
{
    CoroutineSchedular.ShutdownBeforeLuaClose();
    SharedLib = sol::nil;
}

sol::protected_function FLuaManager::GetFunc(sol::environment& Env, const char* Name)
{
    // (*Lua)[BeginPlay]()를 VM이 아닌, env에서 생성 및 캐시한다.
    // TODO : 함수 이름이 중복된다면?
    if (!Env.valid())
        return {};

    sol::object Object = Env.raw_get_or(Name, sol::make_object(Env.lua_state(), sol::nil));
    
    if (Object == sol::nil || Object.get_type() != sol::type::function)
        return {};
    
    sol::protected_function Func = Object.as<sol::protected_function>();
    
    return Func;
}