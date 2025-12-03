#include "pch.h"

#include "BodyInstance.h"

FString UObject::GetName()
{
    return ObjectName.ToString();
}

FString UObject::GetComparisonName()
{
    return FString();
}

// ===== 통합 프로퍼티 직렬화 함수 (재귀적) =====
// Instance: 프로퍼티가 속한 객체/구조체의 포인터
// Prop: 직렬화할 프로퍼티 메타데이터
// bIsLoading: true=로드, false=저장
// InOutJson: JSON 객체 (프로퍼티 이름을 키로 사용)
//
// 이 함수는 모든 프로퍼티 타입을 처리하며,
// Struct, Array<Struct>, Map 등 복합 타입에서 재귀적으로 호출됩니다.
static void SerializeProperty(void* Instance, const FProperty& Prop, bool bIsLoading, JSON& InOutJson);

// 구조체 인스턴스 전체 직렬화 (모든 프로퍼티에 대해 SerializeProperty 호출)
static void SerializeStructInstance(void* StructInstance, UStruct* StructType, bool bIsLoading, JSON& StructJson)
{
    for (const FProperty& StructProp : StructType->GetAllProperties())
    {
        SerializeProperty(StructInstance, StructProp, bIsLoading, StructJson);
    }
}

// 통합 프로퍼티 직렬화 구현
static void SerializeProperty(void* Instance, const FProperty& Prop, bool bIsLoading, JSON& InOutJson)
{
    switch (Prop.Type)
    {
    case EPropertyType::Bool:
    {
        bool* Value = Prop.GetValuePtr<bool>(Instance);
        if (bIsLoading)
        {
            bool ReadValue;
            if (FJsonSerializer::ReadBool(InOutJson, Prop.Name, ReadValue))
                *Value = ReadValue;
        }
        else
        {
            InOutJson[Prop.Name] = *Value;
        }
        break;
    }
    case EPropertyType::Int32:
    {
        int32* Value = Prop.GetValuePtr<int32>(Instance);
        if (bIsLoading)
        {
            int32 ReadValue;
            if (FJsonSerializer::ReadInt32(InOutJson, Prop.Name, ReadValue))
                *Value = ReadValue;
        }
        else
        {
            InOutJson[Prop.Name] = *Value;
        }
        break;
    }
    case EPropertyType::Float:
    {
        float* Value = Prop.GetValuePtr<float>(Instance);
        if (bIsLoading)
        {
            float ReadValue;
            if (FJsonSerializer::ReadFloat(InOutJson, Prop.Name, ReadValue))
                *Value = ReadValue;
        }
        else
        {
            InOutJson[Prop.Name] = *Value;
        }
        break;
    }
    case EPropertyType::FVector:
    {
        FVector* Value = Prop.GetValuePtr<FVector>(Instance);
        if (bIsLoading)
        {
            FVector ReadValue;
            if (FJsonSerializer::ReadVector(InOutJson, Prop.Name, ReadValue))
                *Value = ReadValue;
        }
        else
        {
            InOutJson[Prop.Name] = FJsonSerializer::VectorToJson(*Value);
        }
        break;
    }
    case EPropertyType::FVector2D:
    {
        FVector2D* Value = Prop.GetValuePtr<FVector2D>(Instance);
        if (bIsLoading)
        {
            FVector2D ReadValue;
            if (FJsonSerializer::ReadVector2D(InOutJson, Prop.Name, ReadValue))
                *Value = ReadValue;
        }
        else
        {
            InOutJson[Prop.Name] = FJsonSerializer::Vector2DToJson(*Value);
        }
        break;
    }
    case EPropertyType::FLinearColor:
    {
        FLinearColor* Value = Prop.GetValuePtr<FLinearColor>(Instance);
        if (bIsLoading)
        {
            FVector4 ReadValue;
            if (FJsonSerializer::ReadVector4(InOutJson, Prop.Name, ReadValue))
                *Value = FLinearColor(ReadValue);
        }
        else
        {
            InOutJson[Prop.Name] = FJsonSerializer::Vector4ToJson(Value->ToFVector4());
        }
        break;
    }
    case EPropertyType::FString:
    case EPropertyType::ScriptFile:
    {
        FString* Value = Prop.GetValuePtr<FString>(Instance);
        if (bIsLoading)
        {
            FString ReadValue;
            if (FJsonSerializer::ReadString(InOutJson, Prop.Name, ReadValue))
                *Value = ReadValue;
        }
        else
        {
            InOutJson[Prop.Name] = Value->c_str();
        }
        break;
    }
    case EPropertyType::FName:
    {
        FName* Value = Prop.GetValuePtr<FName>(Instance);
        if (bIsLoading)
        {
            FString ReadValue;
            if (FJsonSerializer::ReadString(InOutJson, Prop.Name, ReadValue))
                *Value = FName(ReadValue);
        }
        else
        {
            InOutJson[Prop.Name] = Value->ToString().c_str();
        }
        break;
    }
    case EPropertyType::Enum:
    {
        // Enum은 uint8로 처리 (PropertyRenderer와 일관성 유지)
        uint8* Value = Prop.GetValuePtr<uint8>(Instance);
        if (bIsLoading)
        {
            int32 ReadValue;
            if (FJsonSerializer::ReadInt32(InOutJson, Prop.Name, ReadValue))
                *Value = static_cast<uint8>(ReadValue);
        }
        else
        {
            InOutJson[Prop.Name] = static_cast<int32>(*Value);
        }
        break;
    }
    case EPropertyType::Texture:
    {
        UTexture** Value = Prop.GetValuePtr<UTexture*>(Instance);
        if (bIsLoading)
        {
            FString TexturePath;
            FJsonSerializer::ReadString(InOutJson, Prop.Name, TexturePath);
            if (!TexturePath.empty())
                *Value = UResourceManager::GetInstance().Load<UTexture>(TexturePath);
            else
                *Value = nullptr;
        }
        else
        {
            if (*Value)
                InOutJson[Prop.Name] = (*Value)->GetFilePath().c_str();
            else
                InOutJson[Prop.Name] = "";
        }
        break;
    }
    case EPropertyType::StaticMesh:
    {
        UStaticMesh** Value = Prop.GetValuePtr<UStaticMesh*>(Instance);
        if (bIsLoading)
        {
            FString MeshPath;
            FJsonSerializer::ReadString(InOutJson, Prop.Name, MeshPath);
            if (!MeshPath.empty())
                *Value = UResourceManager::GetInstance().Load<UStaticMesh>(MeshPath);
            else
                *Value = nullptr;
        }
        else
        {
            if (*Value)
                InOutJson[Prop.Name] = (*Value)->GetAssetPathFileName().c_str();
            else
                InOutJson[Prop.Name] = "";
        }
        break;
    }
    case EPropertyType::SkeletalMesh:
    {
        USkeletalMesh** Value = Prop.GetValuePtr<USkeletalMesh*>(Instance);
        if (bIsLoading)
        {
            FString MeshPath;
            FJsonSerializer::ReadString(InOutJson, Prop.Name, MeshPath);
            if (!MeshPath.empty())
                *Value = UResourceManager::GetInstance().Load<USkeletalMesh>(MeshPath);
            else
                *Value = nullptr;
        }
        else
        {
            if (*Value)
                InOutJson[Prop.Name] = (*Value)->GetPathFileName().c_str();
            else
                InOutJson[Prop.Name] = "";
        }
        break;
    }
    case EPropertyType::Material:
    {
        UMaterial** Value = Prop.GetValuePtr<UMaterial*>(Instance);
        if (bIsLoading)
        {
            FString MaterialPath;
            FJsonSerializer::ReadString(InOutJson, Prop.Name, MaterialPath);
            if (!MaterialPath.empty())
                *Value = UResourceManager::GetInstance().Load<UMaterial>(MaterialPath);
            else
                *Value = nullptr;
        }
        else
        {
            if (*Value)
                InOutJson[Prop.Name] = (*Value)->GetFilePath().c_str();
            else
                InOutJson[Prop.Name] = "";
        }
        break;
    }
    case EPropertyType::ParticleSystem:
    {
        UParticleSystem** Value = Prop.GetValuePtr<UParticleSystem*>(Instance);
        if (bIsLoading)
        {
            FString ParticlePath;
            FJsonSerializer::ReadString(InOutJson, Prop.Name, ParticlePath);
            if (!ParticlePath.empty())
                *Value = UResourceManager::GetInstance().Load<UParticleSystem>(ParticlePath);
            else
                *Value = nullptr;
        }
        else
        {
            if (*Value)
                InOutJson[Prop.Name] = (*Value)->GetFilePath().c_str();
            else
                InOutJson[Prop.Name] = "";
        }
        break;
    }
    case EPropertyType::PhysicsAsset:
    {
        UPhysicsAsset** Value = Prop.GetValuePtr<UPhysicsAsset*>(Instance);
        if (bIsLoading)
        {
            FString PhysicsAssetPath;
            FJsonSerializer::ReadString(InOutJson, Prop.Name, PhysicsAssetPath);
            if (!PhysicsAssetPath.empty())
                *Value = UResourceManager::GetInstance().Load<UPhysicsAsset>(PhysicsAssetPath);
            else
                *Value = nullptr;
        }
        else
        {
            if (*Value)
                InOutJson[Prop.Name] = (*Value)->GetFilePath().c_str();
            else
                InOutJson[Prop.Name] = "";
        }
        break;
    }
    case EPropertyType::PhysicalMaterial:
    {
        UPhysicalMaterial** Value = Prop.GetValuePtr<UPhysicalMaterial*>(Instance);
        if (bIsLoading)
        {
            FString PhysicalMaterialPath;
            FJsonSerializer::ReadString(InOutJson, Prop.Name, PhysicalMaterialPath);
            if (!PhysicalMaterialPath.empty())
                *Value = UResourceManager::GetInstance().Load<UPhysicalMaterial>(PhysicalMaterialPath);
            else
                *Value = nullptr;
        }
        else
        {
            if (*Value)
                InOutJson[Prop.Name] = (*Value)->GetFilePath().c_str();
            else
                InOutJson[Prop.Name] = "";
        }
        break;
    }
    case EPropertyType::Sound:
    {
        USound** Value = Prop.GetValuePtr<USound*>(Instance);
        if (bIsLoading)
        {
            FString SoundPath;
            FJsonSerializer::ReadString(InOutJson, Prop.Name, SoundPath);
            if (!SoundPath.empty())
                *Value = UResourceManager::GetInstance().Load<USound>(SoundPath);
            else
                *Value = nullptr;
        }
        else
        {
            if (*Value)
                InOutJson[Prop.Name] = (*Value)->GetFilePath().c_str();
            else
                InOutJson[Prop.Name] = "";
        }
        break;
    }
    case EPropertyType::Curve:
    {
        // Curve 프로퍼티는 float[4] 배열입니다. 따라서 FVector4로 처리
        if (bIsLoading)
        {
            FVector4 TempVec4;
            if (FJsonSerializer::ReadVector4(InOutJson, Prop.Name, TempVec4))
            {
                float* PropData = Prop.GetValuePtr<float>(Instance);
                memcpy(PropData, &TempVec4, sizeof(float) * 4);
            }
        }
        else
        {
            float* PropData = Prop.GetValuePtr<float>(Instance);
            FVector4 TempVec4(PropData[0], PropData[1], PropData[2], PropData[3]);
            InOutJson[Prop.Name] = FJsonSerializer::Vector4ToJson(TempVec4);
        }
        break;
    }
    case EPropertyType::Struct:
    {
        // Struct 직렬화 - 재귀적으로 SerializeStructInstance 호출
        UStruct* StructType = UStruct::FindStruct(Prop.TypeName);
        if (!StructType)
        {
            UE_LOG("[AutoSerialize] Struct property '%s' has unknown type: %s", Prop.Name, Prop.TypeName);
            break;
        }

        void* StructInstance = (char*)Instance + Prop.Offset;
        JSON StructJson;

        if (bIsLoading)
        {
            if (!InOutJson.hasKey(Prop.Name) || InOutJson[Prop.Name].JSONType() != JSON::Class::Object)
                break;
            StructJson = InOutJson[Prop.Name];
        }
        else
        {
            StructJson = JSON::Make(JSON::Class::Object);
        }

        // 재귀 호출: 구조체의 모든 프로퍼티 직렬화
        SerializeStructInstance(StructInstance, StructType, bIsLoading, StructJson);

        if (!bIsLoading)
        {
            InOutJson[Prop.Name] = StructJson;
        }
        break;
    }
    case EPropertyType::Array:
    {
        // Array 직렬화는 InnerType에 따라 처리
        if (Prop.InnerType == EPropertyType::Unknown)
        {
            UE_LOG("[AutoSerialize] Array property '%s' has Unknown InnerType, skipping.", Prop.Name);
            break;
        }

        JSON ArrayJson;
        if (bIsLoading)
        {
            if (!FJsonSerializer::ReadArray(InOutJson, Prop.Name, ArrayJson))
                break;
        }
        else
        {
            ArrayJson = JSON::Make(JSON::Class::Array);
        }

        switch (Prop.InnerType)
        {
        case EPropertyType::Int32:
        {
            TArray<int32>* ArrayPtr = Prop.GetValuePtr<TArray<int32>>(Instance);
            SerializePrimitiveArray<int32>(ArrayPtr, bIsLoading, ArrayJson);
            break;
        }
        case EPropertyType::Float:
        {
            TArray<float>* ArrayPtr = Prop.GetValuePtr<TArray<float>>(Instance);
            SerializePrimitiveArray<float>(ArrayPtr, bIsLoading, ArrayJson);
            break;
        }
        case EPropertyType::Bool:
        {
            TArray<bool>* ArrayPtr = Prop.GetValuePtr<TArray<bool>>(Instance);
            SerializePrimitiveArray<bool>(ArrayPtr, bIsLoading, ArrayJson);
            break;
        }
        case EPropertyType::FString:
        {
            TArray<FString>* ArrayPtr = Prop.GetValuePtr<TArray<FString>>(Instance);
            SerializePrimitiveArray<FString>(ArrayPtr, bIsLoading, ArrayJson);
            break;
        }
        case EPropertyType::Sound:
        {
            TArray<USound*>* ArrayPtr = Prop.GetValuePtr<TArray<USound*>>(Instance);
            if (bIsLoading)
            {
                ArrayPtr->Empty();
                for (size_t i = 0; i < ArrayJson.size(); ++i)
                {
                    const JSON& Elem = ArrayJson.at(i);
                    if (Elem.JSONType() == JSON::Class::String)
                    {
                        FString Path = Elem.ToString();
                        if (!Path.empty())
                            ArrayPtr->Add(UResourceManager::GetInstance().Load<USound>(Path));
                        else
                            ArrayPtr->Add(nullptr);
                    }
                }
            }
            else
            {
                for (USound* Snd : *ArrayPtr)
                {
                    ArrayJson.append((Snd) ? Snd->GetFilePath().c_str() : "");
                }
            }
            break;
        }
        case EPropertyType::Struct:
        {
            // TArray<Struct> 직렬화 - 재귀적으로 SerializeStructInstance 사용
            UStruct* StructType = UStruct::FindStruct(Prop.TypeName);
            if (!StructType)
            {
                UE_LOG("[AutoSerialize] Array<Struct> property '%s' has unknown struct type: %s", Prop.Name, Prop.TypeName);
                break;
            }

            if (!StructType->ArrayNum || !StructType->ArrayGetData || !StructType->ArrayAdd || !StructType->ArrayClear)
            {
                UE_LOG("[AutoSerialize] Array<Struct> property '%s': Struct '%s' missing array manipulation functions", Prop.Name, Prop.TypeName);
                break;
            }

            void* ArrayPtr = (char*)Instance + Prop.Offset;
            SIZE_T ElementSize = StructType->Size;

            if (bIsLoading)
            {
                StructType->ArrayClear(ArrayPtr);
                for (size_t i = 0; i < ArrayJson.size(); ++i)
                {
                    JSON& ElemJson = ArrayJson.at(i);
                    if (ElemJson.JSONType() != JSON::Class::Object)
                        continue;

                    StructType->ArrayAdd(ArrayPtr);
                    int32 NewIndex = StructType->ArrayNum(ArrayPtr) - 1;
                    void* ArrayData = StructType->ArrayGetData(ArrayPtr);
                    void* ElementPtr = (char*)ArrayData + (NewIndex * ElementSize);

                    // 재귀 호출: 구조체 요소 직렬화
                    SerializeStructInstance(ElementPtr, StructType, true, ElemJson);
                }
            }
            else
            {
                int32 ArrayNum = StructType->ArrayNum(ArrayPtr);
                void* ArrayData = StructType->ArrayGetData(ArrayPtr);

                for (int32 i = 0; i < ArrayNum; ++i)
                {
                    void* ElementPtr = (char*)ArrayData + (i * ElementSize);
                    JSON ElemJson = JSON::Make(JSON::Class::Object);
                    // 재귀 호출: 구조체 요소 직렬화
                    SerializeStructInstance(ElementPtr, StructType, false, ElemJson);
                    ArrayJson.append(ElemJson);
                }
            }
            break;
        }
        default:
            break;
        }

        if (!bIsLoading)
        {
            InOutJson[Prop.Name] = ArrayJson;
        }
        break;
    }
    case EPropertyType::Map:
    {
        // Map 직렬화는 KeyType과 InnerType에 따라 처리
        if (Prop.KeyType == EPropertyType::Unknown || Prop.InnerType == EPropertyType::Unknown)
        {
            UE_LOG("[AutoSerialize] Map property '%s' has Unknown KeyType or InnerType, skipping.", Prop.Name);
            break;
        }

        JSON MapJson;
        if (bIsLoading)
        {
            if (!InOutJson.hasKey(Prop.Name) || InOutJson[Prop.Name].JSONType() != JSON::Class::Object)
                break;
            MapJson = InOutJson[Prop.Name];
        }
        else
        {
            MapJson = JSON::Make(JSON::Class::Object);
        }

        // FString 키 Map 처리
        if (Prop.KeyType == EPropertyType::FString)
        {
            switch (Prop.InnerType)
            {
            case EPropertyType::Int32:
            {
                TMap<FString, int32>* MapPtr = Prop.GetValuePtr<TMap<FString, int32>>(Instance);
                if (bIsLoading)
                {
                    MapPtr->clear();
                    for (auto& [key, value] : MapJson.ObjectRange())
                    {
                        if (value.JSONType() == JSON::Class::Integral)
                            (*MapPtr)[key] = static_cast<int32>(value.ToInt());
                    }
                }
                else
                {
                    for (auto& [key, value] : *MapPtr)
                        MapJson[key] = value;
                }
                break;
            }
            case EPropertyType::Float:
            {
                TMap<FString, float>* MapPtr = Prop.GetValuePtr<TMap<FString, float>>(Instance);
                if (bIsLoading)
                {
                    MapPtr->clear();
                    for (auto& [key, value] : MapJson.ObjectRange())
                    {
                        if (value.JSONType() == JSON::Class::Floating)
                            (*MapPtr)[key] = static_cast<float>(value.ToFloat());
                        else if (value.JSONType() == JSON::Class::Integral)
                            (*MapPtr)[key] = static_cast<float>(value.ToInt());
                    }
                }
                else
                {
                    for (auto& [key, value] : *MapPtr)
                        MapJson[key] = value;
                }
                break;
            }
            case EPropertyType::FString:
            {
                TMap<FString, FString>* MapPtr = Prop.GetValuePtr<TMap<FString, FString>>(Instance);
                if (bIsLoading)
                {
                    MapPtr->clear();
                    for (auto& [key, value] : MapJson.ObjectRange())
                    {
                        if (value.JSONType() == JSON::Class::String)
                            (*MapPtr)[key] = value.ToString();
                    }
                }
                else
                {
                    for (auto& [key, value] : *MapPtr)
                        MapJson[key] = value.c_str();
                }
                break;
            }
            case EPropertyType::Bool:
            {
                TMap<FString, bool>* MapPtr = Prop.GetValuePtr<TMap<FString, bool>>(Instance);
                if (bIsLoading)
                {
                    MapPtr->clear();
                    for (auto& [key, value] : MapJson.ObjectRange())
                    {
                        if (value.JSONType() == JSON::Class::Boolean)
                            (*MapPtr)[key] = value.ToBool();
                    }
                }
                else
                {
                    for (auto& [key, value] : *MapPtr)
                        MapJson[key] = value;
                }
                break;
            }
            default:
                UE_LOG("[AutoSerialize] Map property '%s' has unsupported InnerType: %d", Prop.Name, static_cast<int>(Prop.InnerType));
                break;
            }
        }
        // int32 키 Map 처리
        else if (Prop.KeyType == EPropertyType::Int32)
        {
            switch (Prop.InnerType)
            {
            case EPropertyType::FString:
            {
                TMap<int32, FString>* MapPtr = Prop.GetValuePtr<TMap<int32, FString>>(Instance);
                if (bIsLoading)
                {
                    MapPtr->clear();
                    for (auto& [key, value] : MapJson.ObjectRange())
                    {
                        if (value.JSONType() == JSON::Class::String)
                            (*MapPtr)[std::stoi(key)] = value.ToString();
                    }
                }
                else
                {
                    for (auto& [key, value] : *MapPtr)
                        MapJson[std::to_string(key)] = value.c_str();
                }
                break;
            }
            default:
                break;
            }
        }

        if (!bIsLoading)
        {
            InOutJson[Prop.Name] = MapJson;
        }
        break;
    }
    case EPropertyType::BodyInstance:
    {
        FBodyInstance* BodyInstance = Prop.GetValuePtr<FBodyInstance>(Instance);
        if (bIsLoading)
        {
            if (!InOutJson.hasKey(Prop.Name)) break;
            
            auto& BodyJson = InOutJson[Prop.Name];
            
            // Simulate Physics
            bool bSimulatePhysics = true;
            if (FJsonSerializer::ReadBool(BodyJson, "bSimulatePhysics", bSimulatePhysics))
                BodyInstance->SetSimulatePhysics(bSimulatePhysics);
            
            // Enable Gravity
            bool bEnableGravity = true;
            if (FJsonSerializer::ReadBool(BodyJson, "bEnableGravity", bEnableGravity))
                BodyInstance->SetEnableGravity(bEnableGravity);
            
            // Start Awake
            bool bStartAwake = true;
            if (FJsonSerializer::ReadBool(BodyJson, "bStartAwake", bStartAwake))
                BodyInstance->SetStartAwake(bStartAwake);
            
            // Collision Enabled
            bool bCollisionEnabled = true;
            if (FJsonSerializer::ReadBool(BodyJson, "bCollisionEnabled", bCollisionEnabled))
                BodyInstance->SetCollisionEnabled(bCollisionEnabled);
            
            // Override Mass
            bool bOverrideMass = false;
            if (FJsonSerializer::ReadBool(BodyJson, "bOverrideMass", bOverrideMass))
                BodyInstance->SetOverrideMass(bOverrideMass);
            
            // Mass In Kg
            float MassInKg = 10.0f;
            if (FJsonSerializer::ReadFloat(BodyJson, "MassInKg", MassInKg))
                BodyInstance->SetMassInKg(MassInKg);
            
            // Linear Damping
            float LinearDamping = 0.01f;
            if (FJsonSerializer::ReadFloat(BodyJson, "LinearDamping", LinearDamping))
                BodyInstance->SetLinearDamping(LinearDamping);
            
            // Angular Damping
            float AngularDamping = 0.05f;
            if (FJsonSerializer::ReadFloat(BodyJson, "AngularDamping", AngularDamping))
                BodyInstance->SetAngularDamping(AngularDamping);
            
            // Is Trigger
            bool bIsTrigger = false;
            if (FJsonSerializer::ReadBool(BodyJson, "bIsTrigger", bIsTrigger))
                BodyInstance->SetTrigger(bIsTrigger);
            
            // Notify Rigid Body Collision
            bool bNotifyRigidBodyCollision = true;
            if (FJsonSerializer::ReadBool(BodyJson, "bNotifyRigidBodyCollision", bNotifyRigidBodyCollision))
                BodyInstance->SetNotifyRigidBodyCollision(bNotifyRigidBodyCollision);
            
            // Use CCD
            bool bUseCCD = false;
            if (FJsonSerializer::ReadBool(BodyJson, "bUseCCD", bUseCCD))
                BodyInstance->SetUseCCD(bUseCCD);
            
            // Linear Lock
            bool bLockXLinear = false, bLockYLinear = false, bLockZLinear = false;
            FJsonSerializer::ReadBool(BodyJson, "bLockXLinear", bLockXLinear);
            FJsonSerializer::ReadBool(BodyJson, "bLockYLinear", bLockYLinear);
            FJsonSerializer::ReadBool(BodyJson, "bLockZLinear", bLockZLinear);
            BodyInstance->SetLinearLock(bLockXLinear, bLockYLinear, bLockZLinear);
            
            // Angular Lock
            bool bLockXAngular = false, bLockYAngular = false, bLockZAngular = false;
            FJsonSerializer::ReadBool(BodyJson, "bLockXAngular", bLockXAngular);
            FJsonSerializer::ReadBool(BodyJson, "bLockYAngular", bLockYAngular);
            FJsonSerializer::ReadBool(BodyJson, "bLockZAngular", bLockZAngular);
            BodyInstance->SetAngularLock(bLockXAngular, bLockYAngular, bLockZAngular);
            
            // Sleep Threshold Multiplier
            float SleepThresholdMultiplier = 1.0f;
            if (FJsonSerializer::ReadFloat(BodyJson, "SleepThresholdMultiplier", SleepThresholdMultiplier))
                BodyInstance->SetSleepThresholdMultiplier(SleepThresholdMultiplier);
            
            FString PhysicalMaterialPath;
            if (FJsonSerializer::ReadString(BodyJson, "PhysicalMaterial", PhysicalMaterialPath))
                BodyInstance->PhysMaterialOverride = UResourceManager::GetInstance().Load<UPhysicalMaterial>(PhysicalMaterialPath);
        }
        else
        {
            JSON BodyJson;
            
            // Simulate Physics
            BodyJson["bSimulatePhysics"] = BodyInstance->IsSimulatePhysics();
            
            // Enable Gravity
            BodyJson["bEnableGravity"] = BodyInstance->IsEnabledGravity();
            
            // Start Awake
            BodyJson["bStartAwake"] = BodyInstance->IsStartAwake();
            
            // Collision Enabled
            BodyJson["bCollisionEnabled"] = BodyInstance->GetCollisionEnabled();
            
            // Override Mass
            BodyJson["bOverrideMass"] = BodyInstance->IsOverrideMass();
            
            // Mass In Kg
            BodyJson["MassInKg"] = BodyInstance->GetMassInKg();
            
            // Linear Damping
            BodyJson["LinearDamping"] = BodyInstance->GetLinearDamping();
            
            // Angular Damping
            BodyJson["AngularDamping"] = BodyInstance->GetAngularDamping();
            
            // Is Trigger
            BodyJson["bIsTrigger"] = BodyInstance->IsTrigger();
            
            // Notify Rigid Body Collision
            BodyJson["bNotifyRigidBodyCollision"] = BodyInstance->GetNotifyRigidBodyCollision();
            
            // Use CCD
            BodyJson["bUseCCD"] = BodyInstance->GetUseCCD();
            
            // Linear Lock
            bool bX, bY, bZ;
            BodyInstance->GetLinearLock(bX, bY, bZ);
            BodyJson["bLockXLinear"] = bX;
            BodyJson["bLockYLinear"] = bY;
            BodyJson["bLockZLinear"] = bZ;
            
            // Angular Lock
            BodyInstance->GetAngularLock(bX, bY, bZ);
            BodyJson["bLockXAngular"] = bX;
            BodyJson["bLockYAngular"] = bY;
            BodyJson["bLockZAngular"] = bZ;
            
            // Sleep Threshold Multiplier
            BodyJson["SleepThresholdMultiplier"] = BodyInstance->GetSleepThresholdMultiplier();

            if (BodyInstance->PhysMaterialOverride)
            {
                BodyJson["PhysicalMaterial"] = BodyInstance->PhysMaterialOverride->GetFilePath();
            }
            InOutJson[Prop.Name] = BodyJson;
        }
        break;
    }
    case EPropertyType::ObjectPtr:
    {
        // 일반 UObject* 포인터는 현재 지원하지 않음 (참조 기반이라 복잡)
        break;
    }
    default:
        // 알 수 없는 타입은 무시
        break;
    }
}

// 리플렉션 기반 자동 직렬화 (현재 클래스의 프로퍼티만 처리)
// 통합 SerializeProperty 함수를 사용하여 모든 타입을 일관되게 처리합니다.
void UObject::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	const TArray<FProperty>& Properties = this->GetClass()->GetAllProperties();

	for (const FProperty& Prop : Properties)
	{
		// 통합 직렬화 함수 호출 - 재귀적으로 Struct, Array<Struct> 등 모두 처리
		SerializeProperty(this, Prop, bInIsLoading, InOutHandle);
	}
}

void UObject::OnPropertyChanged(const FProperty& Prop)
{
}

void UObject::DuplicateSubObjects()
{
    UUID = GenerateUUID(); // UUID는 고유값이므로 새로 생성
}

UObject* UObject::Duplicate() const
{
    UObject* NewObject = ObjectFactory::DuplicateObject<UObject>(this); // 모든 멤버 얕은 복사

    NewObject->DuplicateSubObjects(); // 얕은 복사한 멤버들에 대해 메뉴얼하게 깊은 복사 수행

	NewObject->PostDuplicate();
    return NewObject;
}





