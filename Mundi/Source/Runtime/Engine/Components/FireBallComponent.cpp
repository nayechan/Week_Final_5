#include "pch.h"
#include "FireBallComponent.h"
#include "D3D11RHI.h"
#include "Enums.h"
#include "Renderer.h"
#include "ResourceManager.h"
#include "Shader.h"
#include "StaticMesh.h"
#include "StaticMeshComponent.h"
#include "VertexData.h"

IMPLEMENT_CLASS(UFireBallComponent)

UFireBallComponent::UFireBallComponent()
{
	SetCanEverTick(false);
	SetTickEnabled(false);

	LightingShaderPath = "Shaders/Effects/FireBallShader.hlsl";
	LightingShader = UResourceManager::GetInstance().Load<UShader>(LightingShaderPath);
	if (!LightingShader)
	{
		UE_LOG("UFireBallComponent: failed to load fireball lighting shader '%s'", LightingShaderPath.c_str());
	}
}

UFireBallComponent::~UFireBallComponent()
{
	LightingShader = nullptr;
	ShadowResources = nullptr;
}

#pragma region Lifecycle
void UFireBallComponent::InitializeComponent()
{
	Super::InitializeComponent();
}

void UFireBallComponent::BeginPlay()
{
	Super::BeginPlay();
}

void UFireBallComponent::TickComponent(float DeltaTime)
{
	Super::TickComponent(DeltaTime);
}

void UFireBallComponent::EndPlay(EEndPlayReason Reason)
{
	Super::EndPlay(Reason);
}
#pragma endregion

#pragma region Rendering (WIP)
void UFireBallComponent::RenderAffectedPrimitives(URenderer* Renderer, UPrimitiveComponent* Target, const FMatrix& View, const FMatrix& Proj)
{
	if (!Renderer || !Target)
	{
		return;
	}

	UStaticMeshComponent* SMC = Cast<UStaticMeshComponent>(Target);
	if (!SMC)
	{
		return;
	}

	UStaticMesh* StaticMesh = SMC->GetStaticMesh();
	if (!StaticMesh || !StaticMesh->GetStaticMeshAsset())
	{
		return;
	}

	if (!LightingShader)
	{
		LightingShader = UResourceManager::GetInstance().Load<UShader>(LightingShaderPath);
		if (!LightingShader)
		{
			UE_LOG("UFireBallComponent: failed to resolve fireball shader at runtime ('%s')", LightingShaderPath.c_str());
			return;
		}
	}

	D3D11RHI* RHIDevice = Renderer->GetRHIDevice();
	if (!RHIDevice)
	{
		return;
	}

	FMatrix SMCWorld = SMC->GetWorldMatrix();
	FMatrix SMCWorldInvTranspose = SMCWorld.InverseAffine().Transpose();
	RHIDevice->SetAndUpdateConstantBuffer(ModelBufferType(SMCWorld, SMCWorldInvTranspose));
	RHIDevice->SetAndUpdateConstantBuffer(ViewProjBufferType(View, Proj));

	const FVector Center = GetWorldLocation();
	const float SafeRadius = Radius > KINDA_SMALL_NUMBER ? Radius : KINDA_SMALL_NUMBER;
	//RHIDevice->UpdateFireBallConstantBuffers(Center, SafeRadius, Intensity, RadiusFallOff, Color);
	float Pad[2];
	FireBallBufferType FireBallConst{ Center, SafeRadius, Intensity, RadiusFallOff };
	FireBallConst.Color = Color;
	RHIDevice->SetAndUpdateConstantBuffer(FireBallConst);
	RHIDevice->PrepareShader(LightingShader);

	if (StaticMesh->GetVertexType() != EVertexLayoutType::PositionColorTexturNormal)
	{
		UE_LOG("UFireBallComponent: unsupported vertex layout for fireball pass (%d)", static_cast<int>(StaticMesh->GetVertexType()));
		return;
	}

	UINT stride = sizeof(FVertexDynamic);
	UINT offset = 0;
	ID3D11Buffer* VertexBuffer = StaticMesh->GetVertexBuffer();
	ID3D11Buffer* IndexBuffer = StaticMesh->GetIndexBuffer();
	if (!VertexBuffer || !IndexBuffer)
	{
		return;
	}

	RHIDevice->GetDeviceContext()->IASetVertexBuffers(0, 1, &VertexBuffer, &stride, &offset);
	RHIDevice->GetDeviceContext()->IASetIndexBuffer(IndexBuffer, DXGI_FORMAT_R32_UINT, 0);
	RHIDevice->GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	RHIDevice->GetDeviceContext()->DrawIndexed(StaticMesh->GetIndexCount(), 0, 0);
}

