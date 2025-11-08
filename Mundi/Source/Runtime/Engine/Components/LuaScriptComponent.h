#pragma once

#include "ActorComponent.h"
#include "Vector.h"
#include "LuaCoroutineScheduler.h"
#include "ULuaScriptComponent.generated.h"

namespace sol { class state; }
using state = sol::state;

class USceneComponent;

class ULuaScriptComponent : public UActorComponent
{
public:

	GENERATED_REFLECTION_BODY()

	ULuaScriptComponent();
	~ULuaScriptComponent() override;

public:
	void BeginPlay() override;
	void TickComponent(float DeltaTime) override;       // 매 프레임
	void EndPlay() override;							// 파괴/월드 제거 시

	void OnBeginOverlap(UPrimitiveComponent* MyComp, UPrimitiveComponent* OtherComp);
	void OnEndOverlap(UPrimitiveComponent* MyComp, UPrimitiveComponent* OtherComp);
	void OnHit(UPrimitiveComponent* MyComp, UPrimitiveComponent* OtherComp);

	bool Call(const char* FuncName, sol::variadic_args VarArgs); // 다른 클래스가 날 호출할 때 씀

	void CleanupLuaResources();
protected:
	// 이 컴포넌트가 실행할 .lua 스크립트 파일의 경로 (에디터에서 설정)
	UPROPERTY(EditAnywhere, Category="Script", Tooltip="Lua Script 파일 경로입니다")
	FString ScriptFilePath{};

	sol::state* Lua = nullptr;
	sol::environment Env{};

	/* 함수 캐시 */
	sol::protected_function FuncBeginPlay{};
	sol::protected_function FuncTick{};
	sol::protected_function FuncOnBeginOverlap{};
	sol::protected_function FuncOnEndOverlap{};
	sol::protected_function FuncOnHit{};
	sol::protected_function FuncEndPlay{};

	FDelegateHandle BeginHandleLua{};
	FDelegateHandle EndHandleLua{};
	
	bool bIsLuaCleanedUp = false;
};