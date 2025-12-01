#include "pch.h"
#include "PropertyRenderer.h"
#include "ImGui/imgui.h"
#include "Vector.h"
#include "Color.h"
#include "Distribution.h"
#include "SceneComponent.h"
#include "ResourceManager.h"
#include "Texture.h"
#include "StaticMesh.h"
#include "Material.h"
#include "BillboardComponent.h"
#include "DecalComponent.h"
#include "StaticMeshComponent.h"
#include "LightComponentBase.h"
#include "LightComponent.h"
#include "PointLightComponent.h"
#include "SpotLightComponent.h"
#include "PlatformProcess.h"
#include "SkeletalMeshComponent.h"
#include "SlateManager.h"
#include "ImGui/imgui_curve.hpp"
#include "Source/Runtime/Engine/Viewer/EditorAssetPreviewContext.h"
#include "EnumRegistry.generated.h"
#include "ParticleModule.h"
#include "ParticleSystemComponent.h"
#include "ParticleSystem.h"
#include "PhysicsAsset.h"

// 정적 멤버 변수 초기화
TArray<FString> UPropertyRenderer::CachedSkeletalMeshPaths;
TArray<FString> UPropertyRenderer::CachedSkeletalMeshItems;
TArray<FString> UPropertyRenderer::CachedStaticMeshPaths;
TArray<FString> UPropertyRenderer::CachedStaticMeshItems;
TArray<FString> UPropertyRenderer::CachedMaterialPaths;
TArray<const char*> UPropertyRenderer::CachedMaterialItems;
TArray<FString> UPropertyRenderer::CachedShaderPaths;
TArray<const char*> UPropertyRenderer::CachedShaderItems;
TArray<FString> UPropertyRenderer::CachedTexturePaths;
TArray<const char*> UPropertyRenderer::CachedTextureItems;
TArray<FString> UPropertyRenderer::CachedSoundPaths;
TArray<const char*> UPropertyRenderer::CachedSoundItems;
TArray<FString> UPropertyRenderer::CachedScriptPaths;
TArray<const char*> UPropertyRenderer::CachedScriptItems;
TArray<FString> UPropertyRenderer::CachedParticleSystemPaths;
TArray<FString> UPropertyRenderer::CachedParticleSystemItems;
TArray<FString> UPropertyRenderer::CachedPhysicsAssetPaths;
TArray<FString> UPropertyRenderer::CachedPhysicsAssetItems;

static bool ItemsGetter(void* Data, int Index, const char** CItem)
{
	TArray<FString>* Items = (TArray<FString>*)Data;

	if (Index < 0 || Index >= Items->Num())
	{
		return false;
	}

	*CItem = (*Items)[Index].c_str();
	return true;
}

bool UPropertyRenderer::RenderProperty(const FProperty& Property, void* ObjectInstance)
{
	// 렌더링에 필요한 리소스가 캐시되었는지 확인하고, 없으면 로드합니다.
	CacheResources();

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

	case EPropertyType::FVector2D:
		bChanged = RenderVector2DProperty(Property, ObjectInstance);
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

	case EPropertyType::Enum:
		bChanged = RenderEnumProperty(Property, ObjectInstance);
		break;

	case EPropertyType::Map:
		bChanged = RenderMapProperty(Property, ObjectInstance);
		break;

	case EPropertyType::Texture:
		bChanged = RenderTextureProperty(Property, ObjectInstance);
		break;

	case EPropertyType::SkeletalMesh:
		bChanged = RenderSkeletalMeshProperty(Property, ObjectInstance);
		break;

	case EPropertyType::StaticMesh:
		bChanged = RenderStaticMeshProperty(Property, ObjectInstance);
		break;

	case EPropertyType::Material:
		bChanged = RenderMaterialProperty(Property, ObjectInstance);
		break;

	case EPropertyType::SRV:
		bChanged = RenderSRVProperty(Property, ObjectInstance);
		break;

	case EPropertyType::ScriptFile:
		bChanged = RenderScriptFileProperty(Property, ObjectInstance);
		break;

	case EPropertyType::Curve:
		bChanged = RenderCurveProperty(Property, ObjectInstance);
		break;

	case EPropertyType::ParticleSystem:
		bChanged = RenderParticleSystemProperty(Property, ObjectInstance);
		break;

	case EPropertyType::PhysicsAsset:
		bChanged = RenderPhysicsAssetProperty(Property, ObjectInstance);
		break;

	case EPropertyType::Array:
		switch (Property.InnerType)
		{
		case EPropertyType::Material:
			bChanged = RenderMaterialArrayProperty(Property, ObjectInstance);
			break;
		case EPropertyType::Sound:
			// Render array of USound* via simple combo per element
			{
				TArray<USound*>* Arr = Property.GetValuePtr<TArray<USound*>>(ObjectInstance);
				if (Arr)
				{
					if (ImGui::Button("Add Sound")) { Arr->Add(nullptr); bChanged = true; }
					for (int i = 0; i < Arr->Num(); ++i)
					{
						ImGui::PushID(i);
						USound* cur = (*Arr)[i];
						USound* neu = nullptr;
						FString label = FString(Property.Name) + " [" + std::to_string(i) + "]";
						if (RenderSoundSelectionComboSimple(label.c_str(), cur, neu))
						{
							(*Arr)[i] = neu; bChanged = true;
						}
						ImGui::SameLine();
						if (ImGui::Button("Remove")) { Arr->RemoveAt(i); --i; bChanged = true; ImGui::PopID(); continue; }
						ImGui::PopID();
					}
				}
			}
			break;
		case EPropertyType::Int32:
			{
				TArray<int32>* Arr = Property.GetValuePtr<TArray<int32>>(ObjectInstance);
				if (Arr)
				{
					if (ImGui::TreeNode(Property.Name))
					{
						if (ImGui::Button("Add")) { Arr->Add(0); bChanged = true; }
						for (int i = 0; i < Arr->Num(); ++i)
						{
							ImGui::PushID(i);
							FString label = "[" + std::to_string(i) + "]";
							if (ImGui::DragInt(label.c_str(), &(*Arr)[i], 1))
								bChanged = true;
							ImGui::SameLine();
							if (ImGui::Button("X")) { Arr->RemoveAt(i); --i; bChanged = true; ImGui::PopID(); continue; }
							ImGui::PopID();
						}
						ImGui::TreePop();
					}
				}
			}
			break;
		case EPropertyType::Float:
			{
				TArray<float>* Arr = Property.GetValuePtr<TArray<float>>(ObjectInstance);
				if (Arr)
				{
					if (ImGui::TreeNode(Property.Name))
					{
						if (ImGui::Button("Add")) { Arr->Add(0.0f); bChanged = true; }
						for (int i = 0; i < Arr->Num(); ++i)
						{
							ImGui::PushID(i);
							FString label = "[" + std::to_string(i) + "]";
							if (ImGui::DragFloat(label.c_str(), &(*Arr)[i], 0.1f))
								bChanged = true;
							ImGui::SameLine();
							if (ImGui::Button("X")) { Arr->RemoveAt(i); --i; bChanged = true; ImGui::PopID(); continue; }
							ImGui::PopID();
						}
						ImGui::TreePop();
					}
				}
			}
			break;
		case EPropertyType::FString:
			{
				TArray<FString>* Arr = Property.GetValuePtr<TArray<FString>>(ObjectInstance);
				if (Arr)
				{
					if (ImGui::TreeNode(Property.Name))
					{
						if (ImGui::Button("Add")) { Arr->Add(""); bChanged = true; }
						for (int i = 0; i < Arr->Num(); ++i)
						{
							ImGui::PushID(i);
							FString label = "[" + std::to_string(i) + "]";
							char buffer[256];
							strncpy_s(buffer, (*Arr)[i].c_str(), sizeof(buffer) - 1);
							if (ImGui::InputText(label.c_str(), buffer, sizeof(buffer)))
							{
								(*Arr)[i] = buffer;
								bChanged = true;
							}
							ImGui::SameLine();
							if (ImGui::Button("X")) { Arr->RemoveAt(i); --i; bChanged = true; ImGui::PopID(); continue; }
							ImGui::PopID();
						}
						ImGui::TreePop();
					}
				}
			}
			break;
		case EPropertyType::Struct:
			// TArray<Struct> 렌더링 - 동적 배열 처리
			bChanged = RenderStructArrayProperty(Property, ObjectInstance);
			break;
		default:
			ImGui::Text("%s: [Unsupported Array Type]", Property.Name);
			break;
		}
		break;

	case EPropertyType::Sound:
		bChanged = RenderSoundProperty(Property, ObjectInstance);
		break;

	case EPropertyType::DistributionFloat:
		bChanged = RenderDistributionFloatProperty(Property, ObjectInstance);
		break;

	case EPropertyType::DistributionVector:
		bChanged = RenderDistributionVectorProperty(Property, ObjectInstance);
		break;

	case EPropertyType::DistributionColor:
		bChanged = RenderDistributionColorProperty(Property, ObjectInstance);
		break;
	
	case EPropertyType::BodyInstance:
		bChanged = RenderBodyInstanceProperty(Property, ObjectInstance);
		break;

	default:
		ImGui::Text("%s: [Unknown Type]", Property.Name);
		break;
	}

	// 배열 타입은 내부에서 개별 프로퍼티마다 툴팁을 처리하므로 스킵
	if (Property.Type != EPropertyType::Array &&
		Property.Tooltip && Property.Tooltip[0] != '\0' && ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("%s", Property.Tooltip);
	}

	// SceneComponent/LightComponent는 특정 프로퍼티가 변경되면 Setter를 통해 동기화
	// OwnerKind가 Class인 경우에만 UObject로 캐스팅 가능 (Struct인 경우 크래시 발생)
	if (bChanged && ObjectInstance && Property.OwnerKind == EOwnerKind::Class)
	{
		UObject* Obj = static_cast<UObject*>(ObjectInstance);

		// SceneComponent Transform 프로퍼티 처리
		if (USceneComponent* SceneComponent = Cast<USceneComponent>(Obj))
		{
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

		// LightComponent Light 프로퍼티가 변경되면 UpdateLightData 호출
		if (ULightComponentBase* LightComponent = Cast<ULightComponentBase>(Obj))
		{
			LightComponent->UpdateLightData();
		}

		// ParticleSystemComponent DebugParticleType 프로퍼티가 변경되면 디버그 파티클 시스템 재생성
		if (UParticleSystemComponent* ParticleComponent = Cast<UParticleSystemComponent>(Obj))
		{
			if (strcmp(Property.Name, "DebugParticleType") == 0)
			{
				ParticleComponent->RefreshDebugParticleSystem();
			}
		}
	}

	return bChanged;
}

void UPropertyRenderer::RenderProperties(const TArray<FProperty>& Properties, UObject* Object)
{
	if (Properties.IsEmpty())
		return;

	// 1. 카테고리 (삽입 순서 보장용 TArray)
	TArray<TPair<FString, TArray<const FProperty*>>> CategorizedProps;

	// 2. 카테고리 인덱스 (빠른 검색용 TMap)
	TMap<FString, int32> CategoryIndexMap;

	for (const FProperty& Prop : Properties)
	{
		if (Prop.bIsEditAnywhere)
		{
			FString CategoryName = Prop.Category ? Prop.Category : "Default";

			// 3. 맵에서 이미 카테고리가 있는지 빠르게 검색
			int32* IndexPtr = CategoryIndexMap.Find(CategoryName);

			if (IndexPtr)
			{
				// 4-A. 이미 존재하는 카테고리면, TArray의 해당 인덱스에 프로퍼티만 추가
				CategorizedProps[*IndexPtr].second.Add(&Prop);
			}
			else
			{
				// 4-B. 새 카테고리면, TArray에 새 항목을 추가
				TArray<const FProperty*> NewPropArray;
				NewPropArray.Add(&Prop);

				// TArray에 실제 데이터 추가 (MakePair는 std::make_pair와 동일)
				int32 NewIndex = CategorizedProps.Add(TPair<FString, TArray<const FProperty*>>(CategoryName, NewPropArray));

				// TMap에 "이 카테고리는 TArray의 NewIndex에 있다"고 기록
				CategoryIndexMap.Add(CategoryName, NewIndex);
			}
		}
	}

	// 5. TArray를 순회하면 '삽입 순서'가 완벽하게 보장됩니다.
	// (순서를 뒤집고 싶다면 여기서 CategorizedProps.rbegin() / rend() 사용)
	for (auto& Pair : CategorizedProps)
	{
		const FString& Category = Pair.first;
		const TArray<const FProperty*>& Props = Pair.second;

		ImGui::Separator();
		ImGui::Text("%s", Category.c_str());

		for (const FProperty* Prop : Props)
		{
			ImGui::PushID(Prop); // 프로퍼티 포인터로 고유 ID 푸시
			RenderProperty(*Prop, Object);
			ImGui::PopID();
		}
	}
}

void UPropertyRenderer::RenderAllProperties(UObject* Object)
{
	if (!Object)
		return;

	UClass* Class = Object->GetClass();
	const TArray<FProperty>& Properties = Class->GetProperties();

	RenderProperties(Properties, Object);
}

bool UPropertyRenderer::RenderAllPropertiesWithInheritance(UObject* Object)
{
	if (!Object)
		return false;

	bool bAnyChanged = false;
	UClass* Class = Object->GetClass();
	const TArray<FProperty>& AllProperties = Class->GetAllProperties();

	// 카테고리별 그룹핑
	TArray<TPair<FString, TArray<const FProperty*>>> CategorizedProps;
	TMap<FString, int32> CategoryIndexMap;

	for (const FProperty& Prop : AllProperties)
	{
		if (Prop.bIsEditAnywhere)
		{
			FString CategoryName = Prop.Category ? Prop.Category : "Default";
			int32* IndexPtr = CategoryIndexMap.Find(CategoryName);

			if (IndexPtr)
			{
				CategorizedProps[*IndexPtr].second.Add(&Prop);
			}
			else
			{
				TArray<const FProperty*> NewPropArray;
				NewPropArray.Add(&Prop);
				int32 NewIndex = CategorizedProps.Add(
					TPair<FString, TArray<const FProperty*>>(CategoryName, NewPropArray));
				CategoryIndexMap.Add(CategoryName, NewIndex);
			}
		}
	}

	// 카테고리별 렌더링 및 변경 감지
	for (auto& Pair : CategorizedProps)
	{
		const FString& Category = Pair.first;
		const TArray<const FProperty*>& Props = Pair.second;

		ImGui::Separator();
		ImGui::Text("%s", Category.c_str());

		for (const FProperty* Prop : Props)
		{
			ImGui::PushID(Prop);
			if (RenderProperty(*Prop, Object))
			{
				bAnyChanged = true;
			}
			ImGui::PopID();
		}
	}

	return bAnyChanged;
}

// ===== 리소스 캐싱 =====

