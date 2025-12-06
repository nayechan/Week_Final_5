#include "pch.h"
#include "LuaManager.h"
#include "LuaComponentProxy.h"
#include "LuaArrayProxy.h"
#include "LuaMapProxy.h"
#include "LuaStructProxy.h"
#include "GameObject.h"
#include "ObjectIterator.h"
#include "CameraActor.h"
#include "CameraComponent.h"
#include "PlayerCameraManager.h"
#include "GameInstance.h"
#include "GameEngine.h"
#include "ItemComponent.h"
#include "RenderManager.h"
#include "Renderer.h"
#include "PrimitiveComponent.h"
#include <tuple>

sol::object MakeCompProxy(sol::state_view SolState, UObject* Instance, UClass* Class) {
    LuaComponentProxy Proxy;
    Proxy.Instance = Instance;
    Proxy.Class = Class;
    // Build bound class for reflection-based access
    BuildBoundClass(Class);
    return sol::make_object(SolState, std::move(Proxy));
}

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

    SharedLib = Lua->create_table();

    // Lua에서 Actor와 FGameObject 가 1대1로 매칭되고
    // Component는 그대로 Component와 1대1로 매칭된다
    // NOTE: 그냥 FGameObject 개념을 없애고 그냥 Actor/Component 그대로 사용하는 게 좋을듯?
    Lua->new_usertype<FGameObject>("GameObject",
        "UUID", &FGameObject::UUID,
        "Tag", sol::property(
            [](FGameObject& GO) -> FString {
                if (GO.IsDestroyed()) return "";
                AActor* Owner = GO.GetOwner();
                if (!Owner || Owner->IsPendingDestroy()) return "";
                return GO.GetTag();
            },
            [](FGameObject& GO, FString Value) {
                if (GO.IsDestroyed()) return;
                AActor* Owner = GO.GetOwner();
                if (!Owner || Owner->IsPendingDestroy()) return;
                GO.SetTag(Value);
            }),
        "Location", sol::property(
            [](FGameObject& GO) -> FVector {
                if (GO.IsDestroyed()) return FVector(0, 0, 0);
                AActor* Owner = GO.GetOwner();
                if (!Owner || Owner->IsPendingDestroy()) return FVector(0, 0, 0);
                return GO.GetLocation();
            },
            [](FGameObject& GO, FVector Value) {
                if (GO.IsDestroyed()) return;
                AActor* Owner = GO.GetOwner();
                if (!Owner || Owner->IsPendingDestroy()) return;
                GO.SetLocation(Value);
            }),
        "Rotation", sol::property(
            [](FGameObject& GO) -> FVector {
                if (GO.IsDestroyed()) return FVector(0, 0, 0);
                AActor* Owner = GO.GetOwner();
                if (!Owner || Owner->IsPendingDestroy()) return FVector(0, 0, 0);
                return GO.GetRotation();
            },
            [](FGameObject& GO, FVector Value) {
                if (GO.IsDestroyed()) return;
                AActor* Owner = GO.GetOwner();
                if (!Owner || Owner->IsPendingDestroy()) return;
                GO.SetRotation(Value);
            }),
        "Scale", sol::property(
            [](FGameObject& GO) -> FVector {
                if (GO.IsDestroyed()) return FVector(1, 1, 1);
                AActor* Owner = GO.GetOwner();
                if (!Owner || Owner->IsPendingDestroy()) return FVector(1, 1, 1);
                return GO.GetScale();
            },
            [](FGameObject& GO, FVector Value) {
                if (GO.IsDestroyed()) return;
                AActor* Owner = GO.GetOwner();
                if (!Owner || Owner->IsPendingDestroy()) return;
                GO.SetScale(Value);
            }),
        "bIsActive", sol::property(
            [](FGameObject& GO) -> bool {
                // bIsDestroyed 체크를 먼저 수행 (dangling pointer 방지)
                if (GO.IsDestroyed())
                    return false;
                AActor* Owner = GO.GetOwner();
                if (!Owner || Owner->IsPendingDestroy())
                    return false;  // 파괴됨/파괴대기 중이면 비활성으로 간주
                return GO.GetIsActive();
            },
            [](FGameObject& GO, bool Value) {
                if (GO.IsDestroyed())
                    return;
                AActor* Owner = GO.GetOwner();
                if (!Owner || Owner->IsPendingDestroy())
                    return;  // 파괴됨/파괴대기 중이면 무시
                GO.SetIsActive(Value);
            }),
        "Velocity", &FGameObject::Velocity,
        "PrintLocation", [](FGameObject& GO) {
            if (GO.IsDestroyed()) return;
            AActor* Owner = GO.GetOwner();
            if (!Owner || Owner->IsPendingDestroy()) return;
            GO.PrintLocation();
        },
        "GetForward", [](FGameObject& GO) -> FVector {
            if (GO.IsDestroyed()) return FVector(0, 0, 0);
            AActor* Owner = GO.GetOwner();
            if (!Owner || Owner->IsPendingDestroy()) return FVector(0, 0, 0);
            return GO.GetForward();
        },
        "IsDestroyed", [](FGameObject& GO) -> bool {
            if (GO.IsDestroyed()) return true;
            AActor* Owner = GO.GetOwner();
            if (!Owner || Owner->IsPendingDestroy()) return true;
            return false;
        }
    );
    
    Lua->new_usertype<UCameraComponent>("CameraComponent",
        sol::no_constructor,
        "SetLocation", sol::overload(
            [](UCameraComponent* Camera, FVector Location)
            {
                if (!Camera)
                {
                    return;
                }
                Camera->SetWorldLocation(Location);
            },
            [](UCameraComponent* Camera, float X, float Y, float Z)
            {
                if (!Camera)
                {
                    return;
                }
                Camera->SetWorldLocation(FVector(X, Y, Z));
            }
        ),
        "SetCameraForward",
        [](UCameraComponent* Camera, FVector Direction)
        {
            if (!Camera)
            {
                return;
            }
            ACameraActor* CameraActor = Cast<ACameraActor>(Camera->GetOwner());
            CameraActor->SetForward(Direction);
        },
        "GetActorLocation", [](UCameraComponent* Camera) -> FVector
        {
            if (!Camera)
            {
                // 유효하지 않은 경우 0 벡터 반환
                return FVector(0.f, 0.f, 0.f);
            }
            return Camera->GetWorldLocation();
        },
        "GetActorRight", [](UCameraComponent* Camera) -> FVector
        {
            if (!Camera) return FVector(0.f, 0.f, 1.f); // 기본값 (World Forward)

            // C++ UCameraComponent 클래스의 실제 함수명으로 변경해야 합니다.
            // (예: GetActorForwardVector(), GetForward() 등)
            return Camera->GetForward();
        }
    );
    Lua->new_usertype<UInputManager>("InputManager",
        "IsKeyDown", sol::overload(
            &UInputManager::IsKeyDown,
            [](UInputManager* Self, const FString& Key) {
                if (Key.empty()) return false;
                return Self->IsKeyDown(Key[0]);
            }),
        "IsKeyPressed", sol::overload(
            &UInputManager::IsKeyPressed,
            [](UInputManager* Self, const FString& Key) {
                if (Key.empty()) return false;
                return Self->IsKeyPressed(Key[0]);
            }),
        "IsKeyReleased", sol::overload(
            &UInputManager::IsKeyReleased,
            [](UInputManager* Self, const FString& Key) {
                if (Key.empty()) return false;
                return Self->IsKeyReleased(Key[0]);
            }),
        "IsMouseButtonDown", &UInputManager::IsMouseButtonDown,
        "IsMouseButtonPressed", &UInputManager::IsMouseButtonPressed,
        "IsMouseButtonReleased", &UInputManager::IsMouseButtonReleased,
        "GetMouseDelta", [](UInputManager* Self) {
            const FVector2D Delta = Self->GetMouseDelta();
            return FVector(Delta.X, Delta.Y, 1.0);
        },
        "SetCursorVisible", [](UInputManager* Self, bool bVisible){
            if (bVisible)
            { 
                Self->SetCursorVisible(true);
                if (Self->IsCursorLocked())
                    Self->ReleaseCursor();
            } else
            { 
                Self->SetCursorVisible(false);
                Self->LockCursor();
            }
        }
    );                
    
    sol::table MouseButton = Lua->create_table("MouseButton");
    MouseButton["Left"] = EMouseButton::LeftButton;
    MouseButton["Right"] = EMouseButton::RightButton;
    MouseButton["Middle"] = EMouseButton::MiddleButton;
    MouseButton["XButton1"] = EMouseButton::XButton1;
    MouseButton["XButton2"] = EMouseButton::XButton2;

    // KeyCode enum 바인딩 (Windows Virtual Key Codes)
    sol::table KeyCode = Lua->create_table("Keycode");
    // 숫자 키 (메인 키보드)
    KeyCode["Alpha0"] = '0';
    KeyCode["Alpha1"] = '1';
    KeyCode["Alpha2"] = '2';
    KeyCode["Alpha3"] = '3';
    KeyCode["Alpha4"] = '4';
    KeyCode["Alpha5"] = '5';
    KeyCode["Alpha6"] = '6';
    KeyCode["Alpha7"] = '7';
    KeyCode["Alpha8"] = '8';
    KeyCode["Alpha9"] = '9';

    // 알파벳 키
    KeyCode["A"] = 'A';
    KeyCode["B"] = 'B';
    KeyCode["C"] = 'C';
    KeyCode["D"] = 'D';
    KeyCode["E"] = 'E';
    KeyCode["F"] = 'F';
    KeyCode["G"] = 'G';
    KeyCode["H"] = 'H';
    KeyCode["I"] = 'I';
    KeyCode["J"] = 'J';
    KeyCode["K"] = 'K';
    KeyCode["L"] = 'L';
    KeyCode["M"] = 'M';
    KeyCode["N"] = 'N';
    KeyCode["O"] = 'O';
    KeyCode["P"] = 'P';
    KeyCode["Q"] = 'Q';
    KeyCode["R"] = 'R';
    KeyCode["S"] = 'S';
    KeyCode["T"] = 'T';
    KeyCode["U"] = 'U';
    KeyCode["V"] = 'V';
    KeyCode["W"] = 'W';
    KeyCode["X"] = 'X';
    KeyCode["Y"] = 'Y';
    KeyCode["Z"] = 'Z';

    // 특수 키
    KeyCode["Space"] = VK_SPACE;
    KeyCode["Enter"] = VK_RETURN;
    KeyCode["Escape"] = VK_ESCAPE;
    KeyCode["Tab"] = VK_TAB;
    KeyCode["Backspace"] = VK_BACK;
    KeyCode["Delete"] = VK_DELETE;
    KeyCode["Insert"] = VK_INSERT;

    // 방향키
    KeyCode["Left"] = VK_LEFT;
    KeyCode["Right"] = VK_RIGHT;
    KeyCode["Up"] = VK_UP;
    KeyCode["Down"] = VK_DOWN;

    // 기능키
    KeyCode["F1"] = VK_F1;
    KeyCode["F2"] = VK_F2;
    KeyCode["F3"] = VK_F3;
    KeyCode["F4"] = VK_F4;
    KeyCode["F5"] = VK_F5;
    KeyCode["F6"] = VK_F6;
    KeyCode["F7"] = VK_F7;
    KeyCode["F8"] = VK_F8;
    KeyCode["F9"] = VK_F9;
    KeyCode["F10"] = VK_F10;
    KeyCode["F11"] = VK_F11;
    KeyCode["F12"] = VK_F12;

    // 수정자 키
    KeyCode["LeftShift"] = VK_LSHIFT;
    KeyCode["RightShift"] = VK_RSHIFT;
    KeyCode["LeftControl"] = VK_LCONTROL;
    KeyCode["RightControl"] = VK_RCONTROL;
    KeyCode["LeftAlt"] = VK_LMENU;
    KeyCode["RightAlt"] = VK_RMENU;
    
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
    SharedLib.set_function("DeleteObject", sol::overload(
        [](const FGameObject& GameObject)
        {
            for (TObjectIterator<AActor> It; It; ++It)
            {
                AActor* Actor = *It;

                if (Actor->UUID == GameObject.UUID)
                {
                    Actor->Destroy();   // 지연 삭제 요청 (즉시 삭제하면 터짐)
                    break;
                }
            }
        }
    ));
    SharedLib.set_function("FindObjectByName",
        [](const FString& ActorName) -> FGameObject*
        {
            if (!GWorld)
            {
                return nullptr;
            }

            // Lua의 FString을 FName으로 변환
            FName NameToFind(ActorName);

            AActor* FoundActor = GWorld->FindActorByName(NameToFind);

            // Lua 스크립트가 사용할 수 있도록 AActor*를 FGameObject*로 변환
            if (FoundActor && !FoundActor->IsPendingDestroy())
            {
                return FoundActor->GetGameObject();
            }

            return nullptr; // 찾지 못함
        }
    );
    SharedLib.set_function("FindComponentByName",
        [this](const FString& ComponentName) -> UActorComponent*
        {
            if (!GWorld)
            {
                return nullptr;
            }

            // Lua의 FString을 FName으로 변환
            FName NameToFind(ComponentName);

            UActorComponent* FoundComponent = GWorld->FindComponentByName(NameToFind);

            // Lua 스크립트가 사용할 수 있도록 UActorComponent*를 LuaComponentProxy로 변환
            if (FoundComponent && !FoundComponent->IsPendingDestroy())
            {
                return FoundComponent;
            }

            return nullptr; // 찾지 못함
        }
    );
    SharedLib.set_function("GetCamera",
        []() -> UCameraComponent*
        {
            if (!GWorld)
            {
                return nullptr;
            }
            return GWorld->GetPlayerCameraManager()->GetViewCamera();
        }
    );
    SharedLib.set_function("GetCameraManager",
        []() -> APlayerCameraManager*
        {
            if (!GWorld)
            {
                return nullptr;
            }
            return GWorld->GetPlayerCameraManager();
        }
    );
    SharedLib.set_function("SetPlayerForward",
        [](FGameObject& GameObject, FVector Direction)
        {
            AActor* Player = GameObject.GetOwner();
            if (!Player)
            {
                return;
            }

            USceneComponent* SceneComponent = Player->GetRootComponent();

            if (!SceneComponent)
            {
                return;
            }

            SceneComponent->SetForward(Direction);
        }
   );
    SharedLib.set_function("Vector", sol::overload(
       []() { return FVector(0.0f, 0.0f, 0.0f); },
       [](float x, float y, float z) { return FVector(x, y, z); }
   ));

    //@TODO(Timing)
    SharedLib.set_function("SetSlomo", [](float Duration , float Dilation) { GWorld->RequestSlomo(Duration, Dilation); });

    SharedLib.set_function("HitStop", [](float Duration, sol::optional<float> Scale) { GWorld->RequestHitStop(Duration, Scale.value_or(0.0f)); });
    
    SharedLib.set_function("TargetHitStop", [](FGameObject& Obj, float Duration, sol::optional<float> Scale) 
        {
            if (AActor* Owner = Obj.GetOwner())
            {
                Owner->SetCustomTimeDillation(Duration, Scale.value_or(0.0f));
            }
        });
    
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

    Lua->set_function("Color", sol::overload(
        []() { return FLinearColor(0.0f, 0.0f, 0.0f, 1.0f); },
        [](float R, float G, float B) { return FLinearColor(R, G, B, 1.0f); },
        [](float R, float G, float B, float A) { return FLinearColor(R, G, B, A); }
    ));


    SharedLib.new_usertype<FLinearColor>("FLinearColor",
        sol::no_constructor,
        "R", &FLinearColor::R,
        "G", &FLinearColor::G,
        "B", &FLinearColor::B,
        "A", &FLinearColor::A
    );

    // FName constructor and usertype
    Lua->set_function("Name", sol::overload(
        []() { return FName(); },
        [](const char* Str) { return FName(Str); },
        [](const FString& Str) { return FName(Str); }
    ));

    SharedLib.new_usertype<FName>("FName",
        sol::constructors<FName(), FName(const char*), FName(const FString&)>(),
        "ToString", &FName::ToString,
        sol::meta_function::to_string, &FName::ToString,
        sol::meta_function::equal_to, [](const FName& a, const FName& b) { return a == b; }
    );

    RegisterComponentProxy(*Lua);
    ExposeGlobalFunctions();
    ExposeAllComponentsToLua();

    // ════════════════════════════════════════════════════════════════════════
    // 레벨 전환 시스템 바인딩
    // ════════════════════════════════════════════════════════════════════════

    // GameInstance 바인딩
    Lua->new_usertype<UGameInstance>("GameInstance",
        sol::no_constructor,

        // 아이템 관리
        "AddItem", &UGameInstance::AddItem,
        "GetItemCount", &UGameInstance::GetItemCount,
        "RemoveItem", &UGameInstance::RemoveItem,
        "HasItem", &UGameInstance::HasItem,
        "ClearItems", &UGameInstance::ClearItems,

        // 플레이어 상태
        "SetPlayerHealth", &UGameInstance::SetPlayerHealth,
        "GetPlayerHealth", &UGameInstance::GetPlayerHealth,
        "SetPlayerScore", &UGameInstance::SetPlayerScore,
        "GetPlayerScore", &UGameInstance::GetPlayerScore,
        "SetPlayerName", &UGameInstance::SetPlayerName,
        "GetPlayerName", &UGameInstance::GetPlayerName,
        "SetRescuedCount", &UGameInstance::SetRescuedCount,
        "GetRescuedCount", &UGameInstance::GetRescuedCount,
        "ResetPlayerData", &UGameInstance::ResetPlayerData,

        // 범용 키-값 저장소
        "SetInt", &UGameInstance::SetInt,
        "GetInt", &UGameInstance::GetInt,
        "SetFloat", &UGameInstance::SetFloat,
        "GetFloat", &UGameInstance::GetFloat,
        "SetString", &UGameInstance::SetString,
        "GetString", &UGameInstance::GetString,
        "SetBool", &UGameInstance::SetBool,
        "GetBool", &UGameInstance::GetBool,
        "HasKey", &UGameInstance::HasKey,
        "ClearAll", &UGameInstance::ClearAll,

        // 디버그
        "PrintDebugInfo", &UGameInstance::PrintDebugInfo
    );

    // 전역 함수: GetGameInstance()
    SharedLib.set_function("GetGameInstance",
        []() -> UGameInstance*
        {
            if (!GWorld)
            {
                UE_LOG("[Lua][error] GetGameInstance: GWorld is null");
                return nullptr;
            }
            return GWorld->GetGameInstance();
        }
    );

    // 전역 함수: TransitionToLevel()
    SharedLib.set_function("TransitionToLevel",
        [](const FString& LevelPathUTF8)
        {
            UE_LOG("[Lua] TransitionToLevel called from Lua with path: %s", LevelPathUTF8.c_str());

            if (!GWorld)
            {
                UE_LOG("[Lua][error] TransitionToLevel: GWorld is null");
                return;
            }

            FWideString LevelPathWide = UTF8ToWide(LevelPathUTF8);
            UE_LOG("[Lua] Calling GWorld->TransitionToLevel");
            GWorld->TransitionToLevel(LevelPathWide);
        }
    );

    // ════════════════════════════════════════════════════════════════════════
    // 아이템 시스템 바인딩
    // ════════════════════════════════════════════════════════════════════════

    // GetItemComponent: GameObject에서 ItemComponent 가져오기
    SharedLib.set_function("GetItemComponent",
        [this](sol::object Obj) -> sol::object
        {
            if (!Obj.is<FGameObject&>())
            {
                UE_LOG("[Lua][error] GetItemComponent: Expected GameObject");
                return sol::make_object(*Lua, sol::nil);
            }

            FGameObject& GameObject = Obj.as<FGameObject&>();
            if (GameObject.IsDestroyed()) return sol::make_object(*Lua, sol::nil);

            AActor* Actor = GameObject.GetOwner();
            if (!Actor || Actor->IsPendingDestroy()) return sol::make_object(*Lua, sol::nil);

            UClass* ItemCompClass = UClass::FindClass("UItemComponent");
            if (!ItemCompClass) return sol::make_object(*Lua, sol::nil);

            UActorComponent* ItemComp = Actor->GetComponent(ItemCompClass);
            if (!ItemComp) return sol::make_object(*Lua, sol::nil);

            return MakeCompProxy(*Lua, ItemComp, ItemCompClass);
        }
    );

    // HasItemComponent: GameObject가 ItemComponent를 가지고 있는지 확인
    SharedLib.set_function("HasItemComponent",
        [](sol::object Obj) -> bool
        {
            if (!Obj.is<FGameObject&>())
            {
                return false;
            }

            FGameObject& GameObject = Obj.as<FGameObject&>();
            if (GameObject.IsDestroyed()) return false;

            AActor* Actor = GameObject.GetOwner();
            if (!Actor || Actor->IsPendingDestroy()) return false;

            UClass* ItemCompClass = UClass::FindClass("UItemComponent");
            if (!ItemCompClass) return false;

            return Actor->GetComponent(ItemCompClass) != nullptr;
        }
    );

    // DisableCollision: GameObject의 모든 콜리전 비활성화
    SharedLib.set_function("DisableCollision",
        [](sol::object Obj) -> bool
        {
            if (!Obj.is<FGameObject&>())
            {
                UE_LOG("[Lua][error] DisableCollision: Expected GameObject");
                return false;
            }

            FGameObject& GameObject = Obj.as<FGameObject&>();
            AActor* Actor = GameObject.GetOwner();
            if (!Actor) return false;

            // 모든 PrimitiveComponent의 콜리전 비활성화
            const TSet<UActorComponent*>& Components = Actor->GetOwnedComponents();
            for (UActorComponent* Comp : Components)
            {
                UPrimitiveComponent* PrimComp = Cast<UPrimitiveComponent>(Comp);
                if (PrimComp)
                {
                    PrimComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
                    PrimComp->SetSimulatePhysics(false);
                }
            }
            return true;
        }
    );

    // DestroyObject: C++에 Actor 파괴 요청
    // Lua에서는 리스트에서 제거 후 이 함수 호출, 이후 해당 GameObject에 접근하지 않음
    SharedLib.set_function("DestroyObject",
        [](sol::object Obj) -> bool
        {
            if (!Obj.is<FGameObject&>())
            {
                UE_LOG("[Lua][error] DestroyObject: Expected GameObject");
                return false;
            }

            FGameObject& GameObject = Obj.as<FGameObject&>();

            // 이미 파괴된 경우 중복 처리 방지
            if (GameObject.IsDestroyed()) return false;

            AActor* Actor = GameObject.GetOwner();
            if (!Actor) return false;

            // FGameObject를 파괴됨으로 마킹 (Destroy 중/후 콜백에서 접근 방지)
            GameObject.MarkDestroyed();

            // 하이라이트 제거
            URenderer* Renderer = URenderManager::GetInstance().GetRenderer();
            if (Renderer)
            {
                UClass* StaticMeshCompClass = UClass::FindClass("UStaticMeshComponent");
                if (StaticMeshCompClass)
                {
                    UActorComponent* MeshComp = Actor->GetComponent(StaticMeshCompClass);
                    if (MeshComp)
                    {
                        Renderer->RemoveHighlight(MeshComp->InternalIndex);
                    }
                }
            }

            // FGameObject를 지연 삭제 큐에 추가하고 Actor의 포인터를 클리어
            // EndPlay에서 중복 처리하지 않도록 함
            if (GWorld && GWorld->GetLuaManager())
            {
                GWorld->GetLuaManager()->QueueGameObjectForDestruction(&GameObject);
            }
            Actor->ClearLuaGameObject();

            // Actor 파괴 요청 (엔진이 프레임 끝에 알아서 물리/컴포넌트 정리)
            Actor->Destroy();
            return true;
        }
    );

    // ════════════════════════════════════════════════════════════════════════
    // 하이라이트 시스템 바인딩 (아이템 아웃라인용)
    // ════════════════════════════════════════════════════════════════════════

    // AddHighlight: GameObject에 하이라이트 추가
    SharedLib.set_function("AddHighlight",
        [](sol::object Obj, sol::optional<sol::table> ColorTable) -> bool
        {
            URenderer* Renderer = URenderManager::GetInstance().GetRenderer();
            if (!Renderer) return false;

            if (!Obj.is<FGameObject&>())
            {
                UE_LOG("[Lua][error] AddHighlight: Expected GameObject");
                return false;
            }

            FGameObject& GameObject = Obj.as<FGameObject&>();
            if (GameObject.IsDestroyed()) return false;

            AActor* Actor = GameObject.GetOwner();
            if (!Actor || Actor->IsPendingDestroy()) return false;

            // StaticMeshComponent의 InternalIndex를 ObjectID로 사용
            // (IdBuffer에는 컴포넌트의 InternalIndex가 기록됨)
            UClass* StaticMeshCompClass = UClass::FindClass("UStaticMeshComponent");
            if (!StaticMeshCompClass) return false;

            UActorComponent* MeshComp = Actor->GetComponent(StaticMeshCompClass);
            if (!MeshComp) return false;

            uint32 ObjectID = MeshComp->InternalIndex;
            if (ObjectID == 0) return false;

            // 색상 파싱 (기본값: 노란색)
            FLinearColor OutlineColor(1.0f, 0.8f, 0.2f, 1.0f);
            if (ColorTable.has_value())
            {
                sol::table Color = ColorTable.value();
                OutlineColor.R = Color.get_or("R", Color.get_or(1, 1.0f));
                OutlineColor.G = Color.get_or("G", Color.get_or(2, 0.8f));
                OutlineColor.B = Color.get_or("B", Color.get_or(3, 0.2f));
                OutlineColor.A = Color.get_or("A", Color.get_or(4, 1.0f));
            }

            Renderer->AddHighlight(ObjectID, OutlineColor);
            return true;
        }
    );

    // RemoveHighlight: GameObject의 하이라이트 제거
    SharedLib.set_function("RemoveHighlight",
        [](sol::object Obj) -> bool
        {
            URenderer* Renderer = URenderManager::GetInstance().GetRenderer();
            if (!Renderer) return false;

            if (!Obj.is<FGameObject&>())
            {
                UE_LOG("[Lua][error] RemoveHighlight: Expected GameObject");
                return false;
            }

            FGameObject& GameObject = Obj.as<FGameObject&>();
            if (GameObject.IsDestroyed()) return false;

            AActor* Actor = GameObject.GetOwner();
            if (!Actor || Actor->IsPendingDestroy()) return false;

            // StaticMeshComponent의 InternalIndex를 ObjectID로 사용
            UClass* StaticMeshCompClass = UClass::FindClass("UStaticMeshComponent");
            if (!StaticMeshCompClass) return false;

            UActorComponent* MeshComp = Actor->GetComponent(StaticMeshCompClass);
            if (!MeshComp) return false;

            uint32 ObjectID = MeshComp->InternalIndex;
            if (ObjectID == 0) return false;

            Renderer->RemoveHighlight(ObjectID);
            return true;
        }
    );

    // ClearHighlights: 모든 하이라이트 제거
    SharedLib.set_function("ClearHighlights",
        []() -> void
        {
            URenderer* Renderer = URenderManager::GetInstance().GetRenderer();
            if (Renderer)
            {
                Renderer->ClearHighlights();
            }
        }
    );

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

