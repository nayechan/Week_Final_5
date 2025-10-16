#include "pch.h"
#include "PrimitiveComponent.h"
#include "SceneComponent.h"

IMPLEMENT_CLASS(UPrimitiveComponent)

void UPrimitiveComponent::SetMaterial(const FString& FilePath, EVertexLayoutType layoutType)
{
    Material = UResourceManager::GetInstance().Load<UMaterial>(FilePath, layoutType);
}

//void UPrimitiveComponent::Serialize(bool bIsLoading, FSceneCompData& InOut)
//{
//    if (bIsLoading)
//    {
//        // FPrimitiveData -> 컴포넌트 월드 트랜스폼
//        FTransform WT = GetWorldTransform();
//        WT.Translation = InOut.Location;
//        WT.Rotation = QuatFromEulerZYX_Deg(InOut.Rotation);
//        WT.Scale3D = InOut.Scale;
//        SetWorldTransform(WT);
//    }
//    else
//    {
//        // 컴포넌트 월드 트랜스폼 -> FPrimitiveData
//        const FTransform WT = GetWorldTransform();
//        InOut.Location = WT.Translation;
//        InOut.Rotation = EulerZYX_Deg_FromQuat(WT.Rotation);
//        InOut.Scale = WT.Scale3D;
//    }
//}

void UPrimitiveComponent::DuplicateSubObjects()
{
    Super::DuplicateSubObjects();
}
