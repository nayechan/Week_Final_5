#include "pch.h"
#include "HeightFogComponent.h"

#include "Color.h"
#include "ResourceManager.h"

UHeightFogComponent::UHeightFogComponent()
{
	// 사막 느낌
	FogInscatteringColor = new FLinearColor(0.8f, 0.7f, 0.5f, 1.0f);
	FullScreenQuadMesh = UResourceManager::GetInstance().Load<UStaticMesh>("Data/FullScreenQuad.obj");
	HeightFogShader = UResourceManager::GetInstance().Load<UShader>("HeightFogShader.hlsl");
}

UHeightFogComponent::~UHeightFogComponent()
{
	if (FogInscatteringColor != nullptr)
	{
		delete FogInscatteringColor;
		FogInscatteringColor = nullptr;
	}
}

void UHeightFogComponent::RenderHeightFog(URenderer* Renderer)
{
}

void UHeightFogComponent::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	if (bInIsLoading)
	{
		// Load FogInscatteringColor
		if (InOutHandle.hasKey("FogInscatteringColor"))
		{
			FVector4 ColorVec;
			FJsonSerializer::ReadVector4(InOutHandle, "FogInscatteringColor", ColorVec);
			FogInscatteringColor = new FLinearColor(ColorVec);
		}

		// Load FullScreenQuadMesh
		if (InOutHandle.hasKey("FullScreenQuadMesh"))
		{
			FString meshPath = InOutHandle["FullScreenQuadMesh"].ToString();
			if (!meshPath.empty())
			{
				FullScreenQuadMesh = UResourceManager::GetInstance().Load<UStaticMesh>(meshPath.c_str());
			}
		}

		// Load HeightFogShader
		if (InOutHandle.hasKey("HeightFogShader"))
		{
			FString shaderPath = InOutHandle["HeightFogShader"].ToString();
			if (!shaderPath.empty())
			{
				HeightFogShader = UResourceManager::GetInstance().Load<UShader>(shaderPath.c_str());
			}
		}
	}
	else
	{
		// Save FogInscatteringColor
		if (FogInscatteringColor != nullptr)
		{
			InOutHandle["FogInscatteringColor"] = FJsonSerializer::Vector4ToJson(FogInscatteringColor->ToFVector4());
		}

		// Save FullScreenQuadMesh
		if (FullScreenQuadMesh != nullptr)
		{
			InOutHandle["FullScreenQuadMesh"] = FullScreenQuadMesh->GetFilePath();
		}
		else
		{
			InOutHandle["FullScreenQuadMesh"] = "";
		}

		// Save HeightFogShader
		if (HeightFogShader != nullptr)
		{
			InOutHandle["HeightFogShader"] = HeightFogShader->GetFilePath();
		}
		else
		{
			InOutHandle["HeightFogShader"] = "";
		}
	}
}

void UHeightFogComponent::DuplicateSubObjects()
{
	Super::DuplicateSubObjects();
}
