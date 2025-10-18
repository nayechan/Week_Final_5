#include "pch.h"
#include "PrimitiveComponent.h"
#include "SceneComponent.h"

IMPLEMENT_CLASS(UPrimitiveComponent)

void UPrimitiveComponent::SetMaterial(const FString& FilePath)
{
    Material = UResourceManager::GetInstance().Load<UMaterial>(FilePath);
}

void UPrimitiveComponent::DuplicateSubObjects()
{
    Super::DuplicateSubObjects();
}

void UPrimitiveComponent::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
    Super::Serialize(bInIsLoading, InOutHandle);
}
