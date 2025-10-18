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

BEGIN_PROPERTIES(UFireBallComponent)
	MARK_AS_COMPONENT("파이어볼 컴포넌트", "구 형태의 발광 효과를 생성합니다.")
	ADD_PROPERTY_RANGE(float, Intensity, "FireBall", 0.0f, 100.0f, true, "발광 강도입니다.")
	ADD_PROPERTY_RANGE(float, Radius, "FireBall", 0.0f, 1000.0f, true, "효과 반경입니다.")
	ADD_PROPERTY_RANGE(float, RadiusFallOff, "FireBall", 0.0f, 10.0f, true, "반경 감쇠 지수입니다.")
	ADD_PROPERTY(FLinearColor, Color, "FireBall", true)
	ADD_PROPERTY(FString, LightingShaderPath, "FireBall", false)
END_PROPERTIES()

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

#pragma endregion

void UFireBallComponent::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	Super::Serialize(bInIsLoading, InOutHandle);

	// 리플렉션 기반 자동 직렬화
	AutoSerialize(bInIsLoading, InOutHandle, UFireBallComponent::StaticClass());
}

void UFireBallComponent::DuplicateSubObjects()
{
	// TODO
}

void UFireBallComponent::OnTransformUpdatedChildImpl()
{
	// TODO
}
