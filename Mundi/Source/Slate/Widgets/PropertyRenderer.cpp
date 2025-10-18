#include "pch.h"
#include "PropertyRenderer.h"
#include "ImGui/imgui.h"
#include "Vector.h"
#include "Color.h"
#include "SceneComponent.h"

bool UPropertyRenderer::RenderProperty(const FProperty& Property, void* ObjectInstance)
{
	bool bChanged = false;

	switch (Property.Type)
	{
	case EPropertyType::Bool:
		bChanged = RenderBoolProperty(Property, ObjectInstance);
		break;

	case EPropertyType::Int32:
		bChanged = RenderInt32Property(Property, ObjectInstance);
		break;

	case EPropertyType::Float:
		bChanged = RenderFloatProperty(Property, ObjectInstance);
		break;

	case EPropertyType::FVector:
		bChanged = RenderVectorProperty(Property, ObjectInstance);
		break;

	case EPropertyType::FLinearColor:
		bChanged = RenderColorProperty(Property, ObjectInstance);
		break;

	case EPropertyType::FString:
		bChanged = RenderStringProperty(Property, ObjectInstance);
		break;

	case EPropertyType::FName:
		bChanged = RenderNameProperty(Property, ObjectInstance);
		break;

	case EPropertyType::ObjectPtr:
		bChanged = RenderObjectPtrProperty(Property, ObjectInstance);
		break;

	case EPropertyType::Struct:
		bChanged = RenderStructProperty(Property, ObjectInstance);
		break;

	default:
		ImGui::Text("%s: [Unknown Type]", Property.Name);
		break;
	}

	if (Property.Tooltip && Property.Tooltip[0] != '\0' && ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("%s", Property.Tooltip);
	}

	// SceneComponent는 Transform 프로퍼티가 변경되면 Setter를 통해 동기화
	if (bChanged && ObjectInstance)
	{
		UObject* Obj = static_cast<UObject*>(ObjectInstance);
		if (USceneComponent* SceneComponent = Cast<USceneComponent>(Obj))
		{
			// 프로퍼티 이름으로 Transform 프로퍼티 판별 후 Setter 호출
			if (strcmp(Property.Name, "RelativeRotationEuler") == 0)
			{
				FVector* EulerValue = Property.GetValuePtr<FVector>(ObjectInstance);
				SceneComponent->SetRelativeRotationEuler(*EulerValue);
			}
			else if (strcmp(Property.Name, "RelativeLocation") == 0)
			{
				FVector* LocationValue = Property.GetValuePtr<FVector>(ObjectInstance);
				SceneComponent->SetRelativeLocation(*LocationValue);
			}
			else if (strcmp(Property.Name, "RelativeScale") == 0)
			{
				FVector* ScaleValue = Property.GetValuePtr<FVector>(ObjectInstance);
				SceneComponent->SetRelativeScale(*ScaleValue);
			}
		}
	}

	return bChanged;
}

void UPropertyRenderer::RenderAllProperties(UObject* Object)
{
	if (!Object)
		return;

	UClass* Class = Object->GetClass();
	const TArray<FProperty>& Properties = Class->GetProperties();

	if (Properties.IsEmpty())
		return;

	// 카테고리별로 그룹화
	TMap<FString, TArray<const FProperty*>> CategorizedProps;

	for (const FProperty& Prop : Properties)
	{
		if (Prop.bIsEditAnywhere)
		{
			FString CategoryName = Prop.Category ? Prop.Category : "Default";
			if (!CategorizedProps.Contains(CategoryName))
			{
				CategorizedProps.Add(CategoryName, TArray<const FProperty*>());
			}
			CategorizedProps[CategoryName].Add(&Prop);
		}
	}

	// 카테고리별로 렌더링
	for (auto& Pair : CategorizedProps)
	{
		const FString& Category = Pair.first;
		const TArray<const FProperty*>& Props = Pair.second;

		ImGui::Separator();
		ImGui::Text("%s", Category.c_str());

		for (const FProperty* Prop : Props)
		{
			RenderProperty(*Prop, Object);
		}
	}
}

