#pragma once
#include "Property.h"
#include "Color.h"
#include "StaticMesh.h"
#include "Texture.h"
#include "TSubclassOf.h"
#include "Distribution.h"
//#include "BodyInstance.h"
#include <type_traits>

struct FBodyInstance;

// ===== 타입 자동 감지 템플릿 =====

// 기본 타입 감지 템플릿
// 참고: UStaticMesh, UTexture 등 리소스 타입은 명시적 매크로(ADD_PROPERTY_STATICMESH, ADD_PROPERTY_TEXTURE)를 사용하므로
// 여기서는 포인터로 처리하고 순환 의존성을 방지합니다.
template<typename T>
struct TPropertyTypeTraits
{
	static constexpr EPropertyType GetType()
	{
		if constexpr (std::is_same_v<T, bool>)
			return EPropertyType::Bool;
		else if constexpr (std::is_same_v<T, int32> || std::is_same_v<T, int> ||
		                   std::is_same_v<T, uint32> || std::is_same_v<T, unsigned int>)
			return EPropertyType::Int32;  // Lua number는 부호 구분 없음, int32로 통합
		else if constexpr (std::is_same_v<T, int64> || std::is_same_v<T, long long> ||
		                   std::is_same_v<T, uint64> || std::is_same_v<T, unsigned long long>)
			return EPropertyType::Int32;  // Lua number로 변환 (정밀도 손실 가능)
		else if constexpr (std::is_same_v<T, float>)
			return EPropertyType::Float;
		else if constexpr (std::is_same_v<T, double>)
			return EPropertyType::Float;  // Lua number는 기본적으로 double
		else if constexpr (std::is_same_v<T, FVector>)
			return EPropertyType::FVector;
		else if constexpr (std::is_same_v<T, FVector2D>)
			return EPropertyType::FVector2D;
		else if constexpr (std::is_same_v<T, FLinearColor>)
			return EPropertyType::FLinearColor;
		else if constexpr (std::is_same_v<T, FString>)
			return EPropertyType::FString;
		else if constexpr (std::is_same_v<T, FName>)
			return EPropertyType::FName;
		else if constexpr (std::is_same_v<T, UClass*>)
			return EPropertyType::UClass;  // UClass* 타입 (클래스 선택 UI)
		else if constexpr (std::is_pointer_v<T>)
			return EPropertyType::ObjectPtr;  // UObject* 및 파생 타입 (UStaticMesh*, UTexture* 포함)
		else if constexpr (std::is_enum_v<T>)
			return EPropertyType::Enum;  // enum class 타입
		else
			return EPropertyType::Struct;
	}
};

// TSubclassOf<T> 특수화
template<typename T>
struct TPropertyTypeTraits<TSubclassOf<T>>
{
	static constexpr EPropertyType GetType()
	{
		return EPropertyType::UClass;  // TSubclassOf도 UClass로 처리
	}
};

// Distribution 타입 특수화
template<>
struct TPropertyTypeTraits<FDistributionFloat>
{
	static constexpr EPropertyType GetType()
	{
		return EPropertyType::DistributionFloat;
	}
};

template<>
struct TPropertyTypeTraits<FDistributionVector>
{
	static constexpr EPropertyType GetType()
	{
		return EPropertyType::DistributionVector;
	}
};

template<>
struct TPropertyTypeTraits<FDistributionColor>
{
	static constexpr EPropertyType GetType()
	{
		return EPropertyType::DistributionColor;
	}
};

template<>
struct TPropertyTypeTraits<FBodyInstance>
{
	static constexpr EPropertyType GetType()
	{
		return EPropertyType::BodyInstance;
	}
};

// =
// ===== 코드 생성 마커 매크로 =====

// 코드 생성기가 파싱할 마커 (컴파일 시에는 빈 매크로)
// 실제 동작은 generate.py가 생성한 .generated.cpp에서 이루어짐

// 프로퍼티 자동 생성 마커
// 사용법: UPROPERTY(EditAnywhere, Category="Mesh")
#ifndef UPROPERTY
#define UPROPERTY(...)
#endif

// 함수 자동 바인딩 마커
// 사용법: UFUNCTION(LuaBind, DisplayName="SetColor")
#ifndef UFUNCTION
#define UFUNCTION(...)
#endif

