#pragma once

#include "Property.h"
#include "Object.h"

// 프로퍼티 렌더러
// 리플렉션 정보를 기반으로 ImGui UI를 자동 생성
class UPropertyRenderer
{
public:
	// 단일 프로퍼티 렌더링
	static bool RenderProperty(const FProperty& Property, void* ObjectInstance);

	// 객체의 모든 프로퍼티를 카테고리별로 렌더링
	static void RenderAllProperties(UObject* Object);

	// 객체의 모든 프로퍼티를 카테고리별로 렌더링 (부모 클래스 프로퍼티 포함)
	static void RenderAllPropertiesWithInheritance(UObject* Object);

private:
	// 타입별 렌더링 함수들
	static bool RenderBoolProperty(const FProperty& Prop, void* Instance);
	static bool RenderInt32Property(const FProperty& Prop, void* Instance);
	static bool RenderFloatProperty(const FProperty& Prop, void* Instance);
	static bool RenderVectorProperty(const FProperty& Prop, void* Instance);
	static bool RenderColorProperty(const FProperty& Prop, void* Instance);
	static bool RenderStringProperty(const FProperty& Prop, void* Instance);
	static bool RenderNameProperty(const FProperty& Prop, void* Instance);
	static bool RenderObjectPtrProperty(const FProperty& Prop, void* Instance);
	static bool RenderStructProperty(const FProperty& Prop, void* Instance);
};