void FLuaManager::RegisterComponentProxy(sol::state& Lua) {
    // Register proxy types for Array/Map/Struct
    LuaArrayProxy::RegisterLua(Lua);
    LuaMapProxy::RegisterLua(Lua);
    LuaStructProxy::RegisterLua(Lua);

    Lua.new_usertype<LuaComponentProxy>("Component",
        sol::meta_function::index,     &LuaComponentProxy::Index,
        sol::meta_function::new_index, &LuaComponentProxy::NewIndex
    );
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

    SharedLib.set_function("GetOwnerAs",
        [this](sol::object Obj, const FString& ClassName)
        {
            if (!Obj.is<FGameObject&>()) {
                UE_LOG("[Lua][error] GetOwnerAs: Expected GameObject\n");
                return sol::make_object(*Lua, sol::nil);
            }

            FGameObject& GameObject = Obj.as<FGameObject&>();
            AActor* Actor = GameObject.GetOwner();

            if (!Actor) {
                UE_LOG("[Lua][error] GetOwnerAs: Actor is null\n");
                return sol::make_object(*Lua, sol::nil);
            }

            UClass* Class = UClass::FindClass(ClassName);
            if (!Class) {
                UE_LOG("[Lua][error] GetOwnerAs: Class '%s' not found\n", ClassName.c_str());
                return sol::make_object(*Lua, sol::nil);
            }

            // Actor가 해당 클래스인지 확인 (Cast 검증)
            if (!Actor->IsA(Class)) {
                UE_LOG("[Lua][error] GetOwnerAs: Actor is not of type '%s'\n", ClassName.c_str());
                return sol::make_object(*Lua, sol::nil);
            }

            return MakeCompProxy(*Lua, Actor, Class);
        }
    );
}

