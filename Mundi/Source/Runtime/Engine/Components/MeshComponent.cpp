#include "pch.h"
#include "MeshComponent.h"
#include "LuaBindHelpers.h"
#include "ObjManager.h"
#include "Material.h"
#include "ResourceManager.h"
#include "WorldPartitionManager.h"

UMeshComponent::UMeshComponent() = default;

UMeshComponent::~UMeshComponent()
{
	ClearDynamicMaterials();
}

void UMeshComponent::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	Super::Serialize(bInIsLoading, InOutHandle);
	const FString MaterialSlotsKey = "MaterialSlots";

    if (bInIsLoading) // --- 로드 ---
    {
       // 1. 로드 전 기존 동적 인스턴스 모두 정리
       ClearDynamicMaterials();

       JSON SlotsArrayJson;
       if (FJsonSerializer::ReadArray(InOutHandle, MaterialSlotsKey, SlotsArrayJson, JSON::Make(JSON::Class::Array), false))
       {
          MaterialSlots.resize(SlotsArrayJson.size());

          for (int i = 0; i < SlotsArrayJson.size(); ++i)
          {
             JSON& SlotJson = SlotsArrayJson.at(i);
             if (SlotJson.IsNull())
             {
                MaterialSlots[i] = nullptr;
                continue;
             }

             // 2. JSON에서 클래스 이름 읽기
             FString ClassName;
             FJsonSerializer::ReadString(SlotJson, "Type", ClassName, "None", false);

             UMaterialInterface* LoadedMaterial = nullptr;

             // 3. 클래스 이름에 따라 분기
             if (ClassName == UMaterialInstanceDynamic::StaticClass()->Name)
             {
                // UMID는 인스턴스이므로, 'new'로 생성합니다.
                UMaterialInstanceDynamic* NewMID = new UMaterialInstanceDynamic();

                // 4. 생성된 빈 객체에 Serialize(true)를 호출하여 데이터를 채웁니다.
                NewMID->Serialize(true, SlotJson); // (const_cast가 필요할 수 있음)

                // 5. 소유권 추적 배열에 추가합니다.
                DynamicMaterialInstances.Add(NewMID);
                LoadedMaterial = NewMID;
             }
             else // if(ClassName == UMaterial::StaticClass()->Name 또는 그 외)
             {
                // UMaterial은 리소스이므로, AssetPath로 리소스 매니저에서 로드합니다.
                FString AssetPath;
                FJsonSerializer::ReadString(SlotJson, "AssetPath", AssetPath, "", false);
                if (!AssetPath.empty())
                {
                   // UMaterialInterface 타입으로 로드 시도
                   LoadedMaterial = UResourceManager::GetInstance().Load<UMaterial>(AssetPath);
                }
                else
                {
                   LoadedMaterial = nullptr;
                }
             }

             MaterialSlots[i] = LoadedMaterial;
          }
       }
    }
    else // --- 저장 ---
    {
       JSON SlotsArrayJson = JSON::Make(JSON::Class::Array);
       for (UMaterialInterface* Mtl : MaterialSlots)
       {
          JSON SlotJson = JSON::Make(JSON::Class::Object);

          if (Mtl == nullptr)
          {
             SlotJson["Type"] = "None"; // null 슬롯 표시
          }
          else
          {
             // 1. 클래스 이름 저장 (로드 시 팩토리 구분을 위함)
             SlotJson["Type"] = Mtl->GetClass()->Name;

             // 2. 객체 스스로 데이터를 저장하도록 위임
             Mtl->Serialize(false, SlotJson);
          }

          SlotsArrayJson.append(SlotJson);
       }
       InOutHandle[MaterialSlotsKey] = SlotsArrayJson;
    }
}

