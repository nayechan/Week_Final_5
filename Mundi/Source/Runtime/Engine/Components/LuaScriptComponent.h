#pragma once
#include "ActorComponent.h"
#include "Vector.h"

namespace sol { class state; }
using state = sol::state;

class USceneComponent;

class ULuaScriptComponent : public UActorComponent
{
public:
    DECLARE_CLASS(ULuaScriptComponent, UActorComponent)
	GENERATED_REFLECTION_BODY()
    DECLARE_DUPLICATE(ULuaScriptComponent)

    ULuaScriptComponent();
	~ULuaScriptComponent() override;
protected:
   

public:
	void BeginPlay() override;
	void TickComponent(float DeltaTime) override;       // 매 프레임
	void EndPlay(EEndPlayReason Reason) override;       // 파괴/월드 제거 시
	
	void OnOverlap(const AActor* Other);

protected:
    // 이 컴포넌트가 실행할 .lua 스크립트 파일의 경로 (에디터에서 설정)
    FString ScriptFilePath;

	sol::state* Lua;
};