#include "pch.h"
#include "MeshComponent.h"
#include "StaticMesh.h"
#include "ObjManager.h"
#include "Shader.h"

IMPLEMENT_CLASS(UMeshComponent)

UMeshComponent::UMeshComponent()
{
}

UMeshComponent::~UMeshComponent()
{
    Material = nullptr;
}

void UMeshComponent::SetViewModeShader(UShader* InShader)
{
	// 기본 구현: Material이 있으면 셰이더 교체
	// 파생 클래스에서 필요에 따라 override 가능
	if (InShader && Material)
	{
		Material->SetShader(InShader);
	}
}

void UMeshComponent::DuplicateSubObjects()
{
    Super::DuplicateSubObjects();
}