void UMeshComponent::DuplicateSubObjects()
{
    Super::DuplicateSubObjects();
	
	// 이 함수는 '복사본' (PIE 컴포넌트)에서 실행됩니다.
	// 현재 'DynamicMaterialInstances'와 'MaterialSlots'는 
	// '원본' (에디터 컴포넌트)의 포인터를 얕은 복사한 상태입니다.

	// 원본 MID -> 복사본 MID 매핑 테이블
	TMap<UMaterialInstanceDynamic*, UMaterialInstanceDynamic*> OldToNewMIDMap;

	// 1. 복사본의 MID 소유권 리스트를 비웁니다. (메모리 해제 아님)
	//    이 리스트는 새로운 '복사본 MID'들로 다시 채워질 것입니다.
	DynamicMaterialInstances.Empty();

	// 2. MaterialSlots를 순회하며 MID를 찾습니다.
	for (int32 i = 0; i < MaterialSlots.Num(); ++i)
	{
		UMaterialInterface* CurrentSlot = MaterialSlots[i];
		UMaterialInstanceDynamic* OldMID = Cast<UMaterialInstanceDynamic>(CurrentSlot);

		if (OldMID)
		{
			UMaterialInstanceDynamic* NewMID = nullptr;

			// 이 MID를 이미 복제했는지 확인합니다 (여러 슬롯이 같은 MID를 쓸 경우)
			if (OldToNewMIDMap.Contains(OldMID))
			{
				NewMID = OldToNewMIDMap[OldMID];
			}
			else
			{
				// 3. MID를 복제합니다.
				UMaterialInterface* Parent = OldMID->GetParentMaterial();
				if (!Parent)
				{
					// 부모가 없으면 복제할 수 없으므로 nullptr 처리
					MaterialSlots[i] = nullptr;
					continue;
				}

				// 3-1. 새로운 MID (PIE용)를 생성합니다.
				NewMID = UMaterialInstanceDynamic::Create(Parent);

				// 3-2. 원본(OldMID)의 파라미터를 새 MID로 복사합니다.
				NewMID->CopyParametersFrom(OldMID);

				// 3-3. 이 컴포넌트(복사본)의 소유권 리스트에 새 MID를 추가합니다.
				DynamicMaterialInstances.Add(NewMID);
				OldToNewMIDMap.Add(OldMID, NewMID);
			}

			// 4. MaterialSlots가 원본(OldMID) 대신 새 복사본(NewMID)을 가리키도록 교체합니다.
			MaterialSlots[i] = NewMID;
		}
		// else (원본 UMaterial 애셋인 경우)
		// 얕은 복사된 포인터(애셋 경로)를 그대로 사용해도 안전합니다.
	}
}

void UMeshComponent::MarkWorldPartitionDirty()
{
	if (UWorld* World = GetWorld())
	{
		if (UWorldPartitionManager* Partition = World->GetPartitionManager())
		{
			Partition->MarkDirty(this);
		}
	}
}

UMaterialInterface* UMeshComponent::GetMaterial(uint32 InSectionIndex) const
{
	if (MaterialSlots.size() <= InSectionIndex)
	{
		return nullptr;
	}

	UMaterialInterface* FoundMaterial = MaterialSlots[InSectionIndex];

	if (!FoundMaterial)
	{
		// UE_LOG("GetMaterial: Failed to find material Section %d", InSectionIndex);
		return nullptr;
	}

	return FoundMaterial;
}

void UMeshComponent::SetMaterial(uint32 InElementIndex, UMaterialInterface* InNewMaterial)
{
	if (InElementIndex >= static_cast<uint32>(MaterialSlots.Num()))
	{
		UE_LOG("out of range InMaterialSlotIndex: %d", InElementIndex);
		return;
	}

	// 1. 현재 슬롯에 할당된 머티리얼을 가져옵니다.
	UMaterialInterface* OldMaterial = MaterialSlots[InElementIndex];

	// 2. 교체될 새 머티리얼이 현재 머티리얼과 동일하면 아무것도 하지 않습니다.
	if (OldMaterial == InNewMaterial)
	{
		return;
	}

	// 3. 기존 머티리얼이 이 컴포넌트가 소유한 MID인지 확인합니다.
	if (OldMaterial != nullptr)
	{
		UMaterialInstanceDynamic* OldMID = Cast<UMaterialInstanceDynamic>(OldMaterial);
		if (OldMID)
		{
			// 4. 소유권 리스트(DynamicMaterialInstances)에서 이 MID를 찾아 제거합니다.
			// TArray::Remove()는 첫 번째로 발견된 항목만 제거합니다.
			int32 RemovedCount = DynamicMaterialInstances.Remove(OldMID);

			if (RemovedCount > 0)
			{
				// 5. 소유권 리스트에서 제거된 것이 확인되면, 메모리에서 삭제합니다.
				delete OldMID;
			}
			else
			{
				// 경고: MaterialSlots에는 MID가 있었으나, 소유권 리스트에 없는 경우입니다.
				// 이는 DuplicateSubObjects 등이 잘못 구현되었을 때 발생할 수 있습니다.
				UE_LOG("Warning: SetMaterial is replacing a MID that was not tracked by DynamicMaterialInstances.");
				// 이 경우 delete를 호출하면 다른 객체가 소유한 메모리를 해제할 수 있으므로
				// delete를 호출하지 않는 것이 더 안전할 수 있습니다. (혹은 delete 호출 후 크래시로 버그를 잡습니다.)
				// 여기서는 삭제를 시도합니다.
				delete OldMID;
			}
		}
	}

	// 6. 새 머티리얼을 슬롯에 할당합니다.
	MaterialSlots[InElementIndex] = InNewMaterial;
}

