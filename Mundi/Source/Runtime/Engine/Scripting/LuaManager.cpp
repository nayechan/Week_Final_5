#include "pch.h"
#include "LuaManager.h"
#include "GameObject.h"

FLuaManager::FLuaManager()
{
    Lua = new sol::state();

    // Open essential standard libraries for gameplay scripts
    Lua->open_libraries(
        sol::lib::base,
        sol::lib::coroutine,
        sol::lib::math,
        sol::lib::table,
        sol::lib::string
    );

    Lua->new_usertype<FGameObject>("GameObject",
        "UUID", &FGameObject::UUID,
        "Location", sol::property(&FGameObject::GetLocation, &FGameObject::SetLocation),
        "Rotation", sol::property(&FGameObject::GetRotation, &FGameObject::SetRotation), 
        "Scale", sol::property(&FGameObject::GetScale, &FGameObject::SetScale),
        "bIsActive", sol::property(&FGameObject::GetIsActive, &FGameObject::SetIsActive),
        "Velocity", &FGameObject::Velocity,
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
            UE_LOG("[Lua] %f\n", num);
        },

        [](FVector Vector)
        {
            UE_LOG("[Lua] (%f, %f, %f)\n", Vector.X, Vector.Y, Vector.Z);
        }
    ));

    SharedLib = Lua->create_table();
    
    // GlobalConfig는 전역 table
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

    SharedLib.set_function("Vector", sol::overload(
       []() { return FVector(0.0f, 0.0f, 0.0f); },
       [](float x, float y, float z) { return FVector(x, y, z); }
   ));
    
    // FVector usertype 등록 (메서드와 프로퍼티)
    SharedLib.new_usertype<FVector>("FVector",
        sol::no_constructor,  // 생성자는 위에서 Vector 함수로 등록했음
        // Properties
        "X", &FVector::X,
        "Y", &FVector::Y,
        "Z", &FVector::Z,
        // Operators
        sol::meta_function::addition, [](const FVector& a, const FVector& b) -> FVector {
            return FVector(a.X + b.X, a.Y + b.Y, a.Z + b.Z);
        },
        sol::meta_function::subtraction, [](const FVector& a, const FVector& b) -> FVector {
            return FVector(a.X - b.X, a.Y - b.Y, a.Z - b.Z);
        },
        sol::meta_function::multiplication, sol::overload(
            [](const FVector& v, float f) -> FVector { return v * f; },
            [](float f, const FVector& v) -> FVector { return v * f; }
        ),
        // Methods
        "Length", &FVector::Distance,
        "Normalize", &FVector::Normalize,
        "Dot", [](const FVector& a, const FVector& b) { return FVector::Dot(a, b); },
        "Cross", [](const FVector& a, const FVector& b) { return FVector::Cross(a, b); }
    );
    RegisterComponentProxy(*Lua);
    ExposeAllComponentsToLua();

    // 위 등록 마친 뒤 fall back 설정 : Shared lib의 fall back은 G
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

struct FBoundProp {
    const FProperty* Property = nullptr;
    // TODO : 추후 세부 설정(editable, readonly...)
};

struct FBoundClassDesc { // 해당 Class에 있는 Property 목록
    UClass* Class = nullptr;
    TMap<FString, FBoundProp> PropsByName;  
};

static TMap<UClass*, FBoundClassDesc> GBoundClasses;

struct LuaTypeIO {
    // C++ to Lua
    std::function<sol::object(sol::state_view, const void* Base, const FProperty&)> Read;
    // Lua to C++
    std::function<void(sol::object, void* Base, const FProperty&)> Write;
};

static void BuildBoundClass(UClass* Class) {
    if (!Class) return;
    if (GBoundClasses.count(Class)) return;
    
    FBoundClassDesc Desc;
    Desc.Class = Class;

    for (auto& Property : Class->GetAllProperties()) {
        if (!Property.bIsEditAnywhere) continue;

        FBoundProp BoundProp;
        BoundProp.Property = &Property;
        FString LuaName =Property.Name;

        Desc.PropsByName.emplace(LuaName, BoundProp);
    }
    GBoundClasses.emplace(Class, std::move(Desc));
}

struct LuaComponentProxy {
    void*   Instance = nullptr; /* 객체 주소 */
    UClass* Class    = nullptr; /* 메타 정보 */

    // __index : lua가 읽을 때
    static sol::object Index(sol::this_state Lua, LuaComponentProxy& Self, const char* Key) {
        if (!Self.Instance) return sol::nil;
        
        auto It = GBoundClasses.find(Self.Class);
        if (It == GBoundClasses.end()) return sol::nil;

        // 이름(Key)으로 해당 Property 찾음
        auto Iterate = It->second.PropsByName.find(Key);
        if (Iterate == It->second.PropsByName.end()) return sol::nil;

        const FBoundProp& BoundProp = Iterate->second;
        const FProperty* Property = BoundProp.Property;
        
        sol::state_view State(Lua);

        switch (Property->Type) {
        /* TODO : 호환 자료형 더 늘리기*/
        case EPropertyType::Float:  return sol::make_object(State, *Property->GetValuePtr<float>(Self.Instance));
        case EPropertyType::Int32:  return sol::make_object(State, *Property->GetValuePtr<int>(Self.Instance));
        case EPropertyType::FString: return sol::make_object(State, *Property->GetValuePtr<FString>(Self.Instance));
        case EPropertyType::FVector: return sol::make_object(State, *Property->GetValuePtr<FVector>(Self.Instance));
        default: return sol::nil;
        }
    }