void FLuaManager::ExposeGlobalFunctions()
{
    // APlayerCameraManager 클래스의 멤버 함수들 바인딩
    Lua->new_usertype<APlayerCameraManager>("PlayerCameraManager",
        sol::no_constructor,

        // --- StartCameraShake ---
        "StartCameraShake", sol::overload(
            // (Full) 5개 인수
            [](APlayerCameraManager* Self, float InDuration, float AmpLoc, float AmpRotDeg, float Frequency, int32 InPriority)
            {
                if (Self) Self->StartCameraShake(InDuration, AmpLoc, AmpRotDeg, Frequency, InPriority);
            },
            // (Priority 기본값 사용) 4개 인수
            [](APlayerCameraManager* Self, float InDuration, float AmpLoc, float AmpRotDeg, float Frequency)
            {
                if (Self) Self->StartCameraShake(InDuration, AmpLoc, AmpRotDeg, Frequency);
            }
        ),

        // --- StartFade ---
        "StartFade", sol::overload(
            // (Full) 5개 인수
            [](APlayerCameraManager* Self, float InDuration, float FromAlpha, float ToAlpha, const FLinearColor& InColor, int32 InPriority)
            {
                if (Self) Self->StartFade(InDuration, FromAlpha, ToAlpha, InColor, InPriority);
            },
            // (Priority 기본값 사용) 4개 인수
            [](APlayerCameraManager* Self, float InDuration, float FromAlpha, float ToAlpha, const FLinearColor& InColor)
            {
                if (Self) Self->StartFade(InDuration, FromAlpha, ToAlpha, InColor);
            },
            // (Color, Priority 기본값 사용) 3개 인수
            [](APlayerCameraManager* Self, float InDuration, float FromAlpha, float ToAlpha)
            {
                if (Self) Self->StartFade(InDuration, FromAlpha, ToAlpha);
            }
        ),

        // --- StartLetterBox ---
        "StartLetterBox", sol::overload(
            // (Full) 5개 인수
            [](APlayerCameraManager* Self, float InDuration, float Aspect, float BarHeight, const FLinearColor& InColor, int32 InPriority)
            {
                if (Self) Self->StartLetterBox(InDuration, Aspect, BarHeight, InColor, InPriority);
            },
            // (Priority 기본값 사용) 4개 인수
            [](APlayerCameraManager* Self, float InDuration, float Aspect, float BarHeight, const FLinearColor& InColor)
            {
                if (Self) Self->StartLetterBox(InDuration, Aspect, BarHeight, InColor);
            },
            // (Color, Priority 기본값 사용) 3개 인수
            [](APlayerCameraManager* Self, float InDuration, float Aspect, float BarHeight)
            {
                if (Self) Self->StartLetterBox(InDuration, Aspect, BarHeight);
            }
        ),

        // --- StartVignette (int 반환) ---
        "StartVignette", sol::overload(
            // (Full) 7개 인수
            [](APlayerCameraManager* Self, float InDuration, float Radius, float Softness, float Intensity, float Roundness, const FLinearColor& InColor, int32 InPriority) -> int
            {
                return Self ? Self->StartVignette(InDuration, Radius, Softness, Intensity, Roundness, InColor, InPriority) : -1;
            },
            // (Priority 기본값 사용) 6개 인수
            [](APlayerCameraManager* Self, float InDuration, float Radius, float Softness, float Intensity, float Roundness, const FLinearColor& InColor) -> int
            {
                return Self ? Self->StartVignette(InDuration, Radius, Softness, Intensity, Roundness, InColor) : -1;
            },
            // (Color, Priority 기본값 사용) 5개 인수
            [](APlayerCameraManager* Self, float InDuration, float Radius, float Softness, float Intensity, float Roundness) -> int
            {
                return Self ? Self->StartVignette(InDuration, Radius, Softness, Intensity, Roundness) : -1;
            }
        ),

        // --- UpdateVignette (int 반환) ---
        "UpdateVignette", sol::overload(
            // (Full) 8개 인수
            [](APlayerCameraManager* Self, int Idx, float InDuration, float Radius, float Softness, float Intensity, float Roundness, const FLinearColor& InColor, int32 InPriority) -> int
            {
                return Self ? Self->UpdateVignette(Idx, InDuration, Radius, Softness, Intensity, Roundness, InColor, InPriority) : -1;
            },
            // (Priority 기본값 사용) 7개 인수
            [](APlayerCameraManager* Self, int Idx, float InDuration, float Radius, float Softness, float Intensity, float Roundness, const FLinearColor& InColor) -> int
            {
                return Self ? Self->UpdateVignette(Idx, InDuration, Radius, Softness, Intensity, Roundness, InColor) : -1;
            },
            // (Color, Priority 기본값 사용) 6개 인수
            [](APlayerCameraManager* Self, int Idx, float InDuration, float Radius, float Softness, float Intensity, float Roundness) -> int
            {
                return Self ? Self->UpdateVignette(Idx, InDuration, Radius, Softness, Intensity, Roundness) : -1;
            }
        ),

        // --- AdjustVignette ---
        "AdjustVignette", sol::overload(
            // (Full) 7개 인수
            [](APlayerCameraManager* Self, float InDuration, float Radius, float Softness, float Intensity, float Roundness, const FLinearColor& InColor, int32 InPriority)
            {
                if (Self) Self->AdjustVignette(InDuration, Radius, Softness, Intensity, Roundness, InColor, InPriority);
            },
            // (Priority 기본값 사용) 6개 인수
            [](APlayerCameraManager* Self, float InDuration, float Radius, float Softness, float Intensity, float Roundness, const FLinearColor& InColor)
            {
                if (Self) Self->AdjustVignette(InDuration, Radius, Softness, Intensity, Roundness, InColor);
            },
            // (Color, Priority 기본값 사용) 5개 인수
            [](APlayerCameraManager* Self, float InDuration, float Radius, float Softness, float Intensity, float Roundness)
            {
                if (Self) Self->AdjustVignette(InDuration, Radius, Softness, Intensity, Roundness);
            }
        ),

        // --- DeleteVignette ---
        "DeleteVignette", [](APlayerCameraManager* Self)
        {
            if (Self) Self->DeleteVignette();
        },
            
        "SetViewTarget", [](APlayerCameraManager* self, LuaComponentProxy& Proxy)
        {
            // 타입 안정성 확인
            if (self && Proxy.Instance && Proxy.Class == UCameraComponent::StaticClass())
            {
                // 프록시에서 실제 컴포넌트 포인터 추출
                auto* CameraComp = static_cast<UCameraComponent*>(Proxy.Instance);
                self->SetViewCamera(CameraComp);
            }
        },

        "SetViewTargetWithBlend", [](APlayerCameraManager* self, LuaComponentProxy& Proxy, float InBlendTime)
        {
            // 타입 안정성 확인
            if (self && Proxy.Instance && Proxy.Class == UCameraComponent::StaticClass())
            {
                // 프록시에서 실제 컴포넌트 포인터 추출
                auto* CameraComp = static_cast<UCameraComponent*>(Proxy.Instance);
                self->SetViewCameraWithBlend(CameraComp, InBlendTime);
            }
        },

        // --- Gamma Correction ---
         // (Gamma Correction 기본값 사용) 1개 인수
        "StartGamma", [](APlayerCameraManager* Self, float Gamma)
        {
            if (Self)
            {
                Self->StartGamma(Gamma);
            }
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

    // 프레임 끝에서 지연 삭제 큐 정리
    FlushPendingDestroyGameObjects();
}

void FLuaManager::QueueGameObjectForDestruction(FGameObject* GameObject)
{
    if (GameObject)
    {
        PendingDestroyGameObjects.push_back(GameObject);
    }
}

void FLuaManager::FlushPendingDestroyGameObjects()
{
    // FGameObject를 즉시 삭제하지 않고 좀비 리스트로 이동
    // Lua에서 여전히 참조할 수 있으므로 PIE 종료 시까지 메모리 유지
    // (IsDestroyed() 체크로 안전하게 접근 가능)
    for (FGameObject* GO : PendingDestroyGameObjects)
    {
        if (GO)
        {
            ZombieGameObjects.push_back(GO);
        }
    }
    PendingDestroyGameObjects.clear();
}

void FLuaManager::ShutdownBeforeLuaClose()
{
    CoroutineSchedular.ShutdownBeforeLuaClose();

    // 남은 지연 삭제 큐 정리 (좀비 리스트로 이동)
    FlushPendingDestroyGameObjects();

    // 좀비 FGameObject들 최종 삭제 (PIE 종료 시점이므로 안전)
    for (FGameObject* GO : ZombieGameObjects)
    {
        if (GO)
        {
            delete GO;
        }
    }
    ZombieGameObjects.clear();

    FLuaBindRegistry::Get().Reset();

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