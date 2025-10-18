#pragma once

#include "Property.h"
#include <type_traits>

// ===== 타입 자동 감지 템플릿 =====

// 기본 타입 감지 템플릿
template<typename T>
struct TPropertyTypeTraits
{
	static constexpr EPropertyType GetType()
	{
		if constexpr (std::is_same_v<T, bool>)
			return EPropertyType::Bool;
		else if constexpr (std::is_same_v<T, int32> || std::is_same_v<T, int>)
			return EPropertyType::Int32;
		else if constexpr (std::is_same_v<T, float>)
			return EPropertyType::Float;
		else if constexpr (std::is_same_v<T, FVector>)
			return EPropertyType::FVector;
		else if constexpr (std::is_same_v<T, FLinearColor>)
			return EPropertyType::FLinearColor;
		else if constexpr (std::is_same_v<T, FString>)
			return EPropertyType::FString;
		else if constexpr (std::is_same_v<T, FName>)
			return EPropertyType::FName;
		else if constexpr (std::is_pointer_v<T>)
			return EPropertyType::ObjectPtr;  // UObject* 및 파생 타입
		else
			return EPropertyType::Struct;
	}
};

// ===== 리플렉션 매크로 (수동 등록 방식) =====

// 헤더 파일에서 사용: 리플렉션 등록 함수 선언 + 자동 호출
// 사용법:
// class UMyComponent : public USceneComponent {
//     DECLARE_CLASS(UMyComponent, USceneComponent)
//     GENERATED_REFLECTION_BODY()
// protected:
//     float Intensity = 1.0f;
//     bool bIsEnabled = true;
// };
#define GENERATED_REFLECTION_BODY() \
private: \
	static void StaticRegisterProperties(); \
	inline static const bool bPropertiesRegistered = []() { \
		StaticRegisterProperties(); \
		return true; \
	}(); \
public:

// CPP 파일에서 사용: 프로퍼티 등록 헬퍼 매크로들
// 사용법: ADD_PROPERTY(타입, 변수, 카테고리);
// void UMyComponent::StaticRegisterProperties()
// {
//     UClass* Class = StaticClass();
//     ADD_PROPERTY(bool, bIsEnabled, "Light");
//     ADD_PROPERTY_RANGE(float, Intensity, "Light", 0.0f, 100.0f);
// }

// 범위 제한이 있는 프로퍼티 추가
#define ADD_PROPERTY_RANGE(VarType, VarName, CategoryName, MinVal, MaxVal, bEditAnywhere) \
	{ \
		FProperty Prop; \
		Prop.Name = #VarName; \
		Prop.Type = TPropertyTypeTraits<VarType>::GetType(); \
		Prop.Offset = offsetof(ThisClass_t, VarName); \
		Prop.Category = CategoryName; \
		Prop.MinValue = MinVal; \
		Prop.MaxValue = MaxVal; \
		Prop.bIsEditAnywhere = bEditAnywhere; \
		Class->AddProperty(Prop); \
	}

// 범위 제한이 없는 프로퍼티 추가
#define ADD_PROPERTY(VarType, VarName, CategoryName, bEditAnywhere) \
	ADD_PROPERTY_RANGE(VarType, VarName, CategoryName, 0.0f, 0.0f, bEditAnywhere)

// ===== 클래스 메타데이터 설정 매크로 =====
// StaticRegisterProperties() 함수 내에서 사용

// 스폰 가능한 액터로 설정
// 사용법: MARK_AS_SPAWNABLE("스태틱 메시", "스태틱 메시 액터를 구현합니다.")
#define MARK_AS_SPAWNABLE(InDisplayName, InDesc) \
	Class->bIsSpawnable = true; \
	Class->DisplayName = InDisplayName; \
	Class->Description = InDesc; \

// 컴포넌트로 설정
// 사용법: MARK_AS_COMPONENT("포인트 라이트", "포인트 라이트 컴포넌트를 추가합니다.")
#define MARK_AS_COMPONENT(InDisplayName, InDesc) \
	Class->bIsComponent = true; \
	Class->DisplayName = InDisplayName; \
	Class->Description = InDesc;
