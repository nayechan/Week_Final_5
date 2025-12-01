#include "pch.h"
#include "PhysicalMaterial.h"
#include "PhysXSupport.h"

UPhysicalMaterial::UPhysicalMaterial()
    : PxMat(nullptr)
{
    Friction                        = 0.7f;
    StaticFriction                  = 0.7f;
    Restitution                     = 0.3f;
    Density                         = 1000.0f;

    FrictionCombineMode             = EFrictionCombineMode::Average;
    RestitutionCombineMode          = EFrictionCombineMode::Average;

    bOverrideFrictionCombineMode    = false;
    bOverrideRestitutionCombineMode = false;

    SleepLinearVelocityThreshold    = -1.0f;
}

UPhysicalMaterial::~UPhysicalMaterial()
{
    if (PxMat)
    {
        PxMat->release();
        PxMat = nullptr;
    }
}

PxMaterial* UPhysicalMaterial::GetPxMaterial()
{
    if (PxMat)
    {
        return PxMat;
    }

    if (GPhysXSDK)
    {
        PxMat = GPhysXSDK->createMaterial(StaticFriction, Friction, Restitution);

        if (PxMat)
        {
            PxMat->userData = this;

            if (bOverrideRestitutionCombineMode)
            {
                PxMat->setFrictionCombineMode((PxCombineMode::Enum)FrictionCombineMode);
            }
            if (bOverrideRestitutionCombineMode)
            {
                PxMat->setRestitutionCombineMode((PxCombineMode::Enum)RestitutionCombineMode);
            }
        }
    }

    return PxMat;
}

void UPhysicalMaterial::UpdatePhysicsMaterial()
{
    if (PxMat)
    {
        PxMat->setStaticFriction(StaticFriction);
        PxMat->setDynamicFriction(Friction);
        PxMat->setRestitution(Restitution);

        if (bOverrideFrictionCombineMode)
        {
            PxMat->setFrictionCombineMode((PxCombineMode::Enum)FrictionCombineMode);
        }
        
        if (bOverrideRestitutionCombineMode)
        {
            PxMat->setRestitutionCombineMode((PxCombineMode::Enum)RestitutionCombineMode);
        }
    }
}

void UPhysicalMaterial::Load(const FString& InFilePath, ID3D11Device* InDevice)
{
    FWideString WidePath(InFilePath.begin(), InFilePath.end());

    JSON JsonHandle;
    if (!FJsonSerializer::LoadJsonFromFile(JsonHandle, WidePath))
    {
        UE_LOG("[UPhysicalMaterial] Load 실패: %s", InFilePath.c_str());
        return;
    }

    Serialize(true, JsonHandle);
}

void UPhysicalMaterial::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
    UResourceBase::Serialize(bInIsLoading, InOutHandle);

    if (bInIsLoading)
    {
        FJsonSerializer::ReadFloat(InOutHandle, "Friction", Friction, 0.7f, false);
        FJsonSerializer::ReadFloat(InOutHandle, "StaticFriction", StaticFriction, 0.7f, false);
        
        int32 FMode = 0;
        FJsonSerializer::ReadInt32(InOutHandle, "FrictionCombineMode", FMode, 0, false);
        FrictionCombineMode = (EFrictionCombineMode)FMode;
        
        FJsonSerializer::ReadBool(InOutHandle, "bOverrideFrictionCombineMode", bOverrideFrictionCombineMode, false, false);

        FJsonSerializer::ReadFloat(InOutHandle, "Restitution", Restitution, 0.3f, false);
        
        int32 RMode = 0;
        FJsonSerializer::ReadInt32(InOutHandle, "RestitutionCombineMode", RMode, 0, false);
        RestitutionCombineMode = (EFrictionCombineMode)RMode;

        FJsonSerializer::ReadBool(InOutHandle, "bOverrideRestitutionCombineMode", bOverrideRestitutionCombineMode, false, false);

        FJsonSerializer::ReadFloat(InOutHandle, "Density", Density, 1000.0f, false);
        FJsonSerializer::ReadFloat(InOutHandle, "SleepLinearVelocityThreshold", SleepLinearVelocityThreshold, -1.0f, false);

        if (PxMat)
        {
            UpdatePhysicsMaterial();
        }
    }
    else
    {
        InOutHandle["Friction"] = Friction;
        InOutHandle["StaticFriction"] = StaticFriction;
        InOutHandle["FrictionCombineMode"] = (int32)FrictionCombineMode;
        InOutHandle["bOverrideFrictionCombineMode"] = bOverrideFrictionCombineMode;

        InOutHandle["Restitution"] = Restitution;
        InOutHandle["RestitutionCombineMode"] = (int32)RestitutionCombineMode;
        InOutHandle["bOverrideRestitutionCombineMode"] = bOverrideRestitutionCombineMode;

        InOutHandle["Density"] = Density;
        InOutHandle["SleepLinearVelocityThreshold"] = SleepLinearVelocityThreshold;
    }
}

void UPhysicalMaterial::DuplicateSubObjects()
{
    UResourceBase::DuplicateSubObjects();

    PxMat = nullptr;
}

void UPhysicalMaterial::OnPropertyChanged(const FProperty& Prop)
{
    UResourceBase::OnPropertyChanged(Prop);

    UpdatePhysicsMaterial(); 
}