    // __newindex : lua가 쓸 때
    static void NewIndex(LuaComponentProxy& Self, const char* Key, sol::object Obj) {
        auto IterateClass = GBoundClasses.find(Self.Class);
        if (IterateClass == GBoundClasses.end()) return;

        auto It = IterateClass->second.PropsByName.find(Key);
        if (It == IterateClass->second.PropsByName.end()) return;

        const FBoundProp& BoundProp = It->second;
        const FProperty* Property = BoundProp.Property;
        
        switch (Property->Type) {
        case EPropertyType::Float:
            if (Obj.get_type() == sol::type::number)
                *Property->GetValuePtr<float>(Self.Instance) = (float)Obj.as<double>();
            break;
        case EPropertyType::Int32:
            if (Obj.get_type() == sol::type::number)
                *Property->GetValuePtr<int>(Self.Instance) = (int)Obj.as<double>();
            break;
        case EPropertyType::FString:
            if (Obj.get_type() == sol::type::string)
                *Property->GetValuePtr<FString>(Self.Instance) = Obj.as<FString>();
            break;
        case EPropertyType::FVector:
            if (Obj.is<FVector>()) {
                *Property->GetValuePtr<FVector>(Self.Instance) = Obj.as<FVector>();
            } else if (Obj.get_type() == sol::type::table) {
                sol::table t = Obj.as<sol::table>();
                FVector tmp{
                    (float)t.get_or("X", 0.0),
                    (float)t.get_or("Y", 0.0),
                    (float)t.get_or("Z", 0.0)
                };
                *Property->GetValuePtr<FVector>(Self.Instance) = tmp;
            }
            break;
        default: break;
        }
    }
};

void FLuaManager::RegisterComponentProxy(sol::state& Lua) {
    Lua.new_usertype<LuaComponentProxy>("Component",
        sol::meta_function::index,     &LuaComponentProxy::Index,
        sol::meta_function::new_index, &LuaComponentProxy::NewIndex
    );
}

static sol::object MakeCompProxy(sol::state_view SolState, void* Instance, UClass* Class) {
    BuildBoundClass(Class);
    LuaComponentProxy Proxy;
    Proxy.Instance = Instance;
    Proxy.Class = Class;
    return sol::make_object(SolState, std::move(Proxy));
}

void FLuaManager::ExposeAllComponentsToLua()
{
    SharedLib["Components"] = Lua->create_table();

    SharedLib.set_function("AddComponent",
        [this](sol::object Obj, const FString& ClassName)
        {
           if (!Obj.is<FGameObject&>()) {
                UE_LOG("[Lua][error] Error: Expected GameObject\n");
                return sol::make_object(*Lua, sol::nil);
            }
        
            FGameObject& GameObject = Obj.as<FGameObject&>();
            AActor* Actor = GameObject.GetOwner();

            UClass* Class = UClass::FindClass(ClassName);
            if (!Class) return sol::make_object(*Lua, sol::nil);

            UActorComponent* Comp = Actor->AddNewComponent(Class);
            return MakeCompProxy(*Lua, Comp, Class);
        });

    SharedLib.set_function("GetComponent",
        [this](sol::object Obj, const FString& ClassName)
        {
            if (!Obj.is<FGameObject&>()) {
                UE_LOG("[Lua][error] Error: Expected GameObject\n");
                return sol::make_object(*Lua, sol::nil);
            }
            
            FGameObject& GameObject = Obj.as<FGameObject&>();
            AActor* Actor = GameObject.GetOwner();

            UClass* Class = UClass::FindClass(ClassName);
            if (!Class) return sol::make_object(*Lua, sol::nil);
            
            UActorComponent* Comp = Actor->GetComponent(Class);
            if (!Comp) return sol::make_object(*Lua, sol::nil); 
            
            return MakeCompProxy(*Lua, Comp, Class);
        }
    );
}

bool FLuaManager::LoadScriptInto(sol::environment& Env, const FString& Path) {
    auto Chunk = Lua->load_file(Path);
    if (!Chunk.valid()) { sol::error Err = Chunk; UE_LOG("[Lua][error] %s", Err.what()); return false; }
    
    sol::protected_function ProtectedFunc = Chunk;
    sol::set_environment(Env, ProtectedFunc);         
    auto Result = ProtectedFunc();
    if (!Result.valid()) { sol::error Err = Result; UE_LOG("[Lua][error] %s", Err.what()); return false; }
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

// Lua 함수 캐시 함수
sol::protected_function FLuaManager::GetFunc(sol::environment& Env, const char* Name)
{
    // (*Lua)[BeginPlay]()를 VM이 아닌, env에서 생성 및 캐시한다.
    // TODO : 함수 이름이 중복된다면?
    if (!Env.valid())
        return {};

    sol::object Object = Env[Name];
    
    if (Object == sol::nil || Object.get_type() != sol::type::function)
        return {};
    
    sol::protected_function Func = Object.as<sol::protected_function>();
    
    return Func;
}