void UPropertyRenderer::CacheResources()
{
	// NOTE: 추후에 변경된 것이 있으면 다시 캐싱하도록 변경 필요
	//ClearResourcesCache();

	UResourceManager& ResMgr = UResourceManager::GetInstance();


	// 1. 스태틱 메시
	if (CachedStaticMeshPaths.IsEmpty() && CachedStaticMeshItems.IsEmpty())
	{
		CachedStaticMeshPaths = ResMgr.GetAllFilePaths<UStaticMesh>();
		for (const FString& path : CachedStaticMeshPaths)
		{
			// 파일명만 추출해서 표시
			std::filesystem::path fsPath(UTF8ToWide(path));
			CachedStaticMeshItems.push_back(WideToUTF8(fsPath.filename().wstring()));
		}
		CachedStaticMeshPaths.Insert("", 0);
		CachedStaticMeshItems.Insert("None", 0);
	}

	if (CachedSkeletalMeshPaths.IsEmpty() && CachedSkeletalMeshItems.IsEmpty())
	{
		CachedSkeletalMeshPaths = ResMgr.GetAllFilePaths<USkeletalMesh>();
		for (const FString& path : CachedSkeletalMeshPaths)
		{
			// 파일명만 추출해서 표시
			std::filesystem::path fsPath(UTF8ToWide(path));
			CachedSkeletalMeshItems.push_back(WideToUTF8(fsPath.filename().wstring()));
		}
		CachedSkeletalMeshPaths.Insert("", 0);
		CachedSkeletalMeshItems.Insert("None", 0);
	}

	// 2. 머티리얼
	if (CachedMaterialPaths.IsEmpty() && CachedTexturePaths.IsEmpty())
	{
		CachedMaterialPaths = ResMgr.GetAllFilePaths<UMaterial>();
		CachedMaterialItems.Add("None");
		for (const FString& path : CachedMaterialPaths)
		{
			CachedMaterialItems.push_back(path.c_str());
		}
	}

	// 3. 셰이더
	if (CachedShaderPaths.IsEmpty() && CachedShaderItems.IsEmpty())
	{
		CachedShaderPaths = ResMgr.GetAllFilePaths<UShader>();
		CachedShaderItems.Add("None");
		for (const FString& path : CachedShaderPaths)
		{
			CachedShaderItems.push_back(path.c_str());
		}
	}

	// 4. 텍스처
	if (CachedTexturePaths.IsEmpty() && CachedTextureItems.IsEmpty())
	{
		CachedTexturePaths = ResMgr.GetAllFilePaths<UTexture>();
		CachedTextureItems.Add("None");
		for (const FString& path : CachedTexturePaths)
		{
			CachedTextureItems.push_back(path.c_str());
		}
	}

	// --- 5. 스크립트 파일 ---
	if (CachedScriptPaths.IsEmpty() && CachedScriptItems.IsEmpty())
	{
		// 1. "None" 항목 추가 (경로는 "", UI 표시는 "<스크립트 생성>")
		CachedScriptPaths.Add("");
		CachedScriptItems.Add("<스크립트 생성>");

		// 2. 파일 시스템 스캔 (Content/Scripts/ 디렉토리)
		const FString ScriptDir = GDataDir + "/Scripts/";
		if (fs::exists(UTF8ToWide(ScriptDir)) && fs::is_directory(UTF8ToWide(ScriptDir)))
		{
			for (const auto& Entry : fs::recursive_directory_iterator(UTF8ToWide(ScriptDir)))
			{
				if (Entry.is_regular_file() && Entry.path().extension() == ".lua")
				{
					FString Path = WideToUTF8(Entry.path().generic_wstring());
					CachedScriptPaths.Add(NormalizePath(Path));
				}
			}
		}

		// 3. 콤보박스 아이템 채우기 (경로가 모두 추가된 후에!)
		// "None" 항목(인덱스 0)은 이미 추가했으므로 1부터 시작
		for (size_t i = 1; i < CachedScriptPaths.size(); ++i)
		{
			// c_str() 포인터는 CachedScriptPaths가 재할당되지 않는 한 유효합니다.
			// (주의: ClearResourcesCache() 호출 전까지 유효)
			CachedScriptItems.Add(CachedScriptPaths[i].c_str());
		}
	}
    // 5. Sound (.wav)
    if (CachedSoundPaths.IsEmpty() && CachedSoundItems.IsEmpty())
    {
        CachedSoundPaths = ResMgr.GetAllFilePaths<USound>();
        CachedSoundItems.Add("None");
        for (const FString& path : CachedSoundPaths)
        {
            CachedSoundItems.push_back(path.c_str());
        }
    }

	// 6. ParticleSystem (.particle) - 파일 시스템 스캔
	if (CachedParticleSystemPaths.IsEmpty() && CachedParticleSystemItems.IsEmpty())
	{
		// "None" 항목 추가
		CachedParticleSystemPaths.Add("");
		CachedParticleSystemItems.Add("None");

		// Data/Particles/ 디렉토리 스캔
		const FString ParticleDir = GDataDir + "/Particles/";
		if (fs::exists(UTF8ToWide(ParticleDir)) && fs::is_directory(UTF8ToWide(ParticleDir)))
		{
			for (const auto& Entry : fs::recursive_directory_iterator(UTF8ToWide(ParticleDir)))
			{
				if (Entry.is_regular_file())
				{
					FString Ext = WideToUTF8(Entry.path().extension().wstring());
					std::transform(Ext.begin(), Ext.end(), Ext.begin(), ::tolower);
					if (Ext == ".particle")
					{
						FString Path = NormalizePath(WideToUTF8(Entry.path().wstring()));
						CachedParticleSystemPaths.Add(Path);
						CachedParticleSystemItems.Add(WideToUTF8(Entry.path().filename().wstring()));
					}
				}
			}
		}
	}

	// 7. PhysicsAsset (.physicsasset) - 파일 시스템 스캔
	if (CachedPhysicsAssetPaths.IsEmpty() && CachedPhysicsAssetItems.IsEmpty())
	{
		// "None" 항목 추가
		CachedPhysicsAssetPaths.Add("");
		CachedPhysicsAssetItems.Add("None");

		// Data/ 디렉토리 전체 스캔 (PhysicsAsset은 여러 곳에 있을 수 있음)
		const FString DataDir = GDataDir + "/";
		if (fs::exists(UTF8ToWide(DataDir)) && fs::is_directory(UTF8ToWide(DataDir)))
		{
			for (const auto& Entry : fs::recursive_directory_iterator(UTF8ToWide(DataDir)))
			{
				if (Entry.is_regular_file())
				{
					FString Ext = WideToUTF8(Entry.path().extension().wstring());
					std::transform(Ext.begin(), Ext.end(), Ext.begin(), ::tolower);
					if (Ext == ".physicsasset")
					{
						FString Path = NormalizePath(WideToUTF8(Entry.path().wstring()));
						CachedPhysicsAssetPaths.Add(Path);
						CachedPhysicsAssetItems.Add(WideToUTF8(Entry.path().filename().wstring()));
					}
				}
			}
		}
	}
}

void UPropertyRenderer::ClearResourcesCache()
{
	CachedSkeletalMeshPaths.Empty();
	CachedSkeletalMeshItems.Empty();
	CachedStaticMeshPaths.Empty();
	CachedStaticMeshItems.Empty();
	CachedMaterialPaths.Empty();
	CachedMaterialItems.Empty();
	CachedShaderPaths.Empty();
	CachedShaderItems.Empty();
	CachedTexturePaths.Empty();
	CachedTextureItems.Empty();
	CachedSoundPaths.Empty();
	CachedSoundItems.Empty();
	CachedScriptPaths.Empty();
	CachedScriptItems.Empty();
	CachedParticleSystemPaths.Empty();
	CachedParticleSystemItems.Empty();
	CachedPhysicsAssetPaths.Empty();
	CachedPhysicsAssetItems.Empty();
}

// ===== 타입별 렌더링 구현 =====

bool UPropertyRenderer::RenderBoolProperty(const FProperty& Prop, void* Instance)
{
	bool* Value = Prop.GetValuePtr<bool>(Instance);
	
	if (ImGui::Checkbox(Prop.Name, Value))
	{
		if (UObject* Obj = Cast<UObject>((UObject*)Instance))
		{
			Obj->OnPropertyChanged(Prop);
		}
		return true;
	}
	
	return false;
}

bool UPropertyRenderer::RenderInt32Property(const FProperty& Prop, void* Instance)
{
	int32* Value = Prop.GetValuePtr<int32>(Instance);
	
	const float TotalRange = (float)Prop.MaxValue - (float)Prop.MinValue;
	const float ProportionalSpeed = TotalRange * 0.0005f;
	const float DragSpeed = FMath::Max(ProportionalSpeed, 0.01f);

	if (ImGui::DragInt(Prop.Name, Value, DragSpeed, (int)Prop.MinValue, (int)Prop.MaxValue))
	{
		if (UObject* Obj = Cast<UObject>((UObject*)Instance))
		{
			Obj->OnPropertyChanged(Prop);
		}
		return true;
	}

	return false;
}

bool UPropertyRenderer::RenderFloatProperty(const FProperty& Prop, void* Instance)
{
	float* Value = Prop.GetValuePtr<float>(Instance);
	char DisplayFormat[16];
	bool bChanged = false;

	if (Prop.MinValue == 0.0f && Prop.MaxValue == 0.0f)
	{
		snprintf(DisplayFormat, 16, "%%.3f");
		bChanged = ImGui::DragFloat(Prop.Name, Value, 1.0f, 0.0f, 0.0f, DisplayFormat);
	}
	else
	{
		const float TotalRange = std::fabsf((float)Prop.MaxValue - (float)Prop.MinValue);
		const float ProportionalSpeed = TotalRange * 0.0005f;
		const float DragSpeed = FMath::Max(ProportionalSpeed, 0.00001f);

		int DecimalPlaces = 3;
		if (TotalRange > 0.0f)
		{
			DecimalPlaces = static_cast<int>(3.0 - std::floor(std::log10(TotalRange)));
		}
		DecimalPlaces = FMath::Clamp(DecimalPlaces, 0, 5);

		snprintf(DisplayFormat, 16, "%%.%df", DecimalPlaces);
		bChanged = ImGui::DragFloat(Prop.Name, Value, DragSpeed, Prop.MinValue, Prop.MaxValue, DisplayFormat);
	}

	if (bChanged)
	{
		if (UObject* Obj = Cast<UObject>((UObject*)Instance))
		{
			Obj->OnPropertyChanged(Prop);
		}
	}

	return bChanged;
}

bool UPropertyRenderer::RenderVectorProperty(const FProperty& Prop, void* Instance)
{
	// Transform 프로퍼티 (Location, Rotation, Scale) 감지
	bool bIsTransformProperty =
		strcmp(Prop.Name, "RelativeLocation") == 0 ||
		strcmp(Prop.Name, "RelativeRotationEuler") == 0 ||
		strcmp(Prop.Name, "RelativeScale") == 0;

	if (bIsTransformProperty)
	{
		return RenderTransformProperty(Prop, Instance);
	}

	// 일반 Vector 프로퍼티는 기본 렌더링
	FVector* Value = Prop.GetValuePtr<FVector>(Instance);
	return ImGui::DragFloat3(Prop.Name, &Value->X, 0.1f);
}

bool UPropertyRenderer::RenderVector2DProperty(const FProperty& Prop, void* Instance)
{
	// 2D 벡터 프로퍼티 렌더링 (X, Y)
	FVector2D* Value = Prop.GetValuePtr<FVector2D>(Instance);
	return ImGui::DragFloat2(Prop.Name, &Value->X, 0.1f);
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
	FName* ValuePtr = Prop.GetValuePtr<FName>(Instance);
	if (!ValuePtr)
	{
		return false;
	}

	ImGuiID WidgetID = ImGui::GetID(ValuePtr);

	// 2. 위젯 ID별로 편집용 'char' 버퍼를 저장할 static 맵
	//    TArray<char>를 사용하여 버퍼 메모리를 관리합니다.
	static TMap<ImGuiID, TArray<char>> EditBuffers;

	// 3. 이 위젯 ID에 해당하는 버퍼를 가져옵니다.
	TArray<char>& EditBuffer = EditBuffers[WidgetID];

	const SIZE_T BufferSize = 256; // FName에 충분한 버퍼 크기

	// 4. 버퍼가 비어있거나 활성 상태가 아니면 FName 값으로 채웁니다.
	if (EditBuffer.empty() || !ImGui::IsItemActive())
	{
		FString CurrentNameStr = ValuePtr->ToString();
		EditBuffer.assign(BufferSize, '\0'); // 버퍼를 0으로 초기화
		// FString의 내용을 버퍼로 복사 (strncpy_s 또는 유사 함수 사용 권장)
		strncpy_s(EditBuffer.data(), EditBuffer.size(), CurrentNameStr.c_str(), CurrentNameStr.length());
	}

	// 5. InputText 렌더링
	//    이제 EditBuffer.data() (char* 타입)를 전달합니다.
	bool bValueWasChanged = false;
	if (ImGui::InputText(Prop.Name, EditBuffer.data(), BufferSize, ImGuiInputTextFlags_EnterReturnsTrue))
	{
		// 6. 엔터/포커스 해제 시, 버퍼의 텍스트(char*)로 FName을 생성
		FString BufferText(EditBuffer.data());

		if (BufferText != ValuePtr->ToString())
		{
			*ValuePtr = FName(BufferText);
			bValueWasChanged = true;
		}
	}

	return bValueWasChanged;
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
	// TypeName으로 UStruct 찾기
	UStruct* StructType = UStruct::FindStruct(Prop.TypeName);
	if (!StructType)
	{
		ImGui::Text("%s: [Unknown Struct: %s]", Prop.Name, Prop.TypeName ? Prop.TypeName : "null");
		return false;
	}

	bool bChanged = false;
	void* StructInstance = (char*)Instance + Prop.Offset;

	// TreeNode로 접을 수 있게 표시
	if (ImGui::TreeNode(Prop.Name))
	{
		ImGui::Indent();

		// Struct의 모든 프로퍼티 순회
		for (const FProperty& StructProp : StructType->GetAllProperties())
		{
			ImGui::PushID(&StructProp);

			// 재귀적으로 프로퍼티 렌더링
			if (RenderProperty(StructProp, StructInstance))
			{
				bChanged = true;
			}

			ImGui::PopID();
		}

		ImGui::Unindent();
		ImGui::TreePop();
	}

	return bChanged;
}

bool UPropertyRenderer::RenderStructArrayProperty(const FProperty& Prop, void* Instance)
{
	// TypeName으로 UStruct 찾기 (배열의 내부 타입)
	UStruct* StructType = UStruct::FindStruct(Prop.TypeName);
	if (!StructType)
	{
		ImGui::Text("%s: [Unknown Struct Array: %s]", Prop.Name, Prop.TypeName ? Prop.TypeName : "null");
		return false;
	}

	// 배열 조작 함수가 등록되어 있는지 확인
	if (!StructType->ArrayAdd || !StructType->ArrayRemoveAt || !StructType->ArrayNum || !StructType->ArrayGetData)
	{
		ImGui::Text("%s: [Struct '%s' has no array helpers]", Prop.Name, Prop.TypeName);
		return false;
	}

	bool bChanged = false;
	size_t ElementSize = StructType->Size;

	// 배열의 시작 주소 가져오기
	void* ArrayPtr = (char*)Instance + Prop.Offset;

	// 함수 포인터를 통해 배열 정보 얻기
	int32 ArrayNum = StructType->ArrayNum(ArrayPtr);
	void* ArrayData = StructType->ArrayGetData(ArrayPtr);

	// TreeNode로 배열 접기 (Prop.Name을 고정 ID로 사용하여 배열 크기 변경 시에도 상태 유지)
	char headerLabel[128];
	sprintf_s(headerLabel, "%s [%d]", Prop.Name, ArrayNum);
	bool bHeaderOpen = ImGui::TreeNode(Prop.Name, headerLabel);

	// + 버튼을 헤더 옆에 표시
	ImGui::SameLine();
	if (ImGui::SmallButton("+"))
	{
		StructType->ArrayAdd(ArrayPtr);
		bChanged = true;
	}

	// - 버튼 (마지막 요소 삭제)
	ImGui::SameLine();
	if (ImGui::SmallButton("-") && ArrayNum > 0)
	{
		StructType->ArrayRemoveAt(ArrayPtr, ArrayNum - 1);
		bChanged = true;
	}

	if (bHeaderOpen)
	{
		// 배열 정보 갱신 (Add/Remove 후 변경될 수 있음)
		ArrayNum = StructType->ArrayNum(ArrayPtr);
		ArrayData = StructType->ArrayGetData(ArrayPtr);

		int32 MemberCount = static_cast<int32>(StructType->GetAllProperties().size());

		// 삭제 요청 추적
		int32 DeleteRequestIndex = -1;

		// 각 요소 렌더링
		for (int32 i = 0; i < ArrayNum; ++i)
		{
			ImGui::PushID(i);

			// 요소의 메모리 주소
			void* ElementInstance = (char*)ArrayData + (ElementSize * i);

			// TreeNode로 각 요소 표시 (인덱스 기반 - 언리얼 스타일)
			char elementLabel[32];
			sprintf_s(elementLabel, "인덱스 [%d]", i);
			bool bElementOpen = ImGui::TreeNode(elementLabel);

			// 멤버 수 표시
			ImGui::SameLine();
			ImGui::TextDisabled("멤버: %d", MemberCount);

			// ▼ 드롭다운 메뉴 버튼
			ImGui::SameLine();
			if (ImGui::SmallButton("▼"))
			{
				ImGui::OpenPopup("##menu");
			}

			// 드롭다운 메뉴 팝업
			if (ImGui::BeginPopup("##menu"))
			{
				if (ImGui::MenuItem("삽입"))
				{
					// 아래에 삽입 (i+1 위치)
					if (StructType->ArrayInsertAt)
					{
						StructType->ArrayInsertAt(ArrayPtr, i + 1);
						bChanged = true;
					}
				}
				if (ImGui::MenuItem("삭제"))
				{
					DeleteRequestIndex = i;
					bChanged = true;
				}
				if (ImGui::MenuItem("복제"))
				{
					// 복제 (현재 요소를 다음 위치에 복사)
					if (StructType->ArrayDuplicateAt)
					{
						StructType->ArrayDuplicateAt(ArrayPtr, i);
						bChanged = true;
					}
				}
				ImGui::EndPopup();
			}

			if (bElementOpen)
			{
				ImGui::Indent();

				// Struct의 모든 프로퍼티 순회
				for (const FProperty& StructProp : StructType->GetAllProperties())
				{
					ImGui::PushID(&StructProp);

					// 재귀적으로 프로퍼티 렌더링
					if (RenderProperty(StructProp, ElementInstance))
					{
						bChanged = true;
					}

					ImGui::PopID();
				}

				ImGui::Unindent();
				ImGui::TreePop();
			}

			ImGui::PopID();
		}

		// 루프 종료 후 삭제 처리 (ImGui 상태 안전)
		if (DeleteRequestIndex >= 0)
		{
			StructType->ArrayRemoveAt(ArrayPtr, DeleteRequestIndex);
		}

		ImGui::TreePop();
	}

	return bChanged;
}

