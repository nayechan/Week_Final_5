#include "pch.h"

FString UObject::GetName()
{
    return ObjectName.ToString();
}

FString UObject::GetComparisonName()
{
    return FString();
}

void UObject::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
}

// 리플렉션 기반 자동 직렬화 (현재 클래스의 프로퍼티만 처리)
void UObject::AutoSerialize(const bool bInIsLoading, JSON& InOutHandle, UClass* TargetClass)
{
	const TArray<FProperty>& Properties = TargetClass->GetProperties();

	for (const FProperty& Prop : Properties)
	{
		switch (Prop.Type)
		{
		case EPropertyType::Bool:
		{
			bool* Value = Prop.GetValuePtr<bool>(this);
			if (bInIsLoading)
				FJsonSerializer::ReadBool(InOutHandle, Prop.Name, *Value);
			else
				InOutHandle[Prop.Name] = *Value;
			break;
		}
		case EPropertyType::Int32:
		{
			int32* Value = Prop.GetValuePtr<int32>(this);
			if (bInIsLoading)
				FJsonSerializer::ReadInt32(InOutHandle, Prop.Name, *Value);
			else
				InOutHandle[Prop.Name] = *Value;
			break;
		}
		case EPropertyType::Float:
		{
			float* Value = Prop.GetValuePtr<float>(this);
			if (bInIsLoading)
				FJsonSerializer::ReadFloat(InOutHandle, Prop.Name, *Value);
			else
				InOutHandle[Prop.Name] = *Value;
			break;
		}
		case EPropertyType::FVector:
		{
			FVector* Value = Prop.GetValuePtr<FVector>(this);
			if (bInIsLoading)
				FJsonSerializer::ReadVector(InOutHandle, Prop.Name, *Value);
			else
				InOutHandle[Prop.Name] = FJsonSerializer::VectorToJson(*Value);
			break;
		}
		case EPropertyType::FLinearColor:
		{
			FLinearColor* Value = Prop.GetValuePtr<FLinearColor>(this);
			if (bInIsLoading)
			{
				FVector4 ColorVec;
				FJsonSerializer::ReadVector4(InOutHandle, Prop.Name, ColorVec);
				*Value = FLinearColor(ColorVec);
			}
			else
			{
				InOutHandle[Prop.Name] = FJsonSerializer::Vector4ToJson(Value->ToFVector4());
			}
			break;
		}
		case EPropertyType::FString:
		{
			FString* Value = Prop.GetValuePtr<FString>(this);
			if (bInIsLoading)
				FJsonSerializer::ReadString(InOutHandle, Prop.Name, *Value);
			else
				InOutHandle[Prop.Name] = Value->c_str();
			break;
		}
		// FName, ObjectPtr, Struct 등은 필요시 추가
		}
	}
}

void UObject::DuplicateSubObjects()
{
    UUID = GenerateUUID(); // UUID는 고유값이므로 새로 생성
}

UObject* UObject::Duplicate() const
{
    UObject* NewObject = ObjectFactory::DuplicateObject<UObject>(this); // 모든 멤버 얕은 복사

    NewObject->DuplicateSubObjects(); // 얕은 복사한 멤버들에 대해 메뉴얼하게 깊은 복사 수행

    return NewObject;
}


