#include "pch.h"
#include "PropertyRenderer.h"
#include "ImGui/imgui.h"
#include "Vector.h"
#include "Color.h"
#include "SceneComponent.h"
#include "ResourceManager.h"
#include "Texture.h"
#include "StaticMesh.h"
#include "Material.h"
#include "BillboardComponent.h"
#include "DecalComponent.h"
#include "StaticMeshComponent.h"
#include "LightComponentBase.h"

TArray<FString> UPropertyRenderer::CachedMaterialPaths;
TArray<const char*> UPropertyRenderer::CachedMaterialItems;
TArray<FString> UPropertyRenderer::CachedShaderPaths;
TArray<const char*> UPropertyRenderer::CachedShaderItems;
TArray<FString> UPropertyRenderer::CachedTexturePaths;
TArray<const char*> UPropertyRenderer::CachedTextureItems;

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

	case EPropertyType::Texture:
		bChanged = RenderTextureProperty(Property, ObjectInstance);
		break;

	case EPropertyType::StaticMesh:
		bChanged = RenderStaticMeshProperty(Property, ObjectInstance);
		break;

	case EPropertyType::Material:
		bChanged = RenderMaterialProperty(Property, ObjectInstance);
		break;

	case EPropertyType::Array:
		switch (Property.InnerType)
		{
		case EPropertyType::Material:
			bChanged = RenderMaterialArrayProperty(Property, ObjectInstance);
			break;
		}
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

		// LightComponent는 Light 프로퍼티가 변경되면 UpdateLightData를 호출하여 동기화
		if (ULightComponentBase* LightComponent = Cast<ULightComponentBase>(Obj))
		{
			// Light 관련 프로퍼티가 변경되면 UpdateLightData 호출
			if (strcmp(Property.Name, "LightColor") == 0 ||
				strcmp(Property.Name, "Intensity") == 0 ||
				strcmp(Property.Name, "Temperature") == 0 ||
				strcmp(Property.Name, "bIsEnabled") == 0)
			{
				LightComponent->UpdateLightData();
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

// ===== 리소스 캐싱 =====

void UPropertyRenderer::CacheMaterialResources()
{
	// 이미 캐시되어 있다면 다시 로드하지 않습니다.
	if (!CachedMaterialPaths.IsEmpty())
	{
		return;
	}

	UResourceManager& ResMgr = UResourceManager::GetInstance();

	// 1. 머티리얼
	CachedMaterialPaths = ResMgr.GetAllFilePaths<UMaterial>();
	CachedMaterialItems.Add("None");
	for (const FString& path : CachedMaterialPaths)
	{
		CachedMaterialItems.push_back(path.c_str());
	}

	// 2. 셰이더
	CachedShaderPaths = ResMgr.GetAllFilePaths<UShader>();
	CachedShaderItems.Add("None");
	for (const FString& path : CachedShaderPaths)
	{
		CachedShaderItems.push_back(path.c_str());
	}

	// 3. 텍스처
	CachedTexturePaths = ResMgr.GetAllFilePaths<UTexture>();
	CachedTextureItems.Add("None");
	for (const FString& path : CachedTexturePaths)
	{
		CachedTextureItems.push_back(path.c_str());
	}
}

void UPropertyRenderer::ClearMaterialResourcesCache()
{
	CachedMaterialPaths.Empty();
	CachedMaterialItems.Empty();
	CachedShaderPaths.Empty();
	CachedShaderItems.Empty();
	CachedTexturePaths.Empty();
	CachedTextureItems.Empty();
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

	return ImGui::DragFloat3(Prop.Name, &Value->X, 0.1f);
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

bool UPropertyRenderer::RenderTextureProperty(const FProperty& Prop, void* Instance)
{
	UTexture** TexturePtr = Prop.GetValuePtr<UTexture*>(Instance);

	FString CurrentPath;
	if (*TexturePtr)
	{
		CurrentPath = (*TexturePtr)->GetTextureName();
	}

	const TArray<FString> TexturePaths = UResourceManager::GetInstance().GetAllFilePaths<UTexture>();

	if (TexturePaths.empty())
	{
		ImGui::Text("%s: <No Textures>", Prop.Name);
		return false;
	}

	TArray<const char*> Items;
	Items.reserve(TexturePaths.size());
	for (const FString& path : TexturePaths)
		Items.push_back(path.c_str());

	int SelectedIdx = -1;
	for (int i = 0; i < static_cast<int>(TexturePaths.size()); ++i)
	{
		if (TexturePaths[i] == CurrentPath)
		{
			SelectedIdx = i;
			break;
		}
	}

	ImGui::SetNextItemWidth(240);
	if (ImGui::Combo(Prop.Name, &SelectedIdx, Items.data(), static_cast<int>(Items.size())))
	{
		if (SelectedIdx >= 0 && SelectedIdx < static_cast<int>(TexturePaths.size()))
		{
			*TexturePtr = UResourceManager::GetInstance().Load<UTexture>(TexturePaths[SelectedIdx]);
			if (*TexturePtr)
			{
				(*TexturePtr)->SetTextureName(TexturePaths[SelectedIdx]);
			}

			// 컴포넌트별 Setter 호출
			UObject* Obj = static_cast<UObject*>(Instance);
			if (UBillboardComponent* Billboard = Cast<UBillboardComponent>(Obj))
			{
				Billboard->SetTextureName(TexturePaths[SelectedIdx]);
			}
			else if (UDecalComponent* Decal = Cast<UDecalComponent>(Obj))
			{
				Decal->SetDecalTexture(TexturePaths[SelectedIdx]);
			}

			return true;
		}
	}

	return false;
}

bool UPropertyRenderer::RenderStaticMeshProperty(const FProperty& Prop, void* Instance)
{
	UStaticMesh** MeshPtr = Prop.GetValuePtr<UStaticMesh*>(Instance);

	FString CurrentPath;
	if (*MeshPtr)
	{
		CurrentPath = (*MeshPtr)->GetFilePath();
	}

	const TArray<FString> MeshPaths = UResourceManager::GetInstance().GetAllFilePaths<UStaticMesh>();

	if (MeshPaths.empty())
	{
		ImGui::Text("%s: <No Meshes>", Prop.Name);
		return false;
	}

	TArray<const char*> Items;
	Items.reserve(MeshPaths.size());
	for (const FString& path : MeshPaths)
		Items.push_back(path.c_str());

	int SelectedIdx = -1;
	for (int i = 0; i < static_cast<int>(MeshPaths.size()); ++i)
	{
		if (MeshPaths[i] == CurrentPath)
		{
			SelectedIdx = i;
			break;
		}
	}

	ImGui::SetNextItemWidth(240);
	if (ImGui::Combo(Prop.Name, &SelectedIdx, Items.data(), static_cast<int>(Items.size())))
	{
		if (SelectedIdx >= 0 && SelectedIdx < static_cast<int>(MeshPaths.size()))
		{
			// 컴포넌트별 Setter 호출
			UObject* Object = static_cast<UObject*>(Instance);
			if (UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>(Object))
			{
				StaticMeshComponent->SetStaticMesh(MeshPaths[SelectedIdx]);
			}
			else
			{
				*MeshPtr = UResourceManager::GetInstance().Load<UStaticMesh>(MeshPaths[SelectedIdx]);
			}
			return true;
		}
	}

	return false;
}

bool UPropertyRenderer::RenderMaterialProperty(const FProperty& Prop, void* Instance)
{
	// 렌더링에 필요한 리소스가 캐시되었는지 확인하고, 없으면 로드합니다.
	CacheMaterialResources();

	UMaterial** MaterialPtr = Prop.GetValuePtr<UMaterial*>(Instance);
	if (!MaterialPtr)
	{
		ImGui::Text("%s: [Invalid Material Ptr]", Prop.Name);
		return false;
	}

	UObject* OwningObject = static_cast<UObject*>(Instance);

	// Material 렌더 함수 호출
	return RenderSingleMaterialSlot(Prop.Name, MaterialPtr, OwningObject, 0);	// 단일 슬롯은 0번 인덱스
}

bool UPropertyRenderer::RenderMaterialArrayProperty(const FProperty& Prop, void* Instance)
{
	// 렌더링에 필요한 리소스가 캐시되었는지 확인하고, 없으면 로드합니다.
	CacheMaterialResources();

	TArray<UMaterial*>* MaterialSlots = Prop.GetValuePtr<TArray<UMaterial*>>(Instance);
	if (!MaterialSlots)
	{
		ImGui::Text("%s: [Invalid Array Ptr]", Prop.Name);
		return false;
	}

	UObject* OwningObject = static_cast<UObject*>(Instance);
	bool bArrayChanged = false;

	// 배열의 각 요소를 순회하며 헬퍼 함수 호출 (매개변수 없음)
	for (uint32 MaterialIndex = 0; MaterialIndex < MaterialSlots->Num(); ++MaterialIndex)
	{
		FString Label = FString(Prop.Name) + " [" + std::to_string(MaterialIndex) + "]";
		UMaterial** MaterialPtr = &(*MaterialSlots)[MaterialIndex];

		if (RenderSingleMaterialSlot(Label.c_str(), MaterialPtr, OwningObject, MaterialIndex))
		{
			bArrayChanged = true;
		}
	}

	return bArrayChanged;
}

bool UPropertyRenderer::RenderSingleMaterialSlot(const char* Label, UMaterial** MaterialPtr, UObject* OwningObject, uint32 MaterialIndex)
{
	bool bElementChanged = false;
	UMaterial* CurrentMaterial = *MaterialPtr;

	// --- 5-1. UMaterial 애셋 선택 콤보박스 ---
	FString CurrentMaterialPath = (CurrentMaterial) ? CurrentMaterial->GetFilePath() : "None";

	int SelectedMaterialIdx = 0; // "None"
	for (int j = 0; j < (int)CachedMaterialPaths.size(); ++j)
	{
		if (CachedMaterialPaths[j] == CurrentMaterialPath)
		{
			SelectedMaterialIdx = j + 1;
			break;
		}
	}

	ImGui::SetNextItemWidth(240);
	if (ImGui::Combo(Label, &SelectedMaterialIdx, CachedMaterialItems.data(), static_cast<int>(CachedMaterialItems.size())))
	{
		FString SelectedPath = "None";
		if (SelectedMaterialIdx > 0)
		{
			SelectedPath = CachedMaterialPaths[SelectedMaterialIdx - 1];
		}

		if (UPrimitiveComponent* PrimitiveComponent = Cast<UPrimitiveComponent>(OwningObject))
		{
			PrimitiveComponent->SetMaterialByName(MaterialIndex, SelectedPath);
		}
		else
		{
			// 다른 컴포넌트는 Set 함수를 모르기 때문에 그냥 대입 처리
			*MaterialPtr = (SelectedMaterialIdx == 0) ? nullptr : UResourceManager::GetInstance().Load<UMaterial>(SelectedPath);
		}

		bElementChanged = true;
		CurrentMaterial = *MaterialPtr;
	}

	// --- 5-2. UMaterial 내부 프로퍼티 렌더링 (셰이더, 텍스처) ---
	if (CurrentMaterial)
	{
		ImGui::Indent();

		// --- 5-2a. 셰이더 렌더링 (읽기 전용, 비활성화된 InputText 사용) ---
		UShader* CurrentShader = CurrentMaterial->GetShader();
		FString CurrentShaderPath = (CurrentShader) ? CurrentShader->GetFilePath() : "None";

		// ImGui::InputText에 사용하기 위해 경로를 복사할 버퍼
		char ShaderPathBuffer[512]; // 경로 길이에 맞춰 버퍼 크기 조정
		strncpy_s(ShaderPathBuffer, sizeof(ShaderPathBuffer), CurrentShaderPath.c_str(), _TRUNCATE);

		FString ShaderLabel = "Shader##" + FString(Label);
		ImGui::SetNextItemWidth(220);

		// ImGui::BeginDisabled(true)로 감싸 위젯을 시각적으로 비활성화합니다.
		ImGui::BeginDisabled(true);

		// InputText는 계속 사용하되, 비활성화된 상태로 둡니다.
		ImGui::InputText(ShaderLabel.c_str(), ShaderPathBuffer, sizeof(ShaderPathBuffer), ImGuiInputTextFlags_ReadOnly);

		// 비활성화 상태를 해제합니다.
		ImGui::EndDisabled();

		// --- 5-2b. 텍스처 슬롯 렌더링 ---
		for (uint8 TexSlotIndex = 0; TexSlotIndex < (uint8)EMaterialTextureSlot::Max; ++TexSlotIndex)
		{
			EMaterialTextureSlot Slot = static_cast<EMaterialTextureSlot>(TexSlotIndex);
			UTexture* CurrentTexture = CurrentMaterial->GetTexture(Slot);
			FString CurrentTexturePath = (CurrentTexture) ? CurrentTexture->GetTextureName() : "None";

			int SelectedTextureIdx = 0; // "None"

			for (int j = 0; j < (int)CachedTexturePaths.size(); ++j)
			{
				if (CachedTexturePaths[j] == CurrentTexturePath)
				{
					SelectedTextureIdx = j + 1;
					break;
				}
			}

			const char* SlotName = "Unknown Slot";
			switch (Slot)
			{
			case EMaterialTextureSlot::Diffuse:
				SlotName = "Diffuse Texture";
				break;
			case EMaterialTextureSlot::Normal:
				SlotName = "Normal Texture";
				break;
				// ... (다른 텍스처 슬롯이 EMaterialTextureSlot에 추가되면 여기에도 case 추가) ...
			}

			FString TextureLabel = FString(SlotName) + "##" + Label;
			ImGui::SetNextItemWidth(220);
			if (ImGui::Combo(TextureLabel.c_str(), &SelectedTextureIdx, CachedTextureItems.data(), static_cast<int>(CachedTextureItems.size())))
			{
				FString SelectedTexturePath = (SelectedTextureIdx == 0) ? "None" : CachedTexturePaths[SelectedTextureIdx - 1];
				CurrentMaterial->SetTexture(Slot, SelectedTexturePath);
				bElementChanged = true;
			}
		}

		ImGui::Unindent();
	}

	return bElementChanged;
}