// 클래스 메타데이터 마커
// 사용법: UCLASS(DisplayName="방향성 라이트", Description="태양광과 같은 평행광 액터")
#ifndef UCLASS
#define UCLASS(...)
#endif

// 구조체 메타데이터 마커
// 사용법: USTRUCT(DisplayName="트랜스폼", Description="위치, 회전, 크기 정보")
// 참고: USTRUCT도 UCLASS와 동일하게 GENERATED_REFLECTION_BODY() 매크로를 사용합니다.
//       코드 생성기가 .generated.h 파일에서 적절한 코드를 생성합니다.
#ifndef USTRUCT
#define USTRUCT(...)
#endif

// Enum 메타데이터 마커
// 사용법: UENUM()
//         enum class EAnimationMode : uint8 { ... };
#ifndef UENUM
#define UENUM(...)
#endif

// ===== 리플렉션 매크로 (수동 등록 방식) =====

// 헤더 파일에서 사용: 클래스 선언 + 리플렉션 등록 함수 선언 + 자동 호출
//
// 이제 DECLARE_CLASS와 DECLARE_DUPLICATE의 기능을 포함합니다.
// 실제 내용은 코드 생성기가 생성하는 .generated.h 파일에 정의됩니다.
//
// Static 초기화 순서 안전성:
// - StaticRegisterProperties()는 lambda를 통해 StaticClass() 최초 호출 시 자동 실행됨
// - DECLARE_CLASS 매크로 내부의 static UClass Cls가 먼저 초기화된 후
//   lambda 내부의 bRegistered가 초기화되므로 순서가 보장됨
// - 프로퍼티 등록은 프로그램 시작 시 1회만 발생하며, 런타임 중 변경 불가
//
// 사용법:
// #include "MyComponent.generated.h"  // <-- 코드 생성기가 만든 파일
//
// class UMyComponent : public USceneComponent {
//     GENERATED_REFLECTION_BODY()
// protected:
//     UPROPERTY(EditAnywhere, Category="Light")
//     float Intensity = 1.0f;
// };
//
// 참고:
// - DECLARE_CLASS와 DECLARE_DUPLICATE는 더 이상 수동으로 작성할 필요가 없습니다.
// - 반드시 클래스 정의 전에 .generated.h 파일을 include해야 합니다.
#define GENERATED_REFLECTION_BODY() \
    CURRENT_CLASS_GENERATED_BODY

// CPP 파일에서 사용: 프로퍼티 등록 헬퍼 매크로들
// 사용법:
// BEGIN_PROPERTIES(UMyComponent)
//     MARK_AS_COMPONENT("My Component", "...")
//     ADD_PROPERTY(bool, bIsEnabled, "Light", true)
//     ADD_PROPERTY_RANGE(float, Intensity, "Light", 0.0f, 100.0f, true)
// END_PROPERTIES()

