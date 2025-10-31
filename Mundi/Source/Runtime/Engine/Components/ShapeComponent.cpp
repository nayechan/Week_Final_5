#include "pch.h"
#include "ShapeComponent.h"
#include "OBB.h"

UShapeComponent::UShapeComponent()
{
    ShapeColor = FVector4(0.2f, 0.8f, 1.0f, 1.0f);
}

void UShapeComponent::IsOverrlappingActor(const AActor* Other)
{
}

void UShapeComponent::OnRegister(UWorld* InWorld)
{
    Super::OnRegister(InWorld);
    
    GetWorldAABB();
}

FAABB UShapeComponent::GetWorldAABB() const
{
    if (AActor* Owner = GetOwner())
    {
        FAABB OwnerBounds = Owner->GetBounds();
        const FVector HalfExtent = OwnerBounds.GetHalfExtent();
        WorldAABB = OwnerBounds; 
    }
    return WorldAABB; 
}