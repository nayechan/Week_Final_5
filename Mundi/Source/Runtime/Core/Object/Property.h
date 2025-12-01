#pragma once

// 프로퍼티 타입 열거형
enum class EPropertyType : uint8
{
	Unknown,
	Bool,
	Int32,
	Float,
	FVector,
	FVector2D,
	FLinearColor,
	FString,
	FName,
	ObjectPtr,      // UObject* 및 파생 타입
	Struct,
	Texture,        // UTexture* 타입 (리소스 선택 UI)
	SkeletalMesh,
	StaticMesh,     // UStaticMesh* 타입 (리소스 선택 UI)
	Material,		// UMaterial* 타입 (리소스 선택 UI)
	Array,			// TArray<T> - InnerType으로 T 지정
	Map,			// TMap<K, V> - KeyType으로 K, InnerType으로 V 지정
	SRV,
	ScriptFile,
	Sound,
	Curve,
	UClass,         // UClass* 타입 (클래스 선택 UI)
	Enum,           // enum class 타입 (콤보박스 UI)
	ParticleSystem, // UParticleSystem* 타입 (파티클 시스템 선택 UI)
	PhysicsAsset,   // UPhysicsAsset* 타입 (물리 에셋 선택 UI)
	DistributionFloat,   // FDistributionFloat 타입 (파티클 Distribution)
	DistributionVector,  // FDistributionVector 타입 (파티클 Distribution)
	DistributionColor,   // FDistributionColor 타입 (파티클 Distribution)
	BodyInstance,   // BodyInstance 타입
	// 추후 추가될 프로퍼티들은 직접 해줘야함.
	Count			// 요소 개수, 항상 마지막!
};

// 프로퍼티 소유자 종류
enum class EOwnerKind : uint8
{
	Unknown,
	Class,      // UObject 파생 클래스 (UActorComponent 등)
	Struct      // 구조체 (FTestStruct 등)
};

// 프로퍼티 메타데이터
struct FProperty
{
	const char* Name = nullptr;              // 프로퍼티 이름
	EPropertyType Type = EPropertyType::Unknown;  // 프로퍼티 타입
	EPropertyType InnerType = EPropertyType::Unknown; // TArray<T>의 'T' 또는 TMap<K,V>의 'V'
	EPropertyType KeyType = EPropertyType::Unknown;   // TMap<K,V>의 'K'
	const char* TypeName = nullptr;          // Struct/Enum 타입 이름 (예: "FTestTransform", "EAnimationMode")
	size_t Offset = 0;                       // 클래스 인스턴스 내 오프셋
	EOwnerKind OwnerKind = EOwnerKind::Unknown;   // 프로퍼티 소유자 종류 (Class/Struct)
	const char* Category = nullptr;          // UI 카테고리
	float MinValue = 0.0f;                   // 범위 최소값
	float MaxValue = 0.0f;                   // 범위 최대값
	bool bIsEditAnywhere = false;            // UI에 노출 여부
	const char* Tooltip = nullptr;           // 툴팁 설명

	TMap<FName, FString> Metadata;			 // 모든 부가 정보를 key-value 문자열로 저장합니다.

	// 객체 인스턴스에서 프로퍼티 값의 포인터를 가져옴
	template<typename T>
	T* GetValuePtr(void* ObjectInstance) const
	{
		return reinterpret_cast<T*>(reinterpret_cast<char*>(ObjectInstance) + Offset);
	}

	// 객체 인스턴스에서 프로퍼티 값의 const 포인터를 가져옴
	template<typename T>
	const T* GetValuePtr(const void* ObjectInstance) const
	{
		return reinterpret_cast<const T*>(reinterpret_cast<const char*>(ObjectInstance) + Offset);
	}
};