// StaticRegisterProperties 함수 시작 (UCLASS용)
#define BEGIN_PROPERTIES(ClassName) \
	void ClassName::StaticRegisterProperties() \
	{ \
		UClass* Class = StaticClass(); \
		using ThisClass_t = ClassName; \
		const EOwnerKind CurrentOwnerKind = EOwnerKind::Class; \

// StaticRegisterProperties 함수 시작 (USTRUCT용)
// 사용법:
// BEGIN_STRUCT_PROPERTIES(FTransform)
//     ADD_PROPERTY(FVector, Location, "Transform", true, "위치")
//     ADD_PROPERTY(FVector, Rotation, "Transform", true, "회전")
// END_PROPERTIES()
//
// ===== 설계 노트: 매크로 재사용을 위한 Duck Typing =====
//
// 중요: 변수 이름을 의도적으로 'Class'로 유지합니다!
//
// 이유:
// - 모든 ADD_PROPERTY 계열 매크로(약 20개)가 'Class->' 접두사를 사용합니다
// - UStruct*를 Struct로 명명하면 모든 매크로를 복제해야 합니다 (140줄 중복)
// - UStruct는 UClass와 동일한 인터페이스를 제공합니다:
//   * AddProperty(const FProperty&) 메서드
//   * Properties 멤버 (TArray<FProperty>)
//   * GetAllProperties() 메서드
//
// 따라서 변수 이름만 통일하면 타입이 달라도 모든 매크로가 작동합니다.
// 이것이 "Duck Typing" 원리입니다:
// "오리처럼 걷고, 오리처럼 꽥꽥거리면, 그것은 오리다"
//
// 제한 사항:
// - MARK_AS_SPAWNABLE/COMPONENT는 사용 불가 (UClass 전용 필드)
// - 코드 생성기(property_generator.py)가 struct는 MARK 매크로를 생성하지 않음
//
#define BEGIN_STRUCT_PROPERTIES(StructName) \
	void StructName::StaticRegisterProperties() \
	{ \
		UStruct* Class = StaticStruct(); \
		using ThisClass_t = StructName; \
		const EOwnerKind CurrentOwnerKind = EOwnerKind::Struct; \

// StaticRegisterProperties 함수 종료
#define END_PROPERTIES() \
	}

// 범위 제한이 있는 프로퍼티 추가
#define ADD_PROPERTY_RANGE(VarType, VarName, CategoryName, MinVal, MaxVal, bEditAnywhere, ...) \
	{ \
		static_assert(std::is_array_v<std::remove_reference_t<decltype(CategoryName)>>, \
		              "CategoryName must be a string literal!"); \
		FProperty Prop; \
		Prop.Name = #VarName; \
		Prop.Type = TPropertyTypeTraits<VarType>::GetType(); \
		Prop.TypeName = #VarType; \
		Prop.Offset = offsetof(ThisClass_t, VarName); \
		Prop.Category = CategoryName; \
		Prop.MinValue = MinVal; \
		Prop.MaxValue = MaxVal; \
		Prop.bIsEditAnywhere = bEditAnywhere; \
		Prop.Tooltip = "" __VA_ARGS__; \
		Prop.OwnerKind = CurrentOwnerKind; \
		Class->AddProperty(Prop); \
	}

// 범위 제한이 없는 프로퍼티 추가
#define ADD_PROPERTY(VarType, VarName, CategoryName, bEditAnywhere, ...) \
	ADD_PROPERTY_RANGE(VarType, VarName, CategoryName, 0.0f, 0.0f, bEditAnywhere, __VA_ARGS__)

// 리소스 타입 전용 프로퍼티 매크로
// Texture 프로퍼티 추가
#define ADD_PROPERTY_TEXTURE(VarType, VarName, CategoryName, bEditAnywhere, ...) \
	{ \
		static_assert(std::is_array_v<std::remove_reference_t<decltype(CategoryName)>>, \
		              "CategoryName must be a string literal!"); \
		FProperty Prop; \
		Prop.Name = #VarName; \
		Prop.Type = EPropertyType::Texture; \
		Prop.Offset = offsetof(ThisClass_t, VarName); \
		Prop.Category = CategoryName; \
		Prop.bIsEditAnywhere = bEditAnywhere; \
		Prop.Tooltip = "" __VA_ARGS__; \
		Prop.OwnerKind = CurrentOwnerKind; \
		Class->AddProperty(Prop); \
	}

// StaticMesh 프로퍼티 추가
#define ADD_PROPERTY_STATICMESH(VarType, VarName, CategoryName, bEditAnywhere, ...) \
	{ \
		static_assert(std::is_array_v<std::remove_reference_t<decltype(CategoryName)>>, \
		              "CategoryName must be a string literal!"); \
		FProperty Prop; \
		Prop.Name = #VarName; \
		Prop.Type = EPropertyType::StaticMesh; \
		Prop.Offset = offsetof(ThisClass_t, VarName); \
		Prop.Category = CategoryName; \
		Prop.bIsEditAnywhere = bEditAnywhere; \
		Prop.Tooltip = "" __VA_ARGS__; \
		Prop.OwnerKind = CurrentOwnerKind; \
		Class->AddProperty(Prop); \
	}

// StaticMesh 프로퍼티 추가
#define ADD_PROPERTY_SKELETALMESH(VarType, VarName, CategoryName, bEditAnywhere, ...) \
	{ \
		static_assert(std::is_array_v<std::remove_reference_t<decltype(CategoryName)>>, \
		              "CategoryName must be a string literal!"); \
		FProperty Prop; \
		Prop.Name = #VarName; \
		Prop.Type = EPropertyType::SkeletalMesh; \
		Prop.Offset = offsetof(ThisClass_t, VarName); \
		Prop.Category = CategoryName; \
		Prop.bIsEditAnywhere = bEditAnywhere; \
		Prop.Tooltip = "" __VA_ARGS__; \
		Prop.OwnerKind = CurrentOwnerKind; \
		Class->AddProperty(Prop); \
	}

// Audio 프로퍼티 추가
#define ADD_PROPERTY_AUDIO(VarType, VarName, CategoryName, bEditAnywhere, ...) \
	{ \
		static_assert(std::is_array_v<std::remove_reference_t<decltype(CategoryName)>>, \
		              "CategoryName must be a string literal!"); \
		FProperty Prop; \
		Prop.Name = #VarName; \
		Prop.Type = EPropertyType::Sound; \
		Prop.Offset = offsetof(ThisClass_t, VarName); \
		Prop.Category = CategoryName; \
		Prop.bIsEditAnywhere = bEditAnywhere; \
		Prop.Tooltip = "" __VA_ARGS__; \
		Prop.OwnerKind = CurrentOwnerKind; \
		Class->AddProperty(Prop); \
	}

#define ADD_PROPERTY_CURVE(VarType, VarName, CategoryName, bEditAnywhere, ...) \
	{ \
		static_assert(std::is_array_v<std::remove_reference_t<decltype(CategoryName)>>, \
		              "CategoryName must be a string literal!"); \
		FProperty Prop; \
		Prop.Name = #VarName; \
		Prop.Type = EPropertyType::Curve; \
		Prop.Offset = offsetof(ThisClass_t, VarName); \
		Prop.Category = CategoryName; \
		Prop.bIsEditAnywhere = bEditAnywhere; \
		Prop.Tooltip = "" __VA_ARGS__; \
		Prop.OwnerKind = CurrentOwnerKind; \
		Class->AddProperty(Prop); \
	}
#define ADD_PROPERTY_COUNT(VarType, VarName, CategoryName, bEditAnywhere, ...) \
	{ \
		static_assert(std::is_array_v<std::remove_reference_t<decltype(CategoryName)>>, \
		              "CategoryName must be a string literal!"); \
		FProperty Prop; \
		Prop.Name = #VarName; \
		Prop.Type = EPropertyType::Count; \
		Prop.Offset = offsetof(ThisClass_t, VarName); \
		Prop.Category = CategoryName; \
		Prop.bIsEditAnywhere = bEditAnywhere; \
		Prop.Tooltip = "" __VA_ARGS__; \
		Prop.OwnerKind = CurrentOwnerKind; \
		Class->AddProperty(Prop); \
	}

// Material 프로퍼티 추가
#define ADD_PROPERTY_MATERIAL(VarType, VarName, CategoryName, bEditAnywhere, ...) \
	{ \
		static_assert(std::is_array_v<std::remove_reference_t<decltype(CategoryName)>>, \
		              "CategoryName must be a string literal!"); \
		FProperty Prop; \
		Prop.Name = #VarName; \
		Prop.Type = EPropertyType::Material; \
		Prop.Offset = offsetof(ThisClass_t, VarName); \
		Prop.Category = CategoryName; \
		Prop.bIsEditAnywhere = bEditAnywhere; \
		Prop.Tooltip = "" __VA_ARGS__; \
		Prop.OwnerKind = CurrentOwnerKind; \
		Class->AddProperty(Prop); \
	}

// ParticleSystem 프로퍼티 추가
#define ADD_PROPERTY_PARTICLESYSTEM(VarType, VarName, CategoryName, bEditAnywhere, ...) \
	{ \
		static_assert(std::is_array_v<std::remove_reference_t<decltype(CategoryName)>>, \
		              "CategoryName must be a string literal!"); \
		FProperty Prop; \
		Prop.Name = #VarName; \
		Prop.Type = EPropertyType::ParticleSystem; \
		Prop.Offset = offsetof(ThisClass_t, VarName); \
		Prop.Category = CategoryName; \
		Prop.bIsEditAnywhere = bEditAnywhere; \
		Prop.Tooltip = "" __VA_ARGS__; \
		Prop.OwnerKind = CurrentOwnerKind; \
		Class->AddProperty(Prop); \
	}

// PhysicsAsset 프로퍼티 추가
#define ADD_PROPERTY_PHYSICSASSET(VarType, VarName, CategoryName, bEditAnywhere, ...) \
	{ \
		static_assert(std::is_array_v<std::remove_reference_t<decltype(CategoryName)>>, \
		              "CategoryName must be a string literal!"); \
		FProperty Prop; \
		Prop.Name = #VarName; \
		Prop.Type = EPropertyType::PhysicsAsset; \
		Prop.Offset = offsetof(ThisClass_t, VarName); \
		Prop.Category = CategoryName; \
		Prop.bIsEditAnywhere = bEditAnywhere; \
		Prop.Tooltip = "" __VA_ARGS__; \
		Prop.OwnerKind = CurrentOwnerKind; \
		Class->AddProperty(Prop); \
	}

// PhysicalMaterial 프로퍼티 추가
#define ADD_PROPERTY_PHYSICALMATERIAL(VarType, VarName, CategoryName, bEditAnywhere, ...) \
	{ \
		static_assert(std::is_array_v<std::remove_reference_t<decltype(CategoryName)>>, \
		              "CategoryName must be a string literal!"); \
		FProperty Prop; \
		Prop.Name = #VarName; \
		Prop.Type = EPropertyType::PhysicalMaterial; \
		Prop.Offset = offsetof(ThisClass_t, VarName); \
		Prop.Category = CategoryName; \
		Prop.bIsEditAnywhere = bEditAnywhere; \
		Prop.Tooltip = "" __VA_ARGS__; \
		Prop.OwnerKind = CurrentOwnerKind; \
		Class->AddProperty(Prop); \
	}

// TArray<...> 프로퍼티 추가
// 첫 번째 인자로 배열의 내부 요소 타입을 받습니다.
#define ADD_PROPERTY_ARRAY(InnerPropertyType, VarName, CategoryName, bEditAnywhere, ...) \
{ \
	static_assert(std::is_array_v<std::remove_reference_t<decltype(CategoryName)>>, \
				  "CategoryName must be a string literal!"); \
	FProperty Prop; \
	Prop.Name = #VarName; \
	Prop.Type = EPropertyType::Array; \
	Prop.InnerType = InnerPropertyType; /* EPropertyType::Material 같은 enum 값 */ \
	Prop.Offset = offsetof(ThisClass_t, VarName); \
	Prop.Category = CategoryName; \
	Prop.bIsEditAnywhere = bEditAnywhere; \
	Prop.Tooltip = "" __VA_ARGS__; \
	Prop.OwnerKind = CurrentOwnerKind; \
	Class->AddProperty(Prop); \
}

// TArray<UStruct> 프로퍼티 추가
// 첫 번째 인자로 배열의 내부 Struct 타입을 받습니다.
// 예: ADD_PROPERTY_STRUCT_ARRAY(FParticleBurst, BurstList, "Burst", true)
#define ADD_PROPERTY_STRUCT_ARRAY(StructType, VarName, CategoryName, bEditAnywhere, ...) \
{ \
	static_assert(std::is_array_v<std::remove_reference_t<decltype(CategoryName)>>, \
				  "CategoryName must be a string literal!"); \
	FProperty Prop; \
	Prop.Name = #VarName; \
	Prop.Type = EPropertyType::Array; \
	Prop.InnerType = EPropertyType::Struct; \
	Prop.TypeName = #StructType; /* 내부 Struct 타입 이름 */ \
	Prop.Offset = offsetof(ThisClass_t, VarName); \
	Prop.Category = CategoryName; \
	Prop.bIsEditAnywhere = bEditAnywhere; \
	Prop.Tooltip = "" __VA_ARGS__; \
	Prop.OwnerKind = CurrentOwnerKind; \
	Class->AddProperty(Prop); \
}

// TMap<K, V> 프로퍼티 추가
// 첫 번째 인자: Key 타입, 두 번째 인자: Value 타입
#define ADD_PROPERTY_MAP(KeyPropertyType, ValuePropertyType, VarName, CategoryName, bEditAnywhere, ...) \
{ \
	static_assert(std::is_array_v<std::remove_reference_t<decltype(CategoryName)>>, \
				  "CategoryName must be a string literal!"); \
	FProperty Prop; \
	Prop.Name = #VarName; \
	Prop.Type = EPropertyType::Map; \
	Prop.KeyType = KeyPropertyType; \
	Prop.InnerType = ValuePropertyType; /* Value 타입 */ \
	Prop.Offset = offsetof(ThisClass_t, VarName); \
	Prop.Category = CategoryName; \
	Prop.bIsEditAnywhere = bEditAnywhere; \
	Prop.Tooltip = "" __VA_ARGS__; \
	Prop.OwnerKind = CurrentOwnerKind; \
	Class->AddProperty(Prop); \
}

// Material 프로퍼티 추가
#define ADD_PROPERTY_SRV(VarType, VarName, CategoryName, bEditAnywhere, ...) \
	{ \
		static_assert(std::is_array_v<std::remove_reference_t<decltype(CategoryName)>>, \
		              "CategoryName must be a string literal!"); \
		FProperty Prop; \
		Prop.Name = #VarName; \
		Prop.Type = EPropertyType::SRV; \
		Prop.Offset = offsetof(ThisClass_t, VarName); \
		Prop.Category = CategoryName; \
		Prop.bIsEditAnywhere = bEditAnywhere; \
		Prop.Tooltip = "" __VA_ARGS__; \
		Prop.OwnerKind = CurrentOwnerKind; \
		Class->AddProperty(Prop); \
	}

// 스크립트 파일 프로퍼티 추가
// InExtension 예: ".lua" 또는 "lua"
#define ADD_PROPERTY_SCRIPT(VarType, VarName, CategoryName, InExtension, bEditAnywhere, ...) \
	{ \
		static_assert(std::is_array_v<std::remove_reference_t<decltype(CategoryName)>>, \
					  "CategoryName must be a string literal!"); \
		static_assert(std::is_array_v<std::remove_reference_t<decltype(InExtension)>>, \
					  "InExtension must be a string literal!");\
		FProperty Prop; \
		Prop.Name = #VarName; \
		Prop.Type = EPropertyType::ScriptFile; \
		Prop.Offset = offsetof(ThisClass_t, VarName); \
		Prop.Category = CategoryName; \
		Prop.bIsEditAnywhere = bEditAnywhere; \
		Prop.Tooltip = "" __VA_ARGS__; \
		Prop.Metadata.Add(FName("FileExtension"), InExtension); \
		Prop.OwnerKind = CurrentOwnerKind; \
		Class->AddProperty(Prop); \
	}


  
// 클래스 메타데이터 설정 매크로
// StaticRegisterProperties() 함수 내에서 사용

// 스폰 가능한 액터로 설정
// 사용법: MARK_AS_SPAWNABLE("스태틱 메시", "스태틱 메시 액터를 구현합니다.")
#define MARK_AS_SPAWNABLE(InDisplayName, InDesc) \
	static_assert(std::is_array_v<std::remove_reference_t<decltype(InDisplayName)>>, \
	              "InDisplayName must be a string literal!"); \
	static_assert(std::is_array_v<std::remove_reference_t<decltype(InDesc)>>, \
	              "InDesc must be a string literal!"); \
	assert(!Class->bIsSpawnable && "MARK_AS_SPAWNABLE called multiple times!"); \
	Class->bIsSpawnable = true; \
	Class->DisplayName = InDisplayName; \
	Class->Description = InDesc; \

// 컴포넌트로 설정
// 사용법: MARK_AS_COMPONENT("포인트 라이트", "포인트 라이트 컴포넌트를 추가합니다.")
#define MARK_AS_COMPONENT(InDisplayName, InDesc) \
	static_assert(std::is_array_v<std::remove_reference_t<decltype(InDisplayName)>>, \
	              "InDisplayName must be a string literal"); \
	static_assert(std::is_array_v<std::remove_reference_t<decltype(InDesc)>>, \
	              "InDesc must be a string literal"); \
	assert(!Class->bIsComponent && "MARK_AS_COMPONENT called multiple times!"); \
	Class->bIsComponent = true; \
	Class->DisplayName = InDisplayName; \
	Class->Description = InDesc;

#define CREATE_EDITOR_COMPONENT(InVariableName, Type)\
	InVariableName = NewObject<Type>();\
	InVariableName->SetOwner(this->GetOwner());\
	InVariableName->SetupAttachment(this, EAttachmentRule::KeepRelative);\
	this->GetOwner()->AddOwnedComponent(InVariableName);\
	InVariableName->SetEditability(false);\
	InVariableName->SetHiddenInGame(true);