bool UPropertyRenderer::RenderEnumProperty(const FProperty& Prop, void* Instance)
{
	// TypeName에서 enum 타입 이름 확인
	if (!Prop.TypeName)
	{
		ImGui::Text("%s: [Error: No TypeName]", Prop.Name);
		return false;
	}

	// EnumRegistry에서 메타데이터 조회
	const FEnumInfo* EnumInfo = EnumRegistry::Get().FindEnum(Prop.TypeName);
	if (!EnumInfo)
	{
		ImGui::Text("%s: [Error: Enum '%s' not in registry]", Prop.Name, Prop.TypeName);
		return false;
	}

	// Enum 값의 포인터 가져오기 (uint8로 가정 - 대부분의 UENUM이 uint8)
	uint8* EnumPtr = Prop.GetValuePtr<uint8>(Instance);
	if (!EnumPtr)
	{
		ImGui::Text("%s: [Invalid Enum]", Prop.Name);
		return false;
	}

	bool bChanged = false;
	int CurrentValue = static_cast<int>(*EnumPtr);

	// Entries에서 이름 배열과 현재 인덱스 구성
	TArray<const char*> EnumNames;
	int CurrentIndex = 0;

	for (size_t i = 0; i < EnumInfo->Entries.size(); ++i)
	{
		const FEnumEntry& Entry = EnumInfo->Entries[i];
		EnumNames.Add(Entry.DisplayName ? Entry.DisplayName : Entry.Name);

		if (static_cast<int>(Entry.Value) == CurrentValue)
		{
			CurrentIndex = static_cast<int>(i);
		}
	}

	// 콤보박스 렌더링
	ImGui::SetNextItemWidth(200);
	if (ImGui::Combo(Prop.Name, &CurrentIndex, EnumNames.data(), static_cast<int>(EnumNames.size())))
	{
		if (CurrentIndex >= 0 && CurrentIndex < static_cast<int>(EnumInfo->Entries.size()))
		{
			*EnumPtr = static_cast<uint8>(EnumInfo->Entries[CurrentIndex].Value);
			bChanged = true;
		}
	}

	return bChanged;
}

bool UPropertyRenderer::RenderMapProperty(const FProperty& Prop, void* Instance)
{
	if (Prop.KeyType == EPropertyType::Unknown || Prop.InnerType == EPropertyType::Unknown)
	{
		ImGui::Text("%s: [Invalid Map]", Prop.Name);
		return false;
	}

	bool bChanged = false;

	// TreeNode로 접을 수 있게 표시
	if (ImGui::TreeNode(Prop.Name))
	{
		ImGui::Indent();

		// FString 키 Map 처리 (가장 일반적인 경우)
		if (Prop.KeyType == EPropertyType::FString)
		{
			switch (Prop.InnerType)
			{
			case EPropertyType::Int32:
			{
				TMap<FString, int32>* MapPtr = Prop.GetValuePtr<TMap<FString, int32>>(Instance);
				if (MapPtr)
				{
					// Add 버튼
					static char newKeyBuf[128] = "";
					ImGui::InputText("New Key", newKeyBuf, sizeof(newKeyBuf));
					ImGui::SameLine();
					if (ImGui::Button("Add##Int32"))
					{
						FString newKey = newKeyBuf;
						if (!newKey.empty() && MapPtr->find(newKey) == MapPtr->end())
						{
							(*MapPtr)[newKey] = 0;
							newKeyBuf[0] = '\0';
							bChanged = true;
						}
					}

					// 삭제할 키 저장용
					FString keyToDelete;
					FString keyToRename;
					FString newKeyName;

					int idx = 0;
					for (auto& [key, value] : *MapPtr)
					{
						ImGui::PushID(idx++);

						// Key 편집
						char keyBuf[128];
						strncpy_s(keyBuf, key.c_str(), sizeof(keyBuf) - 1);
						ImGui::SetNextItemWidth(100);
						if (ImGui::InputText("##key", keyBuf, sizeof(keyBuf), ImGuiInputTextFlags_EnterReturnsTrue))
						{
							FString newName = keyBuf;
							if (!newName.empty() && newName != key && MapPtr->find(newName) == MapPtr->end())
							{
								keyToRename = key;
								newKeyName = newName;
							}
						}

						ImGui::SameLine();
						ImGui::SetNextItemWidth(100);
						if (ImGui::DragInt("##val", &value, 1))
							bChanged = true;

						ImGui::SameLine();
						if (ImGui::Button("X"))
							keyToDelete = key;

						ImGui::PopID();
					}

					// 키 이름 변경 처리
					if (!keyToRename.empty())
					{
						int32 val = (*MapPtr)[keyToRename];
						MapPtr->erase(keyToRename);
						(*MapPtr)[newKeyName] = val;
						bChanged = true;
					}

					// 삭제 처리
					if (!keyToDelete.empty())
					{
						MapPtr->erase(keyToDelete);
						bChanged = true;
					}
				}
				break;
			}
			case EPropertyType::Float:
			{
				TMap<FString, float>* MapPtr = Prop.GetValuePtr<TMap<FString, float>>(Instance);
				if (MapPtr)
				{
					// Add 버튼
					static char newKeyBuf[128] = "";
					ImGui::InputText("New Key", newKeyBuf, sizeof(newKeyBuf));
					ImGui::SameLine();
					if (ImGui::Button("Add##Float"))
					{
						FString newKey = newKeyBuf;
						if (!newKey.empty() && MapPtr->find(newKey) == MapPtr->end())
						{
							(*MapPtr)[newKey] = 0.0f;
							newKeyBuf[0] = '\0';
							bChanged = true;
						}
					}

					FString keyToDelete;
					FString keyToRename;
					FString newKeyName;

					int idx = 0;
					for (auto& [key, value] : *MapPtr)
					{
						ImGui::PushID(idx++);

						char keyBuf[128];
						strncpy_s(keyBuf, key.c_str(), sizeof(keyBuf) - 1);
						ImGui::SetNextItemWidth(100);
						if (ImGui::InputText("##key", keyBuf, sizeof(keyBuf), ImGuiInputTextFlags_EnterReturnsTrue))
						{
							FString newName = keyBuf;
							if (!newName.empty() && newName != key && MapPtr->find(newName) == MapPtr->end())
							{
								keyToRename = key;
								newKeyName = newName;
							}
						}

						ImGui::SameLine();
						ImGui::SetNextItemWidth(100);
						if (ImGui::DragFloat("##val", &value, 0.1f))
							bChanged = true;

						ImGui::SameLine();
						if (ImGui::Button("X"))
							keyToDelete = key;

						ImGui::PopID();
					}

					if (!keyToRename.empty())
					{
						float val = (*MapPtr)[keyToRename];
						MapPtr->erase(keyToRename);
						(*MapPtr)[newKeyName] = val;
						bChanged = true;
					}

					if (!keyToDelete.empty())
					{
						MapPtr->erase(keyToDelete);
						bChanged = true;
					}
				}
				break;
			}
			case EPropertyType::FString:
			{
				TMap<FString, FString>* MapPtr = Prop.GetValuePtr<TMap<FString, FString>>(Instance);
				if (MapPtr)
				{
					// Add 버튼
					static char newKeyBuf[128] = "";
					ImGui::InputText("New Key", newKeyBuf, sizeof(newKeyBuf));
					ImGui::SameLine();
					if (ImGui::Button("Add##String"))
					{
						FString newKey = newKeyBuf;
						if (!newKey.empty() && MapPtr->find(newKey) == MapPtr->end())
						{
							(*MapPtr)[newKey] = "";
							newKeyBuf[0] = '\0';
							bChanged = true;
						}
					}

					FString keyToDelete;
					FString keyToRename;
					FString newKeyName;

					int idx = 0;
					for (auto& [key, value] : *MapPtr)
					{
						ImGui::PushID(idx++);

						char keyBuf[128];
						strncpy_s(keyBuf, key.c_str(), sizeof(keyBuf) - 1);
						ImGui::SetNextItemWidth(100);
						if (ImGui::InputText("##key", keyBuf, sizeof(keyBuf), ImGuiInputTextFlags_EnterReturnsTrue))
						{
							FString newName = keyBuf;
							if (!newName.empty() && newName != key && MapPtr->find(newName) == MapPtr->end())
							{
								keyToRename = key;
								newKeyName = newName;
							}
						}

						ImGui::SameLine();
						char valBuf[256];
						strncpy_s(valBuf, value.c_str(), sizeof(valBuf) - 1);
						ImGui::SetNextItemWidth(150);
						if (ImGui::InputText("##val", valBuf, sizeof(valBuf)))
						{
							value = valBuf;
							bChanged = true;
						}

						ImGui::SameLine();
						if (ImGui::Button("X"))
							keyToDelete = key;

						ImGui::PopID();
					}

					if (!keyToRename.empty())
					{
						FString val = (*MapPtr)[keyToRename];
						MapPtr->erase(keyToRename);
						(*MapPtr)[newKeyName] = val;
						bChanged = true;
					}

					if (!keyToDelete.empty())
					{
						MapPtr->erase(keyToDelete);
						bChanged = true;
					}
				}
				break;
			}
			case EPropertyType::Bool:
			{
				TMap<FString, bool>* MapPtr = Prop.GetValuePtr<TMap<FString, bool>>(Instance);
				if (MapPtr)
				{
					// Add 버튼
					static char newKeyBuf[128] = "";
					ImGui::InputText("New Key", newKeyBuf, sizeof(newKeyBuf));
					ImGui::SameLine();
					if (ImGui::Button("Add##Bool"))
					{
						FString newKey = newKeyBuf;
						if (!newKey.empty() && MapPtr->find(newKey) == MapPtr->end())
						{
							(*MapPtr)[newKey] = false;
							newKeyBuf[0] = '\0';
							bChanged = true;
						}
					}

					FString keyToDelete;
					FString keyToRename;
					FString newKeyName;

					int idx = 0;
					for (auto& [key, value] : *MapPtr)
					{
						ImGui::PushID(idx++);

						char keyBuf[128];
						strncpy_s(keyBuf, key.c_str(), sizeof(keyBuf) - 1);
						ImGui::SetNextItemWidth(100);
						if (ImGui::InputText("##key", keyBuf, sizeof(keyBuf), ImGuiInputTextFlags_EnterReturnsTrue))
						{
							FString newName = keyBuf;
							if (!newName.empty() && newName != key && MapPtr->find(newName) == MapPtr->end())
							{
								keyToRename = key;
								newKeyName = newName;
							}
						}

						ImGui::SameLine();
						if (ImGui::Checkbox("##val", &value))
							bChanged = true;

						ImGui::SameLine();
						if (ImGui::Button("X"))
							keyToDelete = key;

						ImGui::PopID();
					}

					if (!keyToRename.empty())
					{
						bool val = (*MapPtr)[keyToRename];
						MapPtr->erase(keyToRename);
						(*MapPtr)[newKeyName] = val;
						bChanged = true;
					}

					if (!keyToDelete.empty())
					{
						MapPtr->erase(keyToDelete);
						bChanged = true;
					}
				}
				break;
			}
			default:
				ImGui::Text("Unsupported value type");
				break;
			}
		}
		else if (Prop.KeyType == EPropertyType::Int32)
		{
			// int32 키 Map은 읽기 전용으로 표시
			ImGui::Text("(int32 key maps: read-only)");
		}
		else
		{
			ImGui::Text("Unsupported key type");
		}

		ImGui::Unindent();
		ImGui::TreePop();
	}

	return bChanged;
}

bool UPropertyRenderer::RenderTextureProperty(const FProperty& Prop, void* Instance)
{
	UTexture** TexturePtr = Prop.GetValuePtr<UTexture*>(Instance);
	UTexture* CurrentTexture = *TexturePtr;
	UTexture* NewTexture = nullptr;

	// 헬퍼 함수를 호출하여 텍스처 콤보박스를 렌더링합니다.
	bool bChanged = RenderTextureSelectionCombo(Prop.Name, CurrentTexture, NewTexture);

	if (bChanged)
	{
		// 프로퍼티 포인터 업데이트
		*TexturePtr = NewTexture;

		// 새 텍스처 경로 (None일 경우 빈 문자열)
		FString NewPath = (NewTexture) ? NewTexture->GetFilePath() : "";

		// 컴포넌트별 Setter 호출
		UObject* Obj = static_cast<UObject*>(Instance);
		if (UBillboardComponent* Billboard = Cast<UBillboardComponent>(Obj))
		{
			Billboard->SetTexture(NewPath);
		}
		else if (UDecalComponent* Decal = Cast<UDecalComponent>(Obj))
		{
			Decal->SetDecalTexture(NewPath);
		}

		return true;
	}

	return false;
}


bool UPropertyRenderer::RenderSoundProperty(const FProperty& Prop, void* Instance)
{
	USound** SoundPtr = Prop.GetValuePtr<USound*>(Instance);
	USound* CurrentSound = *SoundPtr;
	USound* NewSound = nullptr;

	// 헬퍼 함수를 호출하여 텍스처 콤보박스를 렌더링합니다.
	bool bChanged = RenderSoundSelectionComboSimple(Prop.Name, CurrentSound, NewSound);

	if (bChanged)
	{
		// 프로퍼티 포인터 업데이트
		*SoundPtr = NewSound;

		// 새 텍스처 경로 (None일 경우 빈 문자열)
		FString NewPath = (NewSound) ? NewSound->GetFilePath() : "";

		// 컴포넌트별 Setter 호출
		UObject* Obj = static_cast<UObject*>(Instance);
		if (UBillboardComponent* Billboard = Cast<UBillboardComponent>(Obj))
		{
			Billboard->SetTexture(NewPath);
		}
		else if (UDecalComponent* Decal = Cast<UDecalComponent>(Obj))
		{
			Decal->SetDecalTexture(NewPath);
		}

		return true;
	}

	return false;
}


bool UPropertyRenderer::RenderSRVProperty(const FProperty& Prop, void* Instance)
{
	ULightComponent* LightComp = static_cast<ULightComponent*>(Instance);
	if (!LightComp) return false;

	FLightManager* LightManager = GWorld->GetLightManager();
	if (!LightManager) return false;

	//@TODO PropertyRenderer 와 Component 간 결합 줄이기
	// Point Light와 Spot Light 구분
	USpotLightComponent* SpotLight = Cast<USpotLightComponent>(LightComp);
	UPointLightComponent* PointLight = Cast<UPointLightComponent>(LightComp);

	// Point Light (Spot Light 제외)
	if (PointLight && !SpotLight)
	{
		int32 CubeSliceIndex = -1;
		if (!LightManager->GetCachedShadowCubeSliceIndex(LightComp, CubeSliceIndex) || CubeSliceIndex < 0)
		{
			ImGui::Text("Cube Shadow Map not available");
			return false;
		}
		return RenderPointLightCubeShadowMap(LightManager, LightComp, CubeSliceIndex);
	}

	// Spot Light 및 기타 2D Shadow Map
	ID3D11ShaderResourceView* AtlasSRV = LightManager->GetShadowAtlasSRV2D();
	if (!AtlasSRV)
	{
		ImGui::Text("Shadow Atlas SRV is null");
		return false;
	}
	return RenderSpotLightShadowMap(LightManager, LightComp, AtlasSRV);
}