void UPropertyRenderer::RenderAllPropertiesWithInheritance(UObject* Object)
{
	if (!Object)
		return;

	UClass* Class = Object->GetClass();
	const TArray<FProperty>& AllProperties = Class->GetAllProperties();

	if (AllProperties.IsEmpty())
		return;

	// 카테고리별로 그룹화
	TMap<FString, TArray<const FProperty*>> CategorizedProps;

	for (const FProperty& Prop : AllProperties)
	{
		if (Prop.bIsEditAnywhere)
		{
			FString CategoryName = Prop.Category ? Prop.Category : "Default";
			if (!CategorizedProps.Contains(CategoryName))
			{
				CategorizedProps.Add(CategoryName, TArray<const FProperty*>());
			}
			CategorizedProps[CategoryName].Add(&Prop);
		}
	}

	// 카테고리별로 렌더링
	for (auto& Pair : CategorizedProps)
	{
		const FString& Category = Pair.first;
		const TArray<const FProperty*>& Props = Pair.second;

		ImGui::Separator();
		ImGui::Text("%s", Category.c_str());

		for (const FProperty* Prop : Props)
		{
			RenderProperty(*Prop, Object);
		}
	}
}

// ===== 타입별 렌더링 구현 =====

bool UPropertyRenderer::RenderBoolProperty(const FProperty& Prop, void* Instance)
{
	bool* Value = Prop.GetValuePtr<bool>(Instance);
	return ImGui::Checkbox(Prop.Name, Value);
}

bool UPropertyRenderer::RenderInt32Property(const FProperty& Prop, void* Instance)
{
	int32* Value = Prop.GetValuePtr<int32>(Instance);
	return ImGui::DragInt(Prop.Name, Value, 1.0f, (int)Prop.MinValue, (int)Prop.MaxValue);
}

bool UPropertyRenderer::RenderFloatProperty(const FProperty& Prop, void* Instance)
{
	float* Value = Prop.GetValuePtr<float>(Instance);

	// Min과 Max가 둘 다 0이면 범위 제한 없음
	if (Prop.MinValue == 0.0f && Prop.MaxValue == 0.0f)
	{
		return ImGui::DragFloat(Prop.Name, Value, 0.01f);
	}
	else
	{
		return ImGui::DragFloat(Prop.Name, Value, 0.01f, Prop.MinValue, Prop.MaxValue);
	}
}

bool UPropertyRenderer::RenderVectorProperty(const FProperty& Prop, void* Instance)
{
	FVector* Value = Prop.GetValuePtr<FVector>(Instance);
	return ImGui::DragFloat3(Prop.Name, &Value->X, 0.01f);
}

bool UPropertyRenderer::RenderColorProperty(const FProperty& Prop, void* Instance)
{
	FLinearColor* Value = Prop.GetValuePtr<FLinearColor>(Instance);
	return ImGui::ColorEdit4(Prop.Name, &Value->R);
}

bool UPropertyRenderer::RenderStringProperty(const FProperty& Prop, void* Instance)
{
	FString* Value = Prop.GetValuePtr<FString>(Instance);

	// ImGui::InputText는 char 버퍼를 사용하므로 변환 필요
	char Buffer[256];
	strncpy_s(Buffer, Value->c_str(), sizeof(Buffer) - 1);
	Buffer[sizeof(Buffer) - 1] = '\0';

	if (ImGui::InputText(Prop.Name, Buffer, sizeof(Buffer)))
	{
		*Value = Buffer;
		return true;
	}

	return false;
}

bool UPropertyRenderer::RenderNameProperty(const FProperty& Prop, void* Instance)
{
	FName* Value = Prop.GetValuePtr<FName>(Instance);

	// FName은 읽기 전용으로 표시
	FString NameStr = Value->ToString();
	ImGui::Text("%s: %s", Prop.Name, NameStr.c_str());

	return false;
}

bool UPropertyRenderer::RenderObjectPtrProperty(const FProperty& Prop, void* Instance)
{
	UObject** ObjPtr = Prop.GetValuePtr<UObject*>(Instance);

	if (*ObjPtr)
	{
		// 객체가 있으면 클래스 이름과 객체 이름 표시
		UClass* ObjClass = (*ObjPtr)->GetClass();
		FString ObjName = (*ObjPtr)->GetName();
		ImGui::Text("%s: %s (%s)", Prop.Name, ObjName.c_str(), ObjClass->Name);
	}
	else
	{
		// nullptr이면 None 표시
		ImGui::Text("%s: None", Prop.Name);
	}

	return false; // 읽기 전용
}

bool UPropertyRenderer::RenderStructProperty(const FProperty& Prop, void* Instance)
{
	// Struct는 읽기 전용으로 표시
	// FVector, FLinearColor 등 주요 타입은 이미 별도로 처리됨
	ImGui::Text("%s: [Struct]", Prop.Name);
	return false;
}
