#include "pch.h"
#include "HeightFogComponent.h"

#include "Color.h"
#include "ResourceManager.h"

IMPLEMENT_CLASS(UHeightFogComponent)

UHeightFogComponent::UHeightFogComponent()
{
	// 사막 느낌
	FogInscatteringColor = new FLinearColor(0.93f, 0.79f, 0.69f, 1.0f);
	FullScreenQuadMesh = UResourceManager::GetInstance().Load<UStaticMesh>("Data/FullScreenQuad.obj");
	HeightFogShader = UResourceManager::GetInstance().Load<UShader>("Shaders/Effects/Fog.hlsl");
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
	Super::Serialize(bInIsLoading, InOutHandle);

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

		// FogDensity, FogHeightFalloff, StartDistance, FogCutoffDistance, FogMaxOpacity도 로드
		FJsonSerializer::ReadFloat(InOutHandle, "FogDensity", FogDensity, 0.2f);
		FJsonSerializer::ReadFloat(InOutHandle, "FogHeightFalloff", FogHeightFalloff, 0.2f);
		FJsonSerializer::ReadFloat(InOutHandle, "StartDistance", StartDistance, 0.0f);
		FJsonSerializer::ReadFloat(InOutHandle, "FogCutoffDistance", FogCutoffDistance, 6000.0f);
		FJsonSerializer::ReadFloat(InOutHandle, "FogMaxOpacity", FogMaxOpacity, 1.0f);
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

		// FogDensity, FogHeightFalloff, StartDistance, FogCutoffDistance, FogMaxOpacity도 저장
		InOutHandle["FogDensity"] = FogDensity;
		InOutHandle["FogHeightFalloff"] = FogHeightFalloff;
		InOutHandle["StartDistance"] = StartDistance;
		InOutHandle["FogCutoffDistance"] = FogCutoffDistance;
		InOutHandle["FogMaxOpacity"] = FogMaxOpacity;

	}
}

void UHeightFogComponent::DuplicateSubObjects()
{
	Super::DuplicateSubObjects();
}