void UFireBallComponent::RenderDebugVolume(URenderer* Renderer, const FMatrix& View, const FMatrix& Proj) const
{
}
#pragma endregion

#pragma region Parameters
void UFireBallComponent::SetIntensity(float InIntensity)
{
	Intensity = std::max(0.0f, InIntensity);
}

void UFireBallComponent::SetRadius(float InRadius)
{
	Radius = std::max(0.0f, InRadius);
}

void UFireBallComponent::SetRadiusFallOff(float InRadiusFallOff)
{
	RadiusFallOff = std::max(0.0f, InRadiusFallOff);
}

void UFireBallComponent::SetColor(const FLinearColor& InColor)
{
	Color = InColor;
}

FBoundingSphere UFireBallComponent::GetBoundingSphere() const
{
	return FBoundingSphere(GetWorldLocation(), Radius);
}

void UFireBallComponent::SetShadowCaptureEnabled(bool bEnabled)
{
	bShadowCaptureEnabled = bEnabled;
}

void UFireBallComponent::SetShadowMapResolution(uint32 InResolution)
{
	ShadowMapResolution = std::max(128u, InResolution);
}
#pragma endregion

void UFireBallComponent::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	Super::Serialize(bInIsLoading, InOutHandle);

	if (bInIsLoading)
	{
		if (InOutHandle.hasKey("Intensity"))
		{
			FJsonSerializer::ReadFloat(InOutHandle, "Intensity", Intensity);
		}

		if (InOutHandle.hasKey("Radius"))
		{
			FJsonSerializer::ReadFloat(InOutHandle, "Radius", Radius);
		}

		if (InOutHandle.hasKey("RadiusFallOff"))
		{
			FJsonSerializer::ReadFloat(InOutHandle, "RadiusFallOff", RadiusFallOff);
		}

		if (InOutHandle.hasKey("Color"))
		{
			FVector4 ColorVec;
			FJsonSerializer::ReadVector4(InOutHandle, "Color", ColorVec);
			Color = FLinearColor(ColorVec);
		}

		if (InOutHandle.hasKey("LightingShaderPath"))
		{
			FString ShaderPath = InOutHandle["LightingShaderPath"].ToString();
			if (!ShaderPath.empty())
			{
				LightingShaderPath = ShaderPath;
				LightingShader = UResourceManager::GetInstance().Load<UShader>(LightingShaderPath);
				if (!LightingShader)
				{
					UE_LOG("UFireBallComponent: failed to load fireball lighting shader '%s'", LightingShaderPath.c_str());
				}
			}
		}

		if (InOutHandle.hasKey("bShadowCaptureEnabled"))
		{
			int32 Enabled = 0;
			FJsonSerializer::ReadInt32(InOutHandle, "bShadowCaptureEnabled", Enabled);
			bShadowCaptureEnabled = (Enabled != 0);
		}

		if (InOutHandle.hasKey("ShadowMapResolution"))
		{
			int32 Resolution = 0;
			if (FJsonSerializer::ReadInt32(InOutHandle, "ShadowMapResolution", Resolution))
			{
				ShadowMapResolution = std::max(128u, static_cast<uint32>(Resolution));
			}
		}
	}
	else
	{
		InOutHandle["Intensity"] = Intensity;
		InOutHandle["Radius"] = Radius;
		InOutHandle["RadiusFallOff"] = RadiusFallOff;
		InOutHandle["Color"] = FJsonSerializer::Vector4ToJson(Color.ToFVector4());
		InOutHandle["LightingShaderPath"] = LightingShaderPath;
		InOutHandle["bShadowCaptureEnabled"] = bShadowCaptureEnabled ? 1 : 0;
		InOutHandle["ShadowMapResolution"] = static_cast<int32>(ShadowMapResolution);
	}
}

void UFireBallComponent::DuplicateSubObjects()
{
	// TODO
}

void UFireBallComponent::OnTransformUpdatedChildImpl()
{
	// TODO
}
