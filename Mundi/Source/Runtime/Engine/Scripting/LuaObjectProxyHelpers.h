#pragma once

// All necessary types should be included via pch.h
// Forward declarations for types that may not be in pch.h
class UTexture;
class UStaticMesh;
class USkeletalMesh;
class UMaterialInterface;
class USound;

// Forward declarations
extern TArray<UObject*> GUObjectArray;

// Map EPropertyType to expected UClass* for UObject pointer types
// NOTE: Update this map when new UObject-derived types are added to EPropertyType enum
inline const TMap<EPropertyType, UClass*>& GetObjectPointerTypeMap()
{
    static const TMap<EPropertyType, UClass*> ObjectPointerTypeMap = {
        {EPropertyType::ObjectPtr,             UObject::StaticClass()},
        {EPropertyType::Texture,               UTexture::StaticClass()},
        {EPropertyType::StaticMesh,            UStaticMesh::StaticClass()},
        {EPropertyType::SkeletalMesh,          USkeletalMesh::StaticClass()},
        {EPropertyType::Material,              UMaterialInterface::StaticClass()},
        {EPropertyType::PhysicalMaterial,      UPhysicalMaterial::StaticClass()},
        {EPropertyType::Sound,                 USound::StaticClass()}
    };
    return ObjectPointerTypeMap;
}

// Validate UObject pointer using GUObjectArray
inline bool IsValidUObject(UObject* Ptr)
{
    if (!Ptr) return false;

    // Step 1: Check InternalIndex range
    uint32_t idx = Ptr->InternalIndex;
    if (idx >= static_cast<uint32_t>(GUObjectArray.Num()))
        return false;

    // Step 2: Verify GUObjectArray slot points to the same object
    UObject* RegisteredObj = GUObjectArray[idx];
    if (RegisteredObj != Ptr)
        return false;  // Deleted or different object

    // Step 3: UUID validation (commented for now)
    // TODO(SlotReuse): If ObjectFactory enables null slot reuse, uncomment below
    // to prevent ABA problem (same address, different object):
    // if (RegisteredObj->UUID != Ptr->UUID)
    //     return false;

    return true;
}

// Check if EPropertyType represents a UObject pointer
inline bool IsObjectPointerType(EPropertyType Type)
{
    return GetObjectPointerTypeMap().count(Type) > 0;
}

// Get expected UClass* for property type (for type validation)
inline UClass* GetExpectedClassForPropertyType(EPropertyType Type)
{
    auto it = GetObjectPointerTypeMap().find(Type);
    return (it != GetObjectPointerTypeMap().end()) ? it->second : nullptr;
}