bool UPropertyRenderer::RenderScriptFileProperty(const FProperty& Property, void* ObjectInstance)
{
	// 콤보박스 전용 텍스트 필터를 static으로 선언합니다.
	static ImGuiTextFilter ScriptComboFilter;
	static char NewScriptNameBuffer[128] = "NewScript";

	bool bChanged = false;
	FString* FilePath = Property.GetValuePtr<FString>(ObjectInstance);
	if (!FilePath) return false;

	// 1. 현재 선택된 아이템 찾기
	int CurrentItem = 0; // 0번 인덱스("<스크립트 생성>")가 기본값
	for (int i = 0; i < CachedScriptPaths.Num(); ++i)
	{
		// 경로가 비어있지 않고, 캐시된 경로와 일치하는 경우
		if (!FilePath->empty() && CachedScriptPaths[i] == *FilePath)
		{
			CurrentItem = i;
			break;
		}
	}

	// 2. 콤보 박스 렌더링
	ImGui::Text("%s", Property.Name);
	ImGui::SameLine();
	ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
	ImGui::PushID(Property.Name); // 고유 ID

	// 콤보박스에 표시될 텍스트
	const char* PreviewText = CachedScriptItems[CurrentItem];
	FString FormattedPreviewText; // (missing) 텍스트를 담을 임시 FString
	bool bFileIsMissing = false;

	// 실제 파일이 존재하는지 확인
	if (!FilePath->empty())
	{
		if (!fs::exists(UTF8ToWide(*FilePath)))
		{
			bFileIsMissing = true;
			// (missing) 태그가 붙은 임시 텍스트 생성
			FormattedPreviewText = "(missing) " + *FilePath;
			// 콤보박스 프리뷰가 임시 텍스트를 가리키도록 함
			PreviewText = FormattedPreviewText.c_str();
		}
	}

	if (ImGui::BeginCombo("##ScriptCombo", PreviewText))
	{
		// 콤보박스 팝업이 '처음 열린 프레임'에만
		if (ImGui::IsWindowAppearing())
		{
			// 스크립트 목록 다시 로드하기 위해서
			CachedScriptPaths.Empty();
			CachedScriptItems.Empty();

			// "다음에 그려질 위젯" (Search)에 포커스를 한 번만 줍니다.
			ImGui::SetKeyboardFocusHere(0);
		}

		// 검색창을 렌더링하고, 포커스 여부를 '먼저' 변수에 저장
		ScriptComboFilter.Draw("Search");
		bool bFilterIsFocused = ImGui::IsItemFocused();

		for (int i = 0; i < CachedScriptItems.Num(); ++i)
		{
			if (i == 0 || ScriptComboFilter.PassFilter(CachedScriptItems[i]))
			{
				const bool bIsSelected = (CurrentItem == i);
				if (ImGui::Selectable(CachedScriptItems[i], bIsSelected))
				{
					CurrentItem = i;
					*FilePath = CachedScriptPaths[i];
					bChanged = true;

					// 0번("<스크립트 생성>")이 선택되었는지 확인
					if (i == 0)
					{
						// 검색창(Filter)의 텍스트가 비어있지 않다면
						if (ScriptComboFilter.InputBuf[0] != '\0')
						{
							// 검색창의 텍스트를 'NewScriptNameBuffer'로 복사
							strncpy_s(NewScriptNameBuffer,
								IM_ARRAYSIZE(NewScriptNameBuffer),
								ScriptComboFilter.InputBuf,
								_TRUNCATE);
						}
					}

					// 항목이 선택되었으므로 검색창은 비웁니다.
					ScriptComboFilter.Clear();
				}
				if (bIsSelected)
				{
					ImGui::SetItemDefaultFocus();
				}
			}
		}
		ImGui::EndCombo();
	}
	ImGui::PopID();

	// 사라진 파일 표시
	if (bFileIsMissing)
	{
		// 파일이 존재하지 않음 (빨간색 텍스트 + 선택 해제 버튼)
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
		ImGui::TextWrapped("파일이 존재하지 않습니다. 경로를 다시 설정하십시오.");
		ImGui::PopStyleColor();

		ImGui::SameLine();
		if (ImGui::SmallButton("선택 해제"))
		{
			*FilePath = ""; // 경로를 비움
			bChanged = true;
			// CurrentItem은 다음 프레임에 0으로 다시 계산될 것임
		}
	}
	// 3. 스크립트 생성 UI (선택된 파일이 없을 때만 표시)
	else if (FilePath->empty() || CurrentItem == 0)
	{
		ImGui::Separator();

		// "스크립트 생성 메뉴"
		ImGui::InputText("스크립트 명", NewScriptNameBuffer, IM_ARRAYSIZE(NewScriptNameBuffer));
		ImGui::SameLine();

		if (ImGui::Button("스크립트 생성"))
		{
			// 1. 경로 및 확장자 설정
			const FString* ExtPtr = Property.Metadata.Find(FName("FileExtension"));
			FString Extension = (ExtPtr) ? *ExtPtr : ".lua";
			if (Extension[0] != '.') Extension = "." + Extension;

			// (참고: 스크립트 기본 경로는 "Content/Scripts/"로 가정)
			FString BaseDir = GDataDir + "/Scripts/";
			FString NewFileName(NewScriptNameBuffer);
			FString RelativePath = BaseDir + NewFileName + Extension;

			// 템플릿 파일 경로 정의
			const FString TemplatePath = GDataDir + "/Templates/template.lua";

			// 2. 디렉토리 생성 (없을 경우)
			fs::create_directories(UTF8ToWide(BaseDir));

			// 3. 템플릿 파일 존재 여부 확인
			if (!fs::exists(UTF8ToWide(TemplatePath)))
			{
				UE_LOG("[error] 템플릿 파일(template.lua)을 찾을 수 없습니다.");
			}
			// 4. 생성할 파일이 이미 존재하는지 중복 체크
			else if (fs::exists(UTF8ToWide(RelativePath)))
			{
				UE_LOG(("[error] '" + NewFileName + Extension + "' 파일이 이미 존재합니다.").c_str());
			}
			else
			{
				try
				{
					// 템플릿 파일을 새 경로로 복사
					fs::copy(UTF8ToWide(TemplatePath), UTF8ToWide(RelativePath), fs::copy_options::overwrite_existing);

					// 4. 새 파일을 캐시에 즉시 추가
					CachedScriptPaths.Add(RelativePath);

					CachedScriptItems.Add(CachedScriptPaths.back().c_str());

					// 5. 현재 프로퍼티에 새 경로 설정
					*FilePath = RelativePath;
					bChanged = true;
				}
				catch (const fs::filesystem_error& E)
				{
					UE_LOG(("[error] 파일 복사에 실패했습니다. " + FString(E.what())).c_str());
				}
			}

			if (bChanged)
			{
				strcpy_s(NewScriptNameBuffer, IM_ARRAYSIZE(NewScriptNameBuffer), "NewScript");
			}
		}

		ImGui::Separator();
	}
	// 스크립트 파일이 있을 때
	else
	{
		if (ImGui::Button("편집하기"))
		{
			FPlatformProcess::OpenFileInDefaultEditor(UTF8ToWide(*FilePath));
		}
	}

	return bChanged;
}

bool UPropertyRenderer::RenderCurveProperty(const FProperty& Prop, void* Instance)
{
	// 1. 프로퍼티에서 실제 데이터(float[4])에 대한 포인터를 가져옵니다.
    //    GetValuePtr<float>는 배열의 첫 번째 요소(float*)를 반환합니다.
	float* PropertyData = Prop.GetValuePtr<float>(Instance);
	if (!PropertyData)
	{
		ImGui::Text("%s: [Invalid Data]", Prop.Name);
		return false;
	}

	// 2. ImGui::Bezier 위젯은 float[5] 배열을 필요로 합니다 (P[4] = 프리셋 인덱스).
	//    이 위젯을 위한 임시 배열을 스택에 생성합니다.
	float EditorData[5];

	//    실제 데이터(float[4])를 임시 배열의 앞부분(EditorData[0]~[3])으로 복사합니다.
	memcpy(EditorData, PropertyData, sizeof(float) * 4);

	// 3. 프리셋 인덱스(EditorData[4])는 UI 상태이므로, 객체 데이터에 저장되지 않습니다.
	//    ImGui의 내부 상태 저장소를 사용하여 이 UI 상태를 프레임 간에 유지합니다.

	//    이 프로퍼티 위젯 전체에 대한 고유 ID를 설정합니다.
	ImGui::PushID(Prop.Name);
	ImGui::PushID(Instance); // 동일한 객체라도 다른 인스턴스일 수 있으므로 포인터로 ID 추가

	ImGuiStorage* storage = ImGui::GetStateStorage();
	// 이 프로퍼티의 "PresetIndex" 상태를 위한 고유 ID 생성
	ImGuiID presetId = ImGui::GetID("PresetIndex");

	// 4. 저장된 프리셋 인덱스 로드, 없으면 0.0f (일반적으로 "Linear" 또는 "Custom")
	EditorData[4] = storage->GetFloat(presetId, 0.0f);

	// 5. Bezier 위젯을 호출합니다. 
	//    Prop.Name을 레이블로 사용합니다. (PushID로 고유성 보장됨)
	bool bCurveChanged = ImGui::Bezier(Prop.Name, EditorData);

	// 6. 값이 변경되었는지 확인합니다.
	if (bCurveChanged)
	{
		// 7. 변경된 곡선 값(EditorData[0]~[3])을 
		//    실제 프로퍼티(PropertyData)에 다시 복사합니다.
		memcpy(PropertyData, EditorData, sizeof(float) * 4);

		// 8. 변경된 프리셋 인덱스(EditorData[4])를 ImGui 저장소에 다시 저장합니다.
		storage->SetFloat(presetId, EditorData[4]);
	}

	ImGui::PopID(); // Instance ID
	ImGui::PopID(); // Prop.Name ID

	// 툴팁 렌더링은 부모 함수인 RenderProperty에서 이미 처리하고 있습니다.

	return bCurveChanged;
}

bool UPropertyRenderer::RenderPointLightCubeShadowMap(FLightManager* LightManager, ULightComponent* LightComp, int32 CubeSliceIndex)
{
	ImGui::Text("Point Light Cube Shadow Map (Slice %d)", CubeSliceIndex);
	ImGui::Spacing();

	const char* FaceNames[6] = { "+X", "-X", "+Y", "-Y", "+Z", "-Z" };
	const int32 FaceSize = 128;

	// 2x3 그리드로 Cube Map의 6개 면 표시
	for (int row = 0; row < 2; ++row)
	{
		for (int col = 0; col < 3; ++col)
		{
			int32 faceIdx = row * 3 + col;
			ID3D11ShaderResourceView* FaceSRV = LightManager->GetShadowCubeFaceSRV(CubeSliceIndex, faceIdx);

			ImGui::BeginGroup();
			ImGui::Text("%s", FaceNames[faceIdx]);
			if (FaceSRV)
			{
				ImGui::Image((ImTextureID)FaceSRV, ImVec2(FaceSize, FaceSize));
			}
			else
			{
				ImGui::Text("N/A");
			}
			ImGui::EndGroup();

			if (col < 2) ImGui::SameLine();
		}
	}

	return false;
}

bool UPropertyRenderer::RenderSpotLightShadowMap(FLightManager* LightManager, ULightComponent* LightComp, ID3D11ShaderResourceView* AtlasSRV)
{
	FShadowMapData ShadowData;
	if (!LightManager->GetCachedShadowData(LightComp, 0, ShadowData))
	{
		ImGui::Text("Shadow data not available");
		return false;
	}

	// Atlas UV 계산
	ImVec2 uv0(ShadowData.AtlasScaleOffset.Z, ShadowData.AtlasScaleOffset.W);
	ImVec2 uv1(ShadowData.AtlasScaleOffset.Z + ShadowData.AtlasScaleOffset.X,
		ShadowData.AtlasScaleOffset.W + ShadowData.AtlasScaleOffset.Y);

	// Shadow Map 표시
	ImGui::Text("Shadow Map:");
	ImGui::Image((ImTextureID)AtlasSRV, ImVec2(256, 256), uv0, uv1);

	// 전체 Atlas 표시 (디버깅용)
	ImGui::Text("Full Atlas:");
	ImGui::Image((ImTextureID)AtlasSRV, ImVec2(256, 256), ImVec2(0, 0), ImVec2(1, 1));

	return false;
}

bool UPropertyRenderer::RenderSkeletalMeshProperty(const FProperty& Prop, void* Instance)
{
	USkeletalMesh** MeshPtr = Prop.GetValuePtr<USkeletalMesh*>(Instance);

	FString CurrentPath;
	if (*MeshPtr)
	{
		CurrentPath = (*MeshPtr)->GetFilePath();
	}

	if (CachedSkeletalMeshPaths.empty())
	{
		ImGui::Text("%s: <No Meshes>", Prop.Name);
		return false;
	}

	int SelectedIdx = -1;
	for (int i = 0; i < static_cast<int>(CachedSkeletalMeshPaths.size()); ++i)
	{
		if (CachedSkeletalMeshPaths[i] == CurrentPath)
		{
			SelectedIdx = i;
			break;
		}
	}

	if (ImGui::Button("Skeletal Viewer"))
	{
		// Create a context to store asset info
		UEditorAssetPreviewContext* Context = NewObject<UEditorAssetPreviewContext>();
		Context->ViewerType = EViewerType::Skeletal;
		Context->AssetPath = CurrentPath;

		// Request USlateManager to open the viewer
		USlateManager::GetInstance().OpenAssetViewer(Context);
	}
	ImGui::SameLine();
	if (ImGui::Button("Anim Viewer"))
	{
		// Create a context to store asset info
		UEditorAssetPreviewContext* Context = NewObject<UEditorAssetPreviewContext>();
		Context->ViewerType = EViewerType::Animation;
		Context->AssetPath = CurrentPath;

		// Request USlateManager to open the viewer
		USlateManager::GetInstance().OpenAssetViewer(Context);
	}
	ImGui::SameLine();
	if (ImGui::Button("Blend Space Viewer"))
	{
		// Create a context to store asset info
		UEditorAssetPreviewContext* Context = NewObject<UEditorAssetPreviewContext>();
		Context->ViewerType = EViewerType::BlendSpace;
		Context->AssetPath = CurrentPath;

		// Request USlateManager to open the viewer
		USlateManager::GetInstance().OpenAssetViewer(Context);
	}

	ImGui::SetNextItemWidth(240);
	if (ImGui::Combo(Prop.Name, &SelectedIdx, &ItemsGetter, (void*)&CachedSkeletalMeshItems, static_cast<int>(CachedSkeletalMeshItems.size())))
	{
		if (SelectedIdx >= 0 && SelectedIdx < static_cast<int>(CachedSkeletalMeshPaths.size()))
		{
			// 컴포넌트별 Setter 호출
			UObject* Object = static_cast<UObject*>(Instance);
			if (USkeletalMeshComponent* StaticMeshComponent = Cast<USkeletalMeshComponent>(Object))
			{
				StaticMeshComponent->SetSkeletalMesh(CachedSkeletalMeshPaths[SelectedIdx]);
			}
			else
			{
				*MeshPtr = UResourceManager::GetInstance().Load<USkeletalMesh>(CachedSkeletalMeshPaths[SelectedIdx]);
			}
			return true;
		}
	}
	if (ImGui::IsItemHovered())
	{
		ImGui::BeginTooltip();

		// 1. 원본 경로 표시 (CurrentTexturePath는 null일 때 "None"을 가짐)
		ImGui::TextUnformatted(CurrentPath.c_str());

		//// 2. CurrentTexture가 유효하고 캐시 파일 경로가 있다면 추가로 표시
		//if ((*MeshPtr))
		//{
		//	const FString& CachedPath = (*MeshPtr)->GetCacheFilePath();
		//	if (!CachedPath.empty())
		//	{
		//		ImGui::Separator(); // 원본 경로와 구분하기 위해 선 추가
		//		ImGui::Text("Cache: %s", CachedPath.c_str());
		//	}
		//}

		ImGui::EndTooltip();
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

	if (CachedStaticMeshPaths.empty())
	{
		ImGui::Text("%s: <No Meshes>", Prop.Name);
		return false;
	}

	int SelectedIdx = -1;
	for (int i = 0; i < static_cast<int>(CachedStaticMeshPaths.size()); ++i)
	{
		if (CachedStaticMeshPaths[i] == CurrentPath)
		{
			SelectedIdx = i;
			break;
		}
	}

	// TArray<FString>을 const char* 배열로 변환
	TArray<const char*> ItemsPtr;
	ItemsPtr.reserve(CachedStaticMeshItems.size());
	for (const FString& item : CachedStaticMeshItems)
	{
		ItemsPtr.push_back(item.c_str());
	}

	ImGui::SetNextItemWidth(240);
	if (ImGui::Combo(Prop.Name, &SelectedIdx, ItemsPtr.data(), static_cast<int>(ItemsPtr.size())))
	{
		if (SelectedIdx >= 0 && SelectedIdx < static_cast<int>(CachedStaticMeshPaths.size()))
		{
			// 컴포넌트별 Setter 호출
			UObject* Object = static_cast<UObject*>(Instance);
			if (UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>(Object))
			{
				StaticMeshComponent->SetStaticMesh(CachedStaticMeshPaths[SelectedIdx]);
			}
			else
			{
				*MeshPtr = UResourceManager::GetInstance().Load<UStaticMesh>(CachedStaticMeshPaths[SelectedIdx]);
			}
			return true;
		}
	}

	// 닫힌 콤보박스 '텍스트' 부분에 마우스를 올렸을 때 전체 경로 툴팁
	if (ImGui::IsItemHovered())
	{
		ImGui::BeginTooltip();

		// 1. 원본 경로 표시 (CurrentTexturePath는 null일 때 "None"을 가짐)
		ImGui::TextUnformatted(CurrentPath.c_str());

		// 2. CurrentTexture가 유효하고 캐시 파일 경로가 있다면 추가로 표시
		if ((*MeshPtr))
		{
			const FString& CachedPath = (*MeshPtr)->GetCacheFilePath();
			if (!CachedPath.empty())
			{
				ImGui::Separator(); // 원본 경로와 구분하기 위해 선 추가
				ImGui::Text("Cache: %s", CachedPath.c_str());
			}
		}

		ImGui::EndTooltip();
	}

	return false;
}

bool UPropertyRenderer::RenderParticleSystemProperty(const FProperty& Prop, void* Instance)
{
	UParticleSystem** ParticlePtr = Prop.GetValuePtr<UParticleSystem*>(Instance);

	FString CurrentPath;
	if (*ParticlePtr)
	{
		CurrentPath = (*ParticlePtr)->GetFilePath();
	}

	if (CachedParticleSystemPaths.empty())
	{
		ImGui::Text("%s: <No Particle Systems>", Prop.Name);
		return false;
	}

	int SelectedIdx = 0; // Default to "None"
	for (int i = 0; i < static_cast<int>(CachedParticleSystemPaths.size()); ++i)
	{
		if (CachedParticleSystemPaths[i] == CurrentPath)
		{
			SelectedIdx = i;
			break;
		}
	}

	// TArray<FString>을 const char* 배열로 변환
	TArray<const char*> ItemsPtr;
	ItemsPtr.reserve(CachedParticleSystemItems.size());
	for (const FString& item : CachedParticleSystemItems)
	{
		ItemsPtr.push_back(item.c_str());
	}

	ImGui::SetNextItemWidth(240);
	if (ImGui::Combo(Prop.Name, &SelectedIdx, ItemsPtr.data(), static_cast<int>(ItemsPtr.size())))
	{
		if (SelectedIdx >= 0 && SelectedIdx < static_cast<int>(CachedParticleSystemPaths.size()))
		{
			// ParticleSystemComponent인 경우 SetTemplate 호출
			UObject* Object = static_cast<UObject*>(Instance);
			if (UParticleSystemComponent* PSC = Cast<UParticleSystemComponent>(Object))
			{
				if (CachedParticleSystemPaths[SelectedIdx].empty())
				{
					PSC->SetTemplate(nullptr);
				}
				else
				{
					UParticleSystem* NewTemplate = UResourceManager::GetInstance().Load<UParticleSystem>(CachedParticleSystemPaths[SelectedIdx]);
					PSC->SetTemplate(NewTemplate);
				}
			}
			else
			{
				// 일반적인 경우 직접 할당
				if (CachedParticleSystemPaths[SelectedIdx].empty())
				{
					*ParticlePtr = nullptr;
				}
				else
				{
					*ParticlePtr = UResourceManager::GetInstance().Load<UParticleSystem>(CachedParticleSystemPaths[SelectedIdx]);
				}
			}
			return true;
		}
	}

	// 툴팁: 전체 경로 표시
	if (ImGui::IsItemHovered())
	{
		ImGui::BeginTooltip();
		if (CurrentPath.empty())
		{
			ImGui::Text("None");
		}
		else
		{
			ImGui::TextUnformatted(CurrentPath.c_str());
		}
		ImGui::EndTooltip();
	}

	return false;
}

bool UPropertyRenderer::RenderPhysicsAssetProperty(const FProperty& Prop, void* Instance)
{
	CacheResources();

	UPhysicsAsset** PhysicsAssetPtr = Prop.GetValuePtr<UPhysicsAsset*>(Instance);

	FString CurrentPath;
	if (*PhysicsAssetPtr)
	{
		CurrentPath = (*PhysicsAssetPtr)->GetFilePath();
	}

	if (CachedPhysicsAssetPaths.empty())
	{
		ImGui::Text("%s: <No Physics Assets>", Prop.Name);
		return false;
	}

	int SelectedIdx = 0; // Default to "None"
	for (int i = 0; i < static_cast<int>(CachedPhysicsAssetPaths.size()); ++i)
	{
		if (CachedPhysicsAssetPaths[i] == CurrentPath)
		{
			SelectedIdx = i;
			break;
		}
	}

	// TArray<FString>을 const char* 배열로 변환
	TArray<const char*> ItemsPtr;
	ItemsPtr.reserve(CachedPhysicsAssetItems.size());
	for (const FString& item : CachedPhysicsAssetItems)
	{
		ItemsPtr.push_back(item.c_str());
	}

	ImGui::SetNextItemWidth(240);
	if (ImGui::Combo(Prop.Name, &SelectedIdx, ItemsPtr.data(), static_cast<int>(ItemsPtr.size())))
	{
		if (SelectedIdx >= 0 && SelectedIdx < static_cast<int>(CachedPhysicsAssetPaths.size()))
		{
			// PhysicsAsset 직접 할당
			if (CachedPhysicsAssetPaths[SelectedIdx].empty())
			{
				*PhysicsAssetPtr = nullptr;
			}
			else
			{
				*PhysicsAssetPtr = UResourceManager::GetInstance().Load<UPhysicsAsset>(CachedPhysicsAssetPaths[SelectedIdx]);
			}
			return true;
		}
	}

	// 툴팁: 전체 경로 표시
	if (ImGui::IsItemHovered())
	{
		ImGui::BeginTooltip();
		if (CurrentPath.empty())
		{
			ImGui::Text("None");
		}
		else
		{
			ImGui::TextUnformatted(CurrentPath.c_str());
		}
		ImGui::EndTooltip();
	}

	return false;
}

bool UPropertyRenderer::RenderMaterialProperty(const FProperty& Prop, void* Instance)
{
	UMaterialInterface** MaterialPtr = Prop.GetValuePtr<UMaterialInterface*>(Instance);
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
	TArray<UMaterialInterface*>* MaterialSlots = Prop.GetValuePtr<TArray<UMaterialInterface*>>(Instance);
	if (!MaterialSlots)
	{
		ImGui::Text("%s: [Invalid Array Ptr]", Prop.Name);
		return false;
	}

	UObject* OwningObject = static_cast<UObject*>(Instance);
	bool bArrayChanged = false;

	// 배열의 각 요소를 순회하며 헬퍼 함수 호출 (매개변수 없음)
	for (int32 MaterialIndex = 0; MaterialIndex < MaterialSlots->Num(); ++MaterialIndex)
	{
		FString Label = FString(Prop.Name) + " [" + std::to_string(MaterialIndex) + "]";
		UMaterialInterface** MaterialPtr = &(*MaterialSlots)[MaterialIndex];

		if (RenderSingleMaterialSlot(Label.c_str(), MaterialPtr, OwningObject, MaterialIndex))
		{
			bArrayChanged = true;
		}

		// 마지막 머티리얼은 제외하고 영역 분리
		if (MaterialIndex < MaterialSlots->Num() - 1)
		{
			ImGui::NewLine();
			ImGui::Separator();
			ImGui::NewLine();
		}
	}

	return bArrayChanged;
}

bool UPropertyRenderer::RenderSingleMaterialSlot(const char* Label, UMaterialInterface** MaterialPtr, UObject* OwningObject, uint32 MaterialIndex)
{
	bool bElementChanged = false;
	UMaterialInterface* CurrentMaterial = *MaterialPtr;

	// 파티클 모듈 여부 체크
	UParticleModule* ParticleModule = Cast<UParticleModule>(OwningObject);

	// 파티클 모듈 전용 UI
	if (ParticleModule)
	{
		// 텍스처 캐시 확인
		if (CachedTextureItems.empty())
		{
			ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "텍스처 캐시가 비어있습니다.");
			return false;
		}

		// 현재 머티리얼의 Diffuse 텍스처 가져오기
		UTexture* CurrentDiffuse = CurrentMaterial ? CurrentMaterial->GetTexture(EMaterialTextureSlot::Diffuse) : nullptr;
		FString CurrentTexturePath = CurrentDiffuse ? CurrentDiffuse->GetFilePath() : "None";

		// 썸네일 + 콤보박스 레이아웃
		ImGui::BeginGroup();

		// 썸네일 (64x64)
		const float ThumbnailSize = 64.0f;
		ImVec2 ThumbnailSizeVec(ThumbnailSize, ThumbnailSize);

		if (CurrentDiffuse && CurrentDiffuse->GetShaderResourceView())
		{
			ImGui::Image((void*)CurrentDiffuse->GetShaderResourceView(), ThumbnailSizeVec);
		}
		else
		{
			// 빈 썸네일 (검정 배경)
			ImVec2 pos = ImGui::GetCursorScreenPos();
			ImDrawList* drawList = ImGui::GetWindowDrawList();
			drawList->AddRectFilled(pos, ImVec2(pos.x + ThumbnailSize, pos.y + ThumbnailSize), IM_COL32(30, 30, 30, 255));
			drawList->AddRect(pos, ImVec2(pos.x + ThumbnailSize, pos.y + ThumbnailSize), IM_COL32(60, 60, 60, 255));
			ImGui::Dummy(ThumbnailSizeVec);
		}

		ImGui::SameLine();

		// 텍스처 선택 콤보박스
		ImGui::BeginGroup();
		ImGui::Text("%s", Label);

		// UMaterial로 캐스팅 (SetMaterialInfo, ResolveTextures는 UMaterial에만 있음)
		UMaterial* Material = Cast<UMaterial>(CurrentMaterial);

		ImGui::SetNextItemWidth(200);
		FString ComboLabel = "##ParticleTexture" + FString(Label);
		if (ImGui::BeginCombo(ComboLabel.c_str(), CurrentTexturePath.c_str()))
		{
			// 미리보기 썸네일 크기
			const float PreviewSize = 96.0f;

			// "None" 옵션
			bool bIsNoneSelected = (CurrentDiffuse == nullptr);
			if (ImGui::Selectable("None", bIsNoneSelected))
			{
				if (Material)
				{
					FMaterialInfo Info = Material->GetMaterialInfo();
					Info.DiffuseTextureFileName = "";
					Material->SetMaterialInfo(Info);
					Material->ResolveTextures();
					bElementChanged = true;
				}
			}
			if (bIsNoneSelected) ImGui::SetItemDefaultFocus();

			// 캐시된 텍스처 목록
			for (int j = 0; j < (int)CachedTexturePaths.size(); ++j)
			{
				const FString& Path = CachedTexturePaths[j];
				const char* DisplayName = CachedTextureItems[j + 1];
				bool bIsSelected = (CurrentTexturePath == Path);

				if (ImGui::Selectable(DisplayName, bIsSelected))
				{
					if (Material)
					{
						// 셰이더는 유지하고 텍스처만 변경
						FMaterialInfo Info = Material->GetMaterialInfo();
						Info.DiffuseTextureFileName = Path;
						Material->SetMaterialInfo(Info);
						Material->ResolveTextures();
						bElementChanged = true;
					}
				}

				// 호버링 시 텍스처 미리보기 툴팁
				if (ImGui::IsItemHovered())
				{
					// 텍스처 로드 (캐시에서 가져옴)
					UTexture* PreviewTexture = UResourceManager::GetInstance().Load<UTexture>(Path);
					if (PreviewTexture && PreviewTexture->GetShaderResourceView())
					{
						ImGui::BeginTooltip();
						ImGui::Image((void*)PreviewTexture->GetShaderResourceView(), ImVec2(PreviewSize, PreviewSize));
						ImGui::Text("%s", Path.c_str());
						ImGui::EndTooltip();
					}
				}

				if (bIsSelected) ImGui::SetItemDefaultFocus();
			}

			ImGui::EndCombo();
		}

		ImGui::EndGroup();
		ImGui::EndGroup();

		return bElementChanged;
	}

	// ========== 일반 컴포넌트용 기존 UI ==========

	// 캐시가 비어있으면 아무것도 렌더링하지 않음 (필수)
	if (CachedMaterialItems.empty())
	{
		ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "No materials available in cache.");
		return false;
	}

	// --- 1. 머티리얼 에셋 선택 콤보박스 ---
	FString CurrentMaterialPath = (CurrentMaterial) ? CurrentMaterial->GetFilePath() : "None";

	ImGui::SetNextItemWidth(240);
	if (ImGui::BeginCombo(Label, CurrentMaterialPath.c_str()))
	{
		// "None" 옵션
		bool bIsNoneSelected = (CurrentMaterial == nullptr);
		if (ImGui::Selectable(CachedMaterialItems[0], bIsNoneSelected))
		{
			bElementChanged = true;
			if (UPrimitiveComponent* PrimitiveComponent = Cast<UPrimitiveComponent>(OwningObject))
			{
				PrimitiveComponent->SetMaterial(MaterialIndex, nullptr);
			}
			else
			{
				*MaterialPtr = nullptr;
			}
		}
		if (bIsNoneSelected) ImGui::SetItemDefaultFocus();

		// 캐시된 머티리얼 목록
		for (int j = 0; j < (int)CachedMaterialPaths.size(); ++j)
		{
			const FString& Path = CachedMaterialPaths[j];
			const char* DisplayName = CachedMaterialItems[j + 1];
			bool bIsSelected = (CurrentMaterialPath == Path);

			if (ImGui::Selectable(DisplayName, bIsSelected))
			{
				bElementChanged = true;
				if (UPrimitiveComponent* PrimitiveComponent = Cast<UPrimitiveComponent>(OwningObject))
				{
					PrimitiveComponent->SetMaterialByName(MaterialIndex, Path);
				}
				else
				{
					*MaterialPtr = UResourceManager::GetInstance().Load<UMaterial>(Path);
				}
			}
			if (bIsSelected) ImGui::SetItemDefaultFocus();
		}

		ImGui::EndCombo();
		CurrentMaterial = *MaterialPtr; // 콤보박스에서 변경되었을 수 있으므로 업데이트
	}

	// --- 2. UMaterialInterface 내부 프로퍼티 렌더링 ---
	if (CurrentMaterial)
	{
		ImGui::Indent();

		// --- 2-1. 셰이더 (읽기 전용) ---
		UShader* CurrentShader = CurrentMaterial->GetShader();
		FString CurrentShaderPath = (CurrentShader) ? CurrentShader->GetFilePath() : "None";
		char ShaderPathBuffer[512];
		strncpy_s(ShaderPathBuffer, sizeof(ShaderPathBuffer), CurrentShaderPath.c_str(), _TRUNCATE);
		FString ShaderLabel = "Shader##" + FString(Label);
		ImGui::SetNextItemWidth(300);
		ImGui::BeginDisabled(true);
		ImGui::InputText(ShaderLabel.c_str(), ShaderPathBuffer, sizeof(ShaderPathBuffer), ImGuiInputTextFlags_ReadOnly);
		ImGui::EndDisabled();

		// --- 셰이더 매크로 (읽기 전용) ---
		FString MacroKey = UShader::GenerateMacrosToString(CurrentMaterial->GetShaderMacros());
		char ShaderMacroBuffer[512];
		strncpy_s(ShaderMacroBuffer, sizeof(ShaderMacroBuffer), MacroKey.c_str(), _TRUNCATE);
		FString ShaderMacroKeyLabel = "MacroKey##" + FString(Label);
		ImGui::SetNextItemWidth(300);
		ImGui::BeginDisabled(true);
		ImGui::InputText(ShaderMacroKeyLabel.c_str(), ShaderMacroBuffer, sizeof(ShaderMacroBuffer), ImGuiInputTextFlags_ReadOnly);
		ImGui::EndDisabled();

		// --- 2-2. 텍스처 슬롯 ---
		// ImGui 위젯 값이 변경될 때만 호출됩니다.
		UMeshComponent* MeshComponent = Cast<UMeshComponent>(OwningObject);

		if (!MeshComponent)
		{
			ImGui::Text("UMeshComponent만 텍스처를 변경할 수 있습니다");
			ImGui::Unindent();
			return bElementChanged;
		}

		for (uint8 TexSlotIndex = 0; TexSlotIndex < (uint8)EMaterialTextureSlot::Max; ++TexSlotIndex)
		{
			EMaterialTextureSlot Slot = static_cast<EMaterialTextureSlot>(TexSlotIndex);
			UTexture* CurrentTexture = CurrentMaterial->GetTexture(Slot);

			const char* SlotName = "Unknown Slot";
			switch (Slot)
			{
			case EMaterialTextureSlot::Diffuse: SlotName = "Diffuse Texture"; break;
			case EMaterialTextureSlot::Normal: SlotName = "Normal Texture"; break;
			}
			FString TextureLabel = FString(SlotName) + "##" + Label;

			UTexture* NewTexture = nullptr;
			if (RenderTextureSelectionCombo(TextureLabel.c_str(), CurrentTexture, NewTexture))
			{
				MeshComponent->SetMaterialTextureByUser(MaterialIndex, Slot, NewTexture);
				bElementChanged = true;
			}
		}
		

		// --- 2-3. FMaterialInfo 파라미터 (스칼라 및 벡터) ---

		// ImGui에 표시하기 위해 현재 머티리얼의 최종 Info 값을 가져옵니다.
		// (MID인 경우 덮어쓰기가 적용된 복사본, UMaterial인 경우 원본)
		const FMaterialInfo& Info = CurrentMaterial->GetMaterialInfo();

		ImGui::Separator();

		// --- Colors (FVector -> ImGui::ColorEdit3) ---
		FLinearColor TempColor; // ImGui 위젯에 바인딩할 임시 변수

		// DiffuseColor
		TempColor = FLinearColor(Info.DiffuseColor); // FVector에서 FLinearColor로 변환 (생성자 가정)
		FString DiffuseLabel = "Diffuse Color##" + FString(Label);
		if (ImGui::ColorEdit3(DiffuseLabel.c_str(), &TempColor.R))
		{
			MeshComponent->SetMaterialColorByUser(MaterialIndex, "DiffuseColor", TempColor);
			bElementChanged = true;
		}

		// AmbientColor
		TempColor = FLinearColor(Info.AmbientColor);
		FString AmbientLabel = "Ambient Color##" + FString(Label);
		if (ImGui::ColorEdit3(AmbientLabel.c_str(), &TempColor.R))
		{
			MeshComponent->SetMaterialColorByUser(MaterialIndex, "AmbientColor", TempColor);
			bElementChanged = true;
		}

		// SpecularColor
		TempColor = FLinearColor(Info.SpecularColor);
		FString SpecularLabel = "Specular Color##" + FString(Label);
		if (ImGui::ColorEdit3(SpecularLabel.c_str(), &TempColor.R))
		{
			MeshComponent->SetMaterialColorByUser(MaterialIndex, "SpecularColor", TempColor);
			bElementChanged = true;
		}

		// EmissiveColor
		TempColor = FLinearColor(Info.EmissiveColor);
		FString EmissiveLabel = "Emissive Color##" + FString(Label);
		if (ImGui::ColorEdit3(EmissiveLabel.c_str(), &TempColor.R))
		{
			MeshComponent->SetMaterialColorByUser(MaterialIndex, "EmissiveColor", TempColor);
			bElementChanged = true;
		}

		// TransmissionFilter
		TempColor = FLinearColor(Info.TransmissionFilter);
		FString TransmissionLabel = "Transmission Filter##" + FString(Label);
		if (ImGui::ColorEdit3(TransmissionLabel.c_str(), &TempColor.R))
		{
			MeshComponent->SetMaterialColorByUser(MaterialIndex, "TransmissionFilter", TempColor);
			bElementChanged = true;
		}

		// --- Floats (float -> ImGui::DragFloat) ---
		float TempFloat; // ImGui 위젯에 바인딩할 임시 변수

		// SpecularExponent (Shininess)
		TempFloat = Info.SpecularExponent;
		FString SpecExpLabel = "Specular Exponent##" + FString(Label);
		if (ImGui::DragFloat(SpecExpLabel.c_str(), &TempFloat, 1.0f, 0.0f, 1024.0f))
		{
			MeshComponent->SetMaterialScalarByUser(MaterialIndex, "SpecularExponent", TempFloat);
			bElementChanged = true;
		}

		// Transparency (d or Tr)
		TempFloat = Info.Transparency;
		FString TransparencyLabel = "Transparency##" + FString(Label);
		if (ImGui::DragFloat(TransparencyLabel.c_str(), &TempFloat, 0.01f, 0.0f, 1.0f))
		{
			MeshComponent->SetMaterialScalarByUser(MaterialIndex, "Transparency", TempFloat);
			bElementChanged = true;
		}

		// OpticalDensity (Ni)
		TempFloat = Info.OpticalDensity;
		FString OpticalDensityLabel = "Optical Density##" + FString(Label);
		if (ImGui::DragFloat(OpticalDensityLabel.c_str(), &TempFloat, 0.01f, 0.0f, 10.0f)) // 범위는 임의로 지정
		{
			MeshComponent->SetMaterialScalarByUser(MaterialIndex, "OpticalDensity", TempFloat);
			bElementChanged = true;
		}

		// BumpMultiplier (bm)
		TempFloat = Info.BumpMultiplier;
		FString BumpMultiplierLabel = "Bump Multiplier##" + FString(Label);
		if (ImGui::DragFloat(BumpMultiplierLabel.c_str(), &TempFloat, 0.01f, 0.0f, 5.0f)) // 범위는 임의로 지정
		{
			MeshComponent->SetMaterialScalarByUser(MaterialIndex, "BumpMultiplier", TempFloat);
			bElementChanged = true;
		}

		// --- Ints (IlluminationModel) ---
		int TempInt = Info.IlluminationModel;
		FString IllumModelLabel = "Illum Model##" + FString(Label);
		if (ImGui::DragInt(IllumModelLabel.c_str(), &TempInt, 1, 0, 10)) // 0-10은 OBJ 표준 범위
		{
			MeshComponent->SetMaterialScalarByUser(MaterialIndex, "IlluminationModel", (float)TempInt);
			bElementChanged = true;
		}

		ImGui::Unindent();
	}

	return bElementChanged;
}