UMaterialInstanceDynamic* UMeshComponent::CreateAndSetMaterialInstanceDynamic(uint32 ElementIndex)
{
	UMaterialInterface* CurrentMaterial = GetMaterial(ElementIndex);
	if (!CurrentMaterial)
	{
		return nullptr;
	}

	// 이미 MID인 경우, 그대로 반환
	if (UMaterialInstanceDynamic* ExistingMID = Cast<UMaterialInstanceDynamic>(CurrentMaterial))
	{
		return ExistingMID;
	}

	// 현재 머티리얼(UMaterial 또는 다른 MID가 아닌 UMaterialInterface)을 부모로 하는 새로운 MID를 생성
	UMaterialInstanceDynamic* NewMID = UMaterialInstanceDynamic::Create(CurrentMaterial);
	if (NewMID)
	{
		DynamicMaterialInstances.Add(NewMID); // 소멸자에서 해제하기 위해 추적
		SetMaterial(ElementIndex, NewMID);    // 슬롯에 새로 만든 MID 설정
		NewMID->SetFilePath("(Instance) " + CurrentMaterial->GetFilePath());
		return NewMID;
	}

	return nullptr;
}

// 자동 인스턴싱 머티리얼 생성
void UMeshComponent::SetMaterialTextureByUser(const uint32 InMaterialSlotIndex, EMaterialTextureSlot Slot, UTexture* Texture)
{
	UMaterialInterface* CurrentMaterial = GetMaterial(InMaterialSlotIndex);
	UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(CurrentMaterial);
	if (MID == nullptr)
	{
		MID = CreateAndSetMaterialInstanceDynamic(InMaterialSlotIndex);
	}
	MID->SetTextureParameterValue(Slot, Texture);
}

void UMeshComponent::SetMaterialColorByUser(const uint32 InMaterialSlotIndex, const FString& ParameterName, const FLinearColor& Value)
{
	UMaterialInterface* CurrentMaterial = GetMaterial(InMaterialSlotIndex);
	UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(CurrentMaterial);
	if (MID == nullptr)
	{
		MID = CreateAndSetMaterialInstanceDynamic(InMaterialSlotIndex);
	}
	MID->SetColorParameterValue(ParameterName, Value);
}

void UMeshComponent::SetMaterialScalarByUser(const uint32 InMaterialSlotIndex, const FString& ParameterName, float Value)
{
	UMaterialInterface* CurrentMaterial = GetMaterial(InMaterialSlotIndex);
	UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(CurrentMaterial);
	if (MID == nullptr)
	{
		MID = CreateAndSetMaterialInstanceDynamic(InMaterialSlotIndex);
	}
	MID->SetScalarParameterValue(ParameterName, Value);
}

void UMeshComponent::ClearDynamicMaterials()
{
	// 1. 생성된 동적 머티리얼 인스턴스 해제
	for (UMaterialInstanceDynamic* MID : DynamicMaterialInstances)
	{
		delete MID;
	}	
	DynamicMaterialInstances.Empty();

	// 2. 머티리얼 슬롯 배열도 비웁니다.
	// (이 배열이 MID 포인터를 가리키고 있었을 수 있으므로
	//  delete 이후에 비워야 안전합니다.)
	MaterialSlots.Empty();
}
