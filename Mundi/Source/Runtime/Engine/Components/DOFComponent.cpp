#include "pch.h"
#include "DOFComponent.h"
#include "BillboardComponent.h"
void UDOFComponent::OnRegister(UWorld* InWorld)
{
	Super::OnRegister(InWorld);
	if (!SpriteComponent && !InWorld->bPie)
	{
		CREATE_EDITOR_COMPONENT(SpriteComponent, UBillboardComponent);
		SpriteComponent->SetTexture(GDataDir + "/UI/Icons/S_AtmosphericHeightFog.dds");
	}

}

void UDOFComponent::RenderHeightFog(URenderer* Renderer)
{
}

void UDOFComponent::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	Super::Serialize(bInIsLoading, InOutHandle);
}

void UDOFComponent::DuplicateSubObjects()
{
	Super::DuplicateSubObjects();
}