// ===== 텍스처 선택 헬퍼 함수 =====
bool UPropertyRenderer::RenderTextureSelectionCombo(const char* Label, UTexture* CurrentTexture, UTexture*& OutNewTexture)
{
	// 텍스처 캐시가 비어있으면 렌더링하지 않음
	if (CachedTextureItems.empty())
	{
		ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "No textures available in cache.");
		return false;
	}

	bool bChanged = false;
	FString CurrentTexturePath = (CurrentTexture) ? CurrentTexture->GetFilePath() : "None";

	// 1. 현재 선택된 텍스처의 인덱스 찾기
	int SelectedTextureIdx = 0; // "None"
	for (int j = 0; j < (int)CachedTexturePaths.size(); ++j)
	{
		if (CachedTexturePaths[j] == CurrentTexturePath)
		{
			SelectedTextureIdx = j + 1; // 0번은 "None"이므로 +1
			break;
		}
	}

	// 2. 콤보박스 왼쪽에 썸네일 표시
	const float ThumbnailSize = 24.0f;
	const ImVec2 ThumbnailImVec(ThumbnailSize, ThumbnailSize);

	if (CurrentTexture && CurrentTexture->GetShaderResourceView())
	{
		ImGui::Image(reinterpret_cast<ImTextureID>(CurrentTexture->GetShaderResourceView()), ThumbnailImVec);

		// 썸네일 이미지에 마우스를 올렸을 때 이미지/크기 툴팁
		if (ImGui::IsItemHovered())
		{
			ImGui::BeginTooltip();
			ImGui::TextUnformatted(CurrentTexture->GetFilePath().c_str());
			ImGui::Text("Size: %u x %u", CurrentTexture->GetWidth(), CurrentTexture->GetHeight());

			// 썸네일 크기 계산 (가로/세로 비율 유지, 최대 128x128)
			const float MAX_THUMBNAIL_SIZE = 128.0f;
			float width = (float)CurrentTexture->GetWidth();
			float height = (float)CurrentTexture->GetHeight();
			float ratio = width / height;

			ImVec2 thumbSize;
			if (ratio > 1.0f) // 가로가 더 긴 이미지
			{
				thumbSize = { MAX_THUMBNAIL_SIZE, MAX_THUMBNAIL_SIZE / ratio };
			}
			else // 세로가 더 길거나 같은 이미지
			{
				thumbSize = { MAX_THUMBNAIL_SIZE * ratio, MAX_THUMBNAIL_SIZE };
			}

			ImGui::Image(reinterpret_cast<ImTextureID>(CurrentTexture->GetShaderResourceView()), thumbSize);
			ImGui::EndTooltip();
		}
	}
	else
	{
		// 리소스가 없을 때도 레이아웃 유지를 위해 Dummy 사용
		ImGui::Dummy(ThumbnailImVec);
	}

	ImGui::SameLine();

	// 3. 커스텀 콤보박스 시작
	// 콤보박스에 표시될 텍스트 (현재 선택된 항목의 텍스트)
	const char* PreviewText = CachedTextureItems[SelectedTextureIdx];

	// SetNextItemWidth를 썸네일 크기만큼 보정
	ImGui::SetNextItemWidth(220.0f - ThumbnailSize - ImGui::GetStyle().ItemSpacing.x);

	if (ImGui::BeginCombo(Label, PreviewText))
	{
		// --- 콤보박스 드롭다운 리스트 렌더링 ---

		// 드롭다운 리스트 내부의 아이템 간 수직 간격 설정
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(ImGui::GetStyle().ItemSpacing.x, 1.0f));

		// 텍스처 리스트 (미리보기 포함) - "None" 옵션(i=0)을 루프에 포함
		for (int i = 0; i < (int)CachedTextureItems.size(); ++i)
		{
			bool is_selected = (SelectedTextureIdx == i);
			const char* ItemText = CachedTextureItems[i];

			UTexture* previewTexture = nullptr;
			FString TexturePath = "None";

			if (i > 0)
			{
				TexturePath = CachedTexturePaths[i - 1];
				previewTexture = UResourceManager::GetInstance().Load<UTexture>(TexturePath);
			}

			FString SelectableID = FString("##Selectable_") + TexturePath + std::to_string(i) + Label; // Label을 추가하여 ID 고유성 보장

			// 1. Selectable을 먼저 호출
			if (ImGui::Selectable(SelectableID.c_str(), is_selected, 0, ImVec2(0, ThumbnailSize - 1.0f)))
			{
				// 선택된 텍스처 리소스를 경로로부터 로드합니다.
				UTexture* SelectedTexture = (i == 0)
					? nullptr
					: UResourceManager::GetInstance().Load<UTexture>(TexturePath);

				// OutNewTexture에 선택된 텍스처를 할당합니다.
				OutNewTexture = SelectedTexture;
				bChanged = true;
			}
			ImVec2 NextItemCursorPos = ImGui::GetCursorPos(); // 다음 아이템 위치 저장

			// 2. 툴팁 표시
			if (ImGui::IsItemHovered())
			{
				ImGui::BeginTooltip();
				if (previewTexture && previewTexture->GetTexture2D() && previewTexture->GetShaderResourceView())
				{
					ImGui::TextUnformatted(previewTexture->GetFilePath().c_str());
					ImGui::Text("Size: %u x %u", previewTexture->GetWidth(), previewTexture->GetHeight());
					float width = (float)previewTexture->GetWidth();
					float height = (float)previewTexture->GetHeight();
					float ratio = width / height;
					ImVec2 thumbSize = (ratio > 1.0f) ? ImVec2(128.0f, 128.0f / ratio) : ImVec2(128.0f * ratio, 128.0f);
					ImGui::Image(reinterpret_cast<ImTextureID>(previewTexture->GetShaderResourceView()), thumbSize);
				}
				else
				{
					ImGui::TextUnformatted("텍스처 없음");
				}
				ImGui::EndTooltip();
			}

			// 3. Selectable 위에 겹쳐 그리기 위해 커서를 되돌림
			ImVec2 ItemPos = ImGui::GetItemRectMin();

			// 4. 내용물(썸네일) 그리기
			ImVec2 ImagePos = ImVec2(ItemPos.x, ItemPos.y);
			ImGui::SetCursorScreenPos(ImagePos);


			if (previewTexture && previewTexture->GetShaderResourceView())
			{
				ImGui::Image(reinterpret_cast<ImTextureID>(previewTexture->GetShaderResourceView()), ThumbnailImVec);
			}
			else
			{
				ImGui::Dummy(ThumbnailImVec);
			}

			// 5. 내용물(텍스트) 그리기
			ImVec2 TextPos = ImVec2(
				ItemPos.x + ThumbnailSize + ImGui::GetStyle().ItemSpacing.x,
				ItemPos.y
			);
			ImGui::SetCursorScreenPos(TextPos);
			ImGui::TextUnformatted(ItemText);

			if (is_selected)
				ImGui::SetItemDefaultFocus();

			// 6. 커서 위치 복원
			ImGui::SetCursorPos(NextItemCursorPos);

			// --- 어설션 해결을 위해 Dummy 위젯 추가 ---
			ImGui::Dummy(ImVec2(0.0f, 0.0f));
		}

		// PushStyleVar로 변경했던 스타일을 원래대로 복원합니다.
		ImGui::PopStyleVar();

		ImGui::EndCombo();
	}

	// 닫힌 콤보박스 '텍스트' 부분에 마우스를 올렸을 때 전체 경로 툴팁
	if (ImGui::IsItemHovered())
	{
		ImGui::BeginTooltip();

		// 1. 원본 경로 표시 (CurrentTexturePath는 null일 때 "None"을 가짐)
		ImGui::TextUnformatted(CurrentTexturePath.c_str());

		// 2. CurrentTexture가 유효하고 캐시 파일 경로가 있다면 추가로 표시
		if (CurrentTexture)
		{
			const FString& CachedPath = CurrentTexture->GetCacheFilePath();
			if (!CachedPath.empty())
			{
				ImGui::Separator(); // 원본 경로와 구분하기 위해 선 추가
				ImGui::Text("Cache: %s", CachedPath.c_str());
			}
		}

		ImGui::EndTooltip();
	}

	return bChanged;
}
 



bool UPropertyRenderer::RenderSoundSelectionComboSimple(const char* Label, USound* CurrentSound, USound*& OutNewSound)
{
    if (CachedSoundItems.empty())
    {
        ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "No Sound available in cache.");
        return false;
    }

    bool bChanged = false;
    FString CurrentSoundPath = (CurrentSound) ? CurrentSound->GetFilePath() : "None";

    int SelectedSoundIdx = 0; // 0 = None
    for (int j = 0; j < (int)CachedSoundPaths.size(); ++j)
    {
        if (CachedSoundPaths[j] == CurrentSoundPath)
        {
            SelectedSoundIdx = j + 1; // +1 for "None"
            break;
        }
    }

    const char* PreviewText = CachedSoundItems[SelectedSoundIdx];
    ImGui::SetNextItemWidth(220.0f);
    if (ImGui::BeginCombo(Label, PreviewText))
    {
        for (int i = 0; i < (int)CachedSoundItems.size(); ++i)
        {
            bool is_selected = (SelectedSoundIdx == i);
            const char* ItemText = CachedSoundItems[i];

            if (ImGui::Selectable(ItemText, is_selected))
            {
                USound* SelectedSound = (i == 0)
                    ? nullptr
                    : UResourceManager::GetInstance().Load<USound>(CachedSoundPaths[i - 1]);
                OutNewSound = SelectedSound;
                bChanged = true;
            }

            if (is_selected)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    return bChanged;
}


bool UPropertyRenderer::RenderTransformProperty(const FProperty& Prop, void* Instance)
{
	FVector* Value = Prop.GetValuePtr<FVector>(Instance);
	bool bAnyChanged = false;

	// 축 색상 정의
	ImVec4 ColorX(0.85f, 0.2f, 0.2f, 1.0f);   // 부드러운 빨간색
	ImVec4 ColorY(0.3f, 0.8f, 0.3f, 1.0f);    // 부드러운 초록색
	ImVec4 ColorZ(0.2f, 0.5f, 1.0f, 1.0f);    // 밝은 파란색

	float Speed = 0.1f;

	// 라벨 너비 설정 (Location, Rotation, Scale 모두 동일한 너비로 정렬)
	const float LabelWidth = 150.0f;
	const float InputWidth = 70.0f;
	const float ResetButtonWidth = 50.0f;
	const float FrameHeight = ImGui::GetFrameHeight(); // 입력 필드 높이
	const float ColorBoxWidth = FrameHeight * 0.25f;
	const float ColorBoxHeight = FrameHeight;
	const float rounding = ImGui::GetStyle().FrameRounding;

	// 프로퍼티별 고유 ID 스코프 시작
	ImGui::PushID(Prop.Name);

	// 라벨 출력
	ImGui::Text("%s", Prop.Name);
	ImGui::SameLine(LabelWidth);

	ImDrawList* DrawList = ImGui::GetWindowDrawList();
	const char* format = " %.3f"; // 숫자 앞에 패딩 추가

	// X축 (Red)
	ImGui::PushID("X");
	ImGui::PushItemWidth(InputWidth + ColorBoxWidth);
	if (ImGui::DragFloat("##X", &Value->X, Speed, 0.0f, 0.0f, format))
	{
		bAnyChanged = true;
	}
	ImGui::PopItemWidth();
	ImVec2 posX = ImGui::GetItemRectMin();
	DrawList->AddRectFilled(posX, ImVec2(posX.x + ColorBoxWidth, posX.y + ColorBoxHeight), ImGui::ColorConvertFloat4ToU32(ColorX), rounding, ImDrawFlags_RoundCornersLeft);
	ImGui::PopID();
	ImGui::SameLine();

	// Y축 (Green)
	ImGui::PushID("Y");
	ImGui::PushItemWidth(InputWidth + ColorBoxWidth);
	if (ImGui::DragFloat("##Y", &Value->Y, Speed, 0.0f, 0.0f, format))
	{
		bAnyChanged = true;
	}
	ImGui::PopItemWidth();
	ImVec2 posY = ImGui::GetItemRectMin();
	DrawList->AddRectFilled(posY, ImVec2(posY.x + ColorBoxWidth, posY.y + ColorBoxHeight), ImGui::ColorConvertFloat4ToU32(ColorY), rounding, ImDrawFlags_RoundCornersLeft);
	ImGui::PopID();
	ImGui::SameLine();

	// Z축 (Blue)
	ImGui::PushID("Z");
	ImGui::PushItemWidth(InputWidth + ColorBoxWidth);
	if (ImGui::DragFloat("##Z", &Value->Z, Speed, 0.0f, 0.0f, format))
	{
		bAnyChanged = true;
	}
	ImGui::PopItemWidth();
	ImVec2 posZ = ImGui::GetItemRectMin();
	DrawList->AddRectFilled(posZ, ImVec2(posZ.x + ColorBoxWidth, posZ.y + ColorBoxHeight), ImGui::ColorConvertFloat4ToU32(ColorZ), rounding, ImDrawFlags_RoundCornersLeft);
	ImGui::PopID();
	ImGui::SameLine();

	// Reset 버튼
	ImGui::PushID("Reset");
	if (ImGui::Button("Reset", ImVec2(ResetButtonWidth, 0)))
	{
		// 프로퍼티에 따라 기본값 설정
		if (strcmp(Prop.Name, "RelativeLocation") == 0)
		{
			*Value = FVector(0.0f, 0.0f, 0.0f);
		}
		else if (strcmp(Prop.Name, "RelativeRotationEuler") == 0)
		{
			*Value = FVector(0.0f, 0.0f, 0.0f);
		}
		else if (strcmp(Prop.Name, "RelativeScale") == 0)
		{
			*Value = FVector(1.0f, 1.0f, 1.0f);
		}
		bAnyChanged = true;
	}
	ImGui::PopID();

	// 프로퍼티별 고유 ID 스코프 종료
	ImGui::PopID();

	return bAnyChanged;
}

// ============================================================
// Distribution 렌더링 함수들
// ============================================================

bool UPropertyRenderer::RenderDistributionFloatModeCombo(const char* Label, EDistributionType& Type)
{
	static const char* ModeNames[] = {
		"분포 플로트 상수",
		"분포 플로트 유니폼",
		"분포 플로트 상수 커브",
		"분포 플로트 유니폼 커브",
		"분포 플로트 파티클 파라미터"
	};

	int CurrentMode = static_cast<int>(Type);
	if (ImGui::Combo(Label, &CurrentMode, ModeNames, IM_ARRAYSIZE(ModeNames)))
	{
		Type = static_cast<EDistributionType>(CurrentMode);
		return true;
	}
	return false;
}

bool UPropertyRenderer::RenderDistributionVectorModeCombo(const char* Label, EDistributionType& Type)
{
	static const char* ModeNames[] = {
		"분포 벡터 상수",
		"분포 벡터 유니폼",
		"분포 벡터 상수 커브",
		"분포 벡터 유니폼 커브",
		"분포 벡터 파티클 파라미터"
	};

	int CurrentMode = static_cast<int>(Type);
	if (ImGui::Combo(Label, &CurrentMode, ModeNames, IM_ARRAYSIZE(ModeNames)))
	{
		Type = static_cast<EDistributionType>(CurrentMode);
		return true;
	}
	return false;
}

// 보간 모드 콤보박스 렌더링
static bool RenderInterpModeCombo(const char* Label, EInterpCurveMode& Mode)
{
	static const char* ModeNames[] = {
		"Linear",
		"Constant",
		"Curve",
		"Curve Auto",
		"Curve Auto Clamped"
	};

	int CurrentMode = static_cast<int>(Mode);
	if (ImGui::Combo(Label, &CurrentMode, ModeNames, IM_ARRAYSIZE(ModeNames)))
	{
		Mode = static_cast<EInterpCurveMode>(CurrentMode);
		return true;
	}
	return false;
}

bool UPropertyRenderer::RenderInterpCurveFloat(const char* Label, FInterpCurveFloat& Curve)
{
	bool bChanged = false;

	if (ImGui::TreeNode(Label))
	{
		// 상수 커브 섹션
		if (ImGui::TreeNode("상수 커브"))
		{
			// 포인트 배열 헤더
			ImGui::Text("포인트");
			ImGui::SameLine();
			ImGui::TextDisabled("배열 엘리먼트: %d", Curve.Points.Num());
			ImGui::SameLine();

			// 포인트 추가 버튼
			if (ImGui::SmallButton("+"))
			{
				float NewTime = Curve.Points.IsEmpty() ? 0.0f : Curve.Points.Last().InVal + 0.1f;
				float NewValue = Curve.Points.IsEmpty() ? 0.0f : Curve.Points.Last().OutVal;
				Curve.AddPoint(NewTime, NewValue);
				bChanged = true;
			}
			ImGui::SameLine();

			// 마지막 포인트 삭제 버튼
			if (ImGui::SmallButton("-") && Curve.Points.Num() > 0)
			{
				Curve.Points.RemoveAt(Curve.Points.Num() - 1);
				bChanged = true;
			}

			// 삭제 요청 추적 (지연 삭제 패턴)
			int32 DeleteRequestIndex = -1;

			// 각 포인트 렌더링 (인덱스 기반 - 언리얼 스타일)
			for (int32 i = 0; i < Curve.Points.Num(); ++i)
			{
				ImGui::PushID(i);
				FInterpCurvePointFloat& Point = Curve.Points[i];

				char NodeLabel[32];
				sprintf_s(NodeLabel, "인덱스 [%d]", i);

				bool bNodeOpen = ImGui::TreeNode(NodeLabel);

				// 드롭다운 메뉴 버튼
				ImGui::SameLine();
				if (ImGui::SmallButton("▼"))
				{
					ImGui::OpenPopup("##menu");
				}

				// 드롭다운 메뉴 팝업
				if (ImGui::BeginPopup("##menu"))
				{
					if (ImGui::MenuItem("삽입"))
					{
						float NewTime = Point.InVal + 0.1f;
						float NewValue = Point.OutVal;
						Curve.Points.Insert(FInterpCurvePointFloat(NewTime, NewValue), i + 1);
						bChanged = true;
					}
					if (ImGui::MenuItem("삭제"))
					{
						DeleteRequestIndex = i;
						bChanged = true;
					}
					if (ImGui::MenuItem("복제"))
					{
						Curve.Points.Insert(Point, i + 1);
						bChanged = true;
					}
					ImGui::EndPopup();
				}

				if (bNodeOpen)
				{
					ImGui::TextDisabled("멤버: 5");

					// In 값
					if (ImGui::DragFloat("In 값", &Point.InVal, 0.01f))
						bChanged = true;

					// Out Val
					if (ImGui::DragFloat("Out Val", &Point.OutVal, 0.01f))
						bChanged = true;

					// 도착 탄젠트
					if (ImGui::DragFloat("도착 탄젠트", &Point.ArriveTangent, 0.01f))
						bChanged = true;

					// 출발 탄젠트
					if (ImGui::DragFloat("출발 탄젠트", &Point.LeaveTangent, 0.01f))
						bChanged = true;

					// 보간 모드
					if (RenderInterpModeCombo("보간 모드", Point.InterpMode))
						bChanged = true;

					ImGui::TreePop();
				}

				ImGui::PopID();
			}

			// 루프 종료 후 삭제 처리 (ImGui 상태 안전)
			if (DeleteRequestIndex >= 0)
			{
				Curve.Points.RemoveAt(DeleteRequestIndex);
			}

			ImGui::TreePop();
		}

		// 루프 옵션
		if (ImGui::Checkbox("루프 여부", &Curve.bIsLooped))
			bChanged = true;

		if (ImGui::DragFloat("루프 키 오프셋", &Curve.LoopKeyOffset, 0.01f))
			bChanged = true;

		ImGui::TreePop();
	}

	return bChanged;
}

bool UPropertyRenderer::RenderInterpCurveVector(const char* Label, FInterpCurveVector& Curve)
{
	bool bChanged = false;

	if (ImGui::TreeNode(Label))
	{
		// 상수 커브 섹션
		if (ImGui::TreeNode("상수 커브"))
		{
			// 포인트 배열 헤더
			ImGui::Text("포인트");
			ImGui::SameLine();
			ImGui::TextDisabled("배열 엘리먼트: %d", Curve.Points.Num());
			ImGui::SameLine();

			// 포인트 추가 버튼
			if (ImGui::SmallButton("+"))
			{
				float NewTime = Curve.Points.IsEmpty() ? 0.0f : Curve.Points.Last().InVal + 0.1f;
				FVector NewValue = Curve.Points.IsEmpty() ? FVector(0, 0, 0) : Curve.Points.Last().OutVal;
				Curve.AddPoint(NewTime, NewValue);
				bChanged = true;
			}
			ImGui::SameLine();

			// 마지막 포인트 삭제 버튼
			if (ImGui::SmallButton("-") && Curve.Points.Num() > 0)
			{
				Curve.Points.RemoveAt(Curve.Points.Num() - 1);
				bChanged = true;
			}

			// 삭제 요청 추적 (지연 삭제 패턴)
			int32 DeleteRequestIndex = -1;

			// 각 포인트 렌더링 (인덱스 기반 - 언리얼 스타일)
			for (int32 i = 0; i < Curve.Points.Num(); ++i)
			{
				ImGui::PushID(i);
				FInterpCurvePointVector& Point = Curve.Points[i];

				char NodeLabel[32];
				sprintf_s(NodeLabel, "인덱스 [%d]", i);

				bool bNodeOpen = ImGui::TreeNode(NodeLabel);

				// 드롭다운 메뉴 버튼
				ImGui::SameLine();
				if (ImGui::SmallButton("▼"))
				{
					ImGui::OpenPopup("##menu");
				}

				// 드롭다운 메뉴 팝업
				if (ImGui::BeginPopup("##menu"))
				{
					if (ImGui::MenuItem("삽입"))
					{
						float NewTime = Point.InVal + 0.1f;
						FVector NewValue = Point.OutVal;
						Curve.Points.Insert(FInterpCurvePointVector(NewTime, NewValue), i + 1);
						bChanged = true;
					}
					if (ImGui::MenuItem("삭제"))
					{
						DeleteRequestIndex = i;
						bChanged = true;
					}
					if (ImGui::MenuItem("복제"))
					{
						Curve.Points.Insert(Point, i + 1);
						bChanged = true;
					}
					ImGui::EndPopup();
				}

				if (bNodeOpen)
				{
					ImGui::TextDisabled("멤버: 5");

					// In 값
					if (ImGui::DragFloat("In 값", &Point.InVal, 0.01f))
						bChanged = true;

					// Out Val (Vector3)
					if (ImGui::DragFloat3("Out Val", &Point.OutVal.X, 0.01f))
						bChanged = true;

					// 도착 탄젠트 (Vector3)
					if (ImGui::DragFloat3("도착 탄젠트", &Point.ArriveTangent.X, 0.01f))
						bChanged = true;

					// 출발 탄젠트 (Vector3)
					if (ImGui::DragFloat3("출발 탄젠트", &Point.LeaveTangent.X, 0.01f))
						bChanged = true;

					// 보간 모드
					if (RenderInterpModeCombo("보간 모드", Point.InterpMode))
						bChanged = true;

					ImGui::TreePop();
				}

				ImGui::PopID();
			}

			// 루프 종료 후 삭제 처리 (ImGui 상태 안전)
			if (DeleteRequestIndex >= 0)
			{
				Curve.Points.RemoveAt(DeleteRequestIndex);
			}

			ImGui::TreePop();
		}

		// 루프 옵션
		if (ImGui::Checkbox("루프 여부", &Curve.bIsLooped))
			bChanged = true;

		if (ImGui::DragFloat("루프 키 오프셋", &Curve.LoopKeyOffset, 0.01f))
			bChanged = true;

		ImGui::TreePop();
	}

	return bChanged;
}

bool UPropertyRenderer::RenderInterpCurveFloatUniform(const char* Label, FInterpCurveFloat& MinCurve, FInterpCurveFloat& MaxCurve)
{
	bool bChanged = false;

	if (ImGui::TreeNode(Label))
	{
		// 상수 커브 섹션
		if (ImGui::TreeNode("상수 커브"))
		{
			// 포인트 배열 헤더
			int32 NumPoints = MinCurve.Points.Num();

			ImGui::Text("포인트");
			ImGui::SameLine();
			ImGui::TextDisabled("배열 엘리먼트: %d", NumPoints);
			ImGui::SameLine();
			if (ImGui::SmallButton("+"))
			{
				// 두 커브에 동시에 포인트 추가
				float NewTime = NumPoints > 0 ? MinCurve.Points[NumPoints - 1].InVal + 1.0f : 0.0f;
				MinCurve.Points.Add(FInterpCurvePointFloat(NewTime, 0.0f));
				MaxCurve.Points.Add(FInterpCurvePointFloat(NewTime, 0.0f));
				bChanged = true;
			}
			ImGui::SameLine();
			if (ImGui::SmallButton("-") && NumPoints > 0)
			{
				MinCurve.Points.RemoveAt(NumPoints - 1);
				MaxCurve.Points.RemoveAt(NumPoints - 1);
				bChanged = true;
			}

			// 삭제 요청 추적 (지연 삭제 패턴)
			int32 DeleteRequestIndex = -1;

			// 각 포인트 렌더링 (인덱스 기반 - 언리얼 스타일)
			for (int32 i = 0; i < MinCurve.Points.Num(); ++i)
			{
				ImGui::PushID(i);
				auto& MinPoint = MinCurve.Points[i];
				auto& MaxPoint = MaxCurve.Points[i];

				char NodeLabel[32];
				sprintf_s(NodeLabel, "인덱스 [%d]", i);

				bool bNodeOpen = ImGui::TreeNode(NodeLabel);

				// 드롭다운 메뉴 버튼
				ImGui::SameLine();
				if (ImGui::SmallButton("▼"))
				{
					ImGui::OpenPopup("##menu");
				}

				// 드롭다운 메뉴 팝업
				if (ImGui::BeginPopup("##menu"))
				{
					if (ImGui::MenuItem("삽입"))
					{
						float NewTime = MinPoint.InVal + 0.1f;
						MinCurve.Points.Insert(FInterpCurvePointFloat(NewTime, 0.0f), i + 1);
						MaxCurve.Points.Insert(FInterpCurvePointFloat(NewTime, 0.0f), i + 1);
						bChanged = true;
					}
					if (ImGui::MenuItem("삭제"))
					{
						DeleteRequestIndex = i;
						bChanged = true;
					}
					if (ImGui::MenuItem("복제"))
					{
						MinCurve.Points.Insert(MinPoint, i + 1);
						MaxCurve.Points.Insert(MaxPoint, i + 1);
						bChanged = true;
					}
					ImGui::EndPopup();
				}

				if (bNodeOpen)
				{
					ImGui::TextDisabled("멤버: 5");

					// In 값 (두 커브 동기화)
					if (ImGui::DragFloat("In 값", &MinPoint.InVal, 0.01f))
					{
						MaxPoint.InVal = MinPoint.InVal;
						bChanged = true;
					}

					// Out Val (X, Y 두 개)
					float OutVals[2] = { MinPoint.OutVal, MaxPoint.OutVal };
					if (ImGui::DragFloat2("Out Val", OutVals, 0.01f))
					{
						MinPoint.OutVal = OutVals[0];
						MaxPoint.OutVal = OutVals[1];
						bChanged = true;
					}

					// 도착 탄젠트 (X, Y)
					float ArriveTangents[2] = { MinPoint.ArriveTangent, MaxPoint.ArriveTangent };
					if (ImGui::DragFloat2("도착 탄젠트", ArriveTangents, 0.01f))
					{
						MinPoint.ArriveTangent = ArriveTangents[0];
						MaxPoint.ArriveTangent = ArriveTangents[1];
						bChanged = true;
					}

					// 출발 탄젠트 (X, Y)
					float LeaveTangents[2] = { MinPoint.LeaveTangent, MaxPoint.LeaveTangent };
					if (ImGui::DragFloat2("출발 탄젠트", LeaveTangents, 0.01f))
					{
						MinPoint.LeaveTangent = LeaveTangents[0];
						MaxPoint.LeaveTangent = LeaveTangents[1];
						bChanged = true;
					}

					// 보간 모드 (두 커브 동기화)
					if (RenderInterpModeCombo("보간 모드", MinPoint.InterpMode))
					{
						MaxPoint.InterpMode = MinPoint.InterpMode;
						bChanged = true;
					}

					ImGui::TreePop();
				}

				ImGui::PopID();
			}

			// 루프 종료 후 삭제 처리 (ImGui 상태 안전)
			if (DeleteRequestIndex >= 0)
			{
				MinCurve.Points.RemoveAt(DeleteRequestIndex);
				MaxCurve.Points.RemoveAt(DeleteRequestIndex);
			}

			ImGui::TreePop();
		}

		// 루프 옵션
		if (ImGui::Checkbox("루프 여부", &MinCurve.bIsLooped))
		{
			MaxCurve.bIsLooped = MinCurve.bIsLooped;
			bChanged = true;
		}

		if (ImGui::DragFloat("루프 키 오프셋", &MinCurve.LoopKeyOffset, 0.01f))
		{
			MaxCurve.LoopKeyOffset = MinCurve.LoopKeyOffset;
			bChanged = true;
		}

		ImGui::TreePop();
	}

	return bChanged;
}

bool UPropertyRenderer::RenderInterpCurveVectorUniform(const char* Label, FInterpCurveVector& MinCurve, FInterpCurveVector& MaxCurve)
{
	bool bChanged = false;

	if (ImGui::TreeNode(Label))
	{
		// 상수 커브 섹션
		if (ImGui::TreeNode("상수 커브"))
		{
			// 포인트 배열 헤더
			int32 NumPoints = MinCurve.Points.Num();

			ImGui::Text("포인트");
			ImGui::SameLine();
			ImGui::TextDisabled("배열 엘리먼트: %d", NumPoints);
			ImGui::SameLine();
			if (ImGui::SmallButton("+"))
			{
				float NewTime = NumPoints > 0 ? MinCurve.Points[NumPoints - 1].InVal + 1.0f : 0.0f;
				MinCurve.Points.Add(FInterpCurvePointVector(NewTime, FVector(0.0f, 0.0f, 0.0f)));
				MaxCurve.Points.Add(FInterpCurvePointVector(NewTime, FVector(0.0f, 0.0f, 0.0f)));
				bChanged = true;
			}
			ImGui::SameLine();
			if (ImGui::SmallButton("-") && NumPoints > 0)
			{
				MinCurve.Points.RemoveAt(NumPoints - 1);
				MaxCurve.Points.RemoveAt(NumPoints - 1);
				bChanged = true;
			}

			// 삭제 요청 추적 (지연 삭제 패턴)
			int32 DeleteRequestIndex = -1;

			// 각 포인트 렌더링 (인덱스 기반 - 언리얼 스타일)
			for (int32 i = 0; i < MinCurve.Points.Num(); ++i)
			{
				ImGui::PushID(i);
				auto& MinPoint = MinCurve.Points[i];
				auto& MaxPoint = MaxCurve.Points[i];

				char NodeLabel[32];
				sprintf_s(NodeLabel, "인덱스 [%d]", i);

				bool bNodeOpen = ImGui::TreeNode(NodeLabel);

				// 드롭다운 메뉴 버튼
				ImGui::SameLine();
				if (ImGui::SmallButton("▼"))
				{
					ImGui::OpenPopup("##menu");
				}

				// 드롭다운 메뉴 팝업
				if (ImGui::BeginPopup("##menu"))
				{
					if (ImGui::MenuItem("삽입"))
					{
						float NewTime = MinPoint.InVal + 0.1f;
						MinCurve.Points.Insert(FInterpCurvePointVector(NewTime, FVector(0.0f, 0.0f, 0.0f)), i + 1);
						MaxCurve.Points.Insert(FInterpCurvePointVector(NewTime, FVector(0.0f, 0.0f, 0.0f)), i + 1);
						bChanged = true;
					}
					if (ImGui::MenuItem("삭제"))
					{
						DeleteRequestIndex = i;
						bChanged = true;
					}
					if (ImGui::MenuItem("복제"))
					{
						MinCurve.Points.Insert(MinPoint, i + 1);
						MaxCurve.Points.Insert(MaxPoint, i + 1);
						bChanged = true;
					}
					ImGui::EndPopup();
				}

				if (bNodeOpen)
				{
					ImGui::TextDisabled("멤버: 5");

					// In 값 (두 커브 동기화)
					if (ImGui::DragFloat("In 값", &MinPoint.InVal, 0.01f))
					{
						MaxPoint.InVal = MinPoint.InVal;
						bChanged = true;
					}

					// Out Val (V1, V2)
					if (ImGui::TreeNode("Out Val"))
					{
						if (ImGui::DragFloat3("V1", &MinPoint.OutVal.X, 0.01f))
							bChanged = true;
						if (ImGui::DragFloat3("V2", &MaxPoint.OutVal.X, 0.01f))
							bChanged = true;
						ImGui::TreePop();
					}

					// 도착 탄젠트 (V1, V2)
					if (ImGui::TreeNode("도착 탄젠트"))
					{
						if (ImGui::DragFloat3("V1", &MinPoint.ArriveTangent.X, 0.01f))
							bChanged = true;
						if (ImGui::DragFloat3("V2", &MaxPoint.ArriveTangent.X, 0.01f))
							bChanged = true;
						ImGui::TreePop();
					}

					// 출발 탄젠트 (V1, V2)
					if (ImGui::TreeNode("출발 탄젠트"))
					{
						if (ImGui::DragFloat3("V1", &MinPoint.LeaveTangent.X, 0.01f))
							bChanged = true;
						if (ImGui::DragFloat3("V2", &MaxPoint.LeaveTangent.X, 0.01f))
							bChanged = true;
						ImGui::TreePop();
					}

					// 보간 모드 (두 커브 동기화)
					if (RenderInterpModeCombo("보간 모드", MinPoint.InterpMode))
					{
						MaxPoint.InterpMode = MinPoint.InterpMode;
						bChanged = true;
					}

					ImGui::TreePop();
				}

				ImGui::PopID();
			}

			// 루프 종료 후 삭제 처리 (ImGui 상태 안전)
			if (DeleteRequestIndex >= 0)
			{
				MinCurve.Points.RemoveAt(DeleteRequestIndex);
				MaxCurve.Points.RemoveAt(DeleteRequestIndex);
			}

			ImGui::TreePop();
		}

		// 루프 옵션
		if (ImGui::Checkbox("루프 여부", &MinCurve.bIsLooped))
		{
			MaxCurve.bIsLooped = MinCurve.bIsLooped;
			bChanged = true;
		}

		if (ImGui::DragFloat("루프 키 오프셋", &MinCurve.LoopKeyOffset, 0.01f))
		{
			MaxCurve.LoopKeyOffset = MinCurve.LoopKeyOffset;
			bChanged = true;
		}

		ImGui::TreePop();
	}

	return bChanged;
}

bool UPropertyRenderer::RenderDistributionFloatProperty(const FProperty& Prop, void* Instance)
{
	FDistributionFloat* Dist = Prop.GetValuePtr<FDistributionFloat>(Instance);
	if (!Dist) return false;

	bool bChanged = false;

	if (ImGui::TreeNode(Prop.Name))
	{
		bChanged |= RenderDistributionFloatModeCombo("타입", Dist->Type);

		switch (Dist->Type)
		{
		case EDistributionType::Constant:
			if (ImGui::DragFloat("값", &Dist->ConstantValue, 0.01f))
				bChanged = true;
			break;

		case EDistributionType::Uniform:
			if (ImGui::DragFloat("최소", &Dist->MinValue, 0.01f))
				bChanged = true;
			if (ImGui::DragFloat("최대", &Dist->MaxValue, 0.01f))
				bChanged = true;
			break;

		case EDistributionType::ConstantCurve:
			bChanged |= RenderInterpCurveFloat("커브", Dist->ConstantCurve);
			break;

		case EDistributionType::UniformCurve:
			bChanged |= RenderInterpCurveFloatUniform("상수 커브", Dist->MinCurve, Dist->MaxCurve);
			break;

		case EDistributionType::ParticleParameter:
			{
				char Buffer[256];
				strncpy_s(Buffer, Dist->ParameterName.c_str(), 255);
				if (ImGui::InputText("파라미터명", Buffer, 256))
				{
					Dist->ParameterName = Buffer;
					bChanged = true;
				}
				if (ImGui::DragFloat("기본값", &Dist->ParameterDefaultValue, 0.01f))
					bChanged = true;
			}
			break;
		}

		ImGui::TreePop();
	}

	return bChanged;
}

bool UPropertyRenderer::RenderDistributionVectorProperty(const FProperty& Prop, void* Instance)
{
	FDistributionVector* Dist = Prop.GetValuePtr<FDistributionVector>(Instance);
	if (!Dist) return false;

	bool bChanged = false;

	if (ImGui::TreeNode(Prop.Name))
	{
		bChanged |= RenderDistributionVectorModeCombo("타입", Dist->Type);

		switch (Dist->Type)
		{
		case EDistributionType::Constant:
			if (ImGui::DragFloat3("값", &Dist->ConstantValue.X, 0.01f))
				bChanged = true;
			break;

		case EDistributionType::Uniform:
			if (ImGui::DragFloat3("최소", &Dist->MinValue.X, 0.01f))
				bChanged = true;
			if (ImGui::DragFloat3("최대", &Dist->MaxValue.X, 0.01f))
				bChanged = true;
			break;

		case EDistributionType::ConstantCurve:
			bChanged |= RenderInterpCurveVector("커브", Dist->ConstantCurve);
			break;

		case EDistributionType::UniformCurve:
			bChanged |= RenderInterpCurveVectorUniform("상수 커브", Dist->MinCurve, Dist->MaxCurve);
			break;

		case EDistributionType::ParticleParameter:
			{
				char Buffer[256];
				strncpy_s(Buffer, Dist->ParameterName.c_str(), 255);
				if (ImGui::InputText("파라미터명", Buffer, 256))
				{
					Dist->ParameterName = Buffer;
					bChanged = true;
				}
				if (ImGui::DragFloat3("기본값", &Dist->ParameterDefaultValue.X, 0.01f))
					bChanged = true;
			}
			break;
		}

		ImGui::TreePop();
	}

	return bChanged;
}

bool UPropertyRenderer::RenderDistributionColorProperty(const FProperty& Prop, void* Instance)
{
	FDistributionColor* Dist = Prop.GetValuePtr<FDistributionColor>(Instance);
	if (!Dist) return false;

	bool bChanged = false;

	if (ImGui::TreeNode(Prop.Name))
	{
		// RGB (FDistributionVector)
		if (ImGui::TreeNode("RGB"))
		{
			bChanged |= RenderDistributionVectorModeCombo("타입##RGB", Dist->RGB.Type);

			switch (Dist->RGB.Type)
			{
			case EDistributionType::Constant:
				{
					float RGB[3] = { Dist->RGB.ConstantValue.X, Dist->RGB.ConstantValue.Y, Dist->RGB.ConstantValue.Z };
					if (ImGui::ColorEdit3("값", RGB))
					{
						Dist->RGB.ConstantValue = FVector(RGB[0], RGB[1], RGB[2]);
						bChanged = true;
					}
				}
				break;

			case EDistributionType::Uniform:
				{
					float MinRGB[3] = { Dist->RGB.MinValue.X, Dist->RGB.MinValue.Y, Dist->RGB.MinValue.Z };
					float MaxRGB[3] = { Dist->RGB.MaxValue.X, Dist->RGB.MaxValue.Y, Dist->RGB.MaxValue.Z };
					if (ImGui::ColorEdit3("최소", MinRGB))
					{
						Dist->RGB.MinValue = FVector(MinRGB[0], MinRGB[1], MinRGB[2]);
						bChanged = true;
					}
					if (ImGui::ColorEdit3("최대", MaxRGB))
					{
						Dist->RGB.MaxValue = FVector(MaxRGB[0], MaxRGB[1], MaxRGB[2]);
						bChanged = true;
					}
				}
				break;

			case EDistributionType::ConstantCurve:
				bChanged |= RenderInterpCurveVector("커브##RGB", Dist->RGB.ConstantCurve);
				break;

			case EDistributionType::UniformCurve:
				bChanged |= RenderInterpCurveVector("최소 커브##RGB", Dist->RGB.MinCurve);
				bChanged |= RenderInterpCurveVector("최대 커브##RGB", Dist->RGB.MaxCurve);
				break;

			case EDistributionType::ParticleParameter:
				{
					char Buffer[256];
					strncpy_s(Buffer, Dist->RGB.ParameterName.c_str(), 255);
					if (ImGui::InputText("파라미터명##RGB", Buffer, 256))
					{
						Dist->RGB.ParameterName = Buffer;
						bChanged = true;
					}
				}
				break;
			}
			ImGui::TreePop();
		}

		// Alpha (FDistributionFloat)
		if (ImGui::TreeNode("Alpha"))
		{
			bChanged |= RenderDistributionFloatModeCombo("타입##Alpha", Dist->Alpha.Type);

			switch (Dist->Alpha.Type)
			{
			case EDistributionType::Constant:
				if (ImGui::SliderFloat("값", &Dist->Alpha.ConstantValue, 0.0f, 1.0f))
					bChanged = true;
				break;

			case EDistributionType::Uniform:
				if (ImGui::SliderFloat("최소", &Dist->Alpha.MinValue, 0.0f, 1.0f))
					bChanged = true;
				if (ImGui::SliderFloat("최대", &Dist->Alpha.MaxValue, 0.0f, 1.0f))
					bChanged = true;
				break;

			case EDistributionType::ConstantCurve:
				bChanged |= RenderInterpCurveFloat("커브##Alpha", Dist->Alpha.ConstantCurve);
				break;

			case EDistributionType::UniformCurve:
				bChanged |= RenderInterpCurveFloat("최소 커브##Alpha", Dist->Alpha.MinCurve);
				bChanged |= RenderInterpCurveFloat("최대 커브##Alpha", Dist->Alpha.MaxCurve);
				break;

			case EDistributionType::ParticleParameter:
				{
					char Buffer[256];
					strncpy_s(Buffer, Dist->Alpha.ParameterName.c_str(), 255);
					if (ImGui::InputText("파라미터명##Alpha", Buffer, 256))
					{
						Dist->Alpha.ParameterName = Buffer;
						bChanged = true;
					}
				}
				break;
			}
			ImGui::TreePop();
		}

		ImGui::TreePop();
	}

	return bChanged;
}

bool UPropertyRenderer::RenderBodyInstanceProperty(const FProperty& Prop, void* Instance)
{
	UPrimitiveComponent* Primitive = static_cast<UPrimitiveComponent*>(Instance);
	if (!Primitive || !Primitive->CanSimulatingPhysics()) { return false; }
	
	FBodyInstance* BodyInstance = Prop.GetValuePtr<FBodyInstance>(Instance);
	if (!BodyInstance) { return false; }
	// IsValidBodyInstance() 체크 제거 - 실제 물리 바디가 없어도 프로퍼티 편집 가능
	// SkeletalMeshComponent는 per-bone PhysicsAsset을 사용하므로 컴포넌트 레벨 BodyInstance가 없음

	bool bChanged = false;
	ImGuiStorage* storage = ImGui::GetStateStorage();
	ImGuiID base_id = ImGui::GetID(BodyInstance);
	
	// --- 기본 설정 ---
	ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.2f, 1.0f), "기본 설정");
	ImGui::Separator();
	{
		bool bSimulatePhysics = BodyInstance->IsSimulatePhysics();
		if (ImGui::Checkbox("물리 시뮬레이션##SimPhysics", &bSimulatePhysics))
		{
			BodyInstance->SetSimulatePhysics(bSimulatePhysics);
			bChanged = true;
		}

		bool bEnableGravity = BodyInstance->IsEnabledGravity();
		if (ImGui::Checkbox("중력 활성##EnableGravity", &bEnableGravity))
		{
			BodyInstance->SetEnableGravity(bEnableGravity);
			bChanged = true;
		}

		bool bStartAwake = BodyInstance->IsStartAwake();
		if (ImGui::Checkbox("시작 시 활성##StartAwake", &bStartAwake))
		{
			BodyInstance->SetStartAwake(bStartAwake);
			bChanged = true;
		}

		bool bCollisionEnabled = BodyInstance->GetCollisionEnabled();
		if (ImGui::Checkbox("충돌 활성##CollisionEnabled", &bCollisionEnabled))
		{
			BodyInstance->SetCollisionEnabled(bCollisionEnabled);
			bChanged = true;
		}
	}

	ImGui::Spacing();
	ImGui::Spacing();

	// --- 질량 설정 ---
	ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.2f, 1.0f), "질량");
	ImGui::Separator();
	{
		bool bOverrideMass = BodyInstance->IsOverrideMass();
		if (ImGui::Checkbox("질량 오버라이드##OverrideMass", &bOverrideMass))
		{
			BodyInstance->SetOverrideMass(bOverrideMass);
			bChanged = true;
		}

		ImGui::BeginDisabled(!bOverrideMass);
		{
			float MassInKg = storage->GetFloat(base_id + 1, BodyInstance->GetMassInKg());
			ImGui::DragFloat("질량 (kg)##MassInKg", &MassInKg, 0.1f, 0.1f, 10000.0f, "%.2f");
			storage->SetFloat(base_id + 1, MassInKg);
			
			if (ImGui::IsItemDeactivatedAfterEdit())
			{
				MassInKg = FMath::Max(0.1f, MassInKg);
				BodyInstance->SetMassInKg(MassInKg);
				storage->SetFloat(base_id + 1, MassInKg);
				bChanged = true;
			}
		}
		ImGui::EndDisabled();

		// 현재 질량 정보 표시 (읽기 전용)
		ImGui::BeginDisabled(true);
		{
			float BodyMass = BodyInstance->GetBodyMass();
			ImGui::DragFloat("현재 질량##CurrentMass", &BodyMass, 0.0f, 0.0f, 0.0f, "%.2f");
		}
		ImGui::EndDisabled();
	}

	ImGui::Spacing();
	ImGui::Spacing();

	// --- 감쇠 설정 ---
	ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.2f, 1.0f), "감쇠");
	ImGui::Separator();
	{
		float LinearDamping = storage->GetFloat(base_id + 2, BodyInstance->GetLinearDamping());
		ImGui::DragFloat("선형 감쇠##LinearDamping", &LinearDamping, 0.001f, 0.0f, 1.0f, "%.4f");
		storage->SetFloat(base_id + 2, LinearDamping);
		
		if (ImGui::IsItemDeactivatedAfterEdit())
		{
			LinearDamping = FMath::Clamp(LinearDamping, 0.0f, 1.0f);
			BodyInstance->SetLinearDamping(LinearDamping);
			storage->SetFloat(base_id + 2, LinearDamping);
			bChanged = true;
		}

		float AngularDamping = storage->GetFloat(base_id + 3, BodyInstance->GetAngularDamping());
		ImGui::DragFloat("각속도 감쇠##AngularDamping", &AngularDamping, 0.001f, 0.0f, 1.0f, "%.4f");
		storage->SetFloat(base_id + 3, AngularDamping);
		
		if (ImGui::IsItemDeactivatedAfterEdit())
		{
			AngularDamping = FMath::Clamp(AngularDamping, 0.0f, 1.0f);
			BodyInstance->SetAngularDamping(AngularDamping);
			storage->SetFloat(base_id + 3, AngularDamping);
			bChanged = true;
		}
	}

	ImGui::Spacing();
	ImGui::Spacing();

	// --- 충돌 설정 ---
	ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.2f, 1.0f), "충돌");
	ImGui::Separator();
	{
		bool bIsTrigger = BodyInstance->IsTrigger();
		if (ImGui::Checkbox("트리거##IsTrigger", &bIsTrigger))
		{
			BodyInstance->SetTrigger(bIsTrigger);
			bChanged = true;
		}

		bool bNotifyCollision = BodyInstance->GetNotifyRigidBodyCollision();
		if (ImGui::Checkbox("충돌 이벤트##NotifyCollision", &bNotifyCollision))
		{
			BodyInstance->SetNotifyRigidBodyCollision(bNotifyCollision);
			bChanged = true;
		}

		bool bUseCCD = BodyInstance->GetUseCCD();
		if (ImGui::Checkbox("CCD 사용##UseCCD", &bUseCCD))
		{
			BodyInstance->SetUseCCD(bUseCCD);
			bChanged = true;
		}
	}

	ImGui::Spacing();
	ImGui::Spacing();

	// --- DOF 잠금 설정 ---
	ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.2f, 1.0f), "DOF 잠금");
	ImGui::Separator();
	{
		ImGui::Text("선형 잠금");
		{
			bool bX, bY, bZ;
			BodyInstance->GetLinearLock(bX, bY, bZ);

			if (ImGui::Checkbox("X 잠금##LockXLinear", &bX) | ImGui::Checkbox("Y 잠금##LockYLinear", &bY) | ImGui::Checkbox(
				"Z 잠금##LockZLinear", &bZ))
			{
				BodyInstance->SetLinearLock(bX, bY, bZ);
				bChanged = true;
			}
		}

		ImGui::Spacing();

		ImGui::Text("각속도 잠금");
		{
			bool bX, bY, bZ;
			BodyInstance->GetAngularLock(bX, bY, bZ);

			if (ImGui::Checkbox("X 잠금##LockXAngular", &bX) | ImGui::Checkbox("Y 잠금##LockYAngular", &bY) |
				ImGui::Checkbox("Z 잠금##LockZAngular", &bZ))
			{
				BodyInstance->SetAngularLock(bX, bY, bZ);
				bChanged = true;
			}
		}
	}

	ImGui::Spacing();
	ImGui::Spacing();

	// --- 슬립 설정 ---
	ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.2f, 1.0f), "슬립");
	ImGui::Separator();
	{
		float SleepThreshold = storage->GetFloat(base_id + 4, BodyInstance->GetSleepThresholdMultiplier());
		ImGui::DragFloat("슬립 임계값 배수##SleepThreshold", &SleepThreshold, 0.1f, 0.1f, 10.0f, "%.2f");
		storage->SetFloat(base_id + 4, SleepThreshold);
		
		if (ImGui::IsItemDeactivatedAfterEdit())
		{
			SleepThreshold = FMath::Max(0.1f, SleepThreshold);
			BodyInstance->SetSleepThresholdMultiplier(SleepThreshold);
			storage->SetFloat(base_id + 4, SleepThreshold);
			bChanged = true;
		}

		ImGui::Spacing();

		// 상태 표시 (읽기 전용)
		bool bAwake = BodyInstance->IsAwake();
		ImGui::BeginDisabled(true);
		ImGui::Checkbox("활성 상태##IsAwake", &bAwake);
		ImGui::EndDisabled();
	}
	
	return bChanged;
}