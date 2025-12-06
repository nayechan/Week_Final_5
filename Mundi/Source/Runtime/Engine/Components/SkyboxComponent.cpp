#include "pch.h"
#include "SkyboxComponent.h"

#include "ResourceManager.h"
#include "Texture.h"
#include "Shader.h"
#include "StaticMesh.h"
#include "BillboardComponent.h"

USkyboxComponent::USkyboxComponent()
{
	InitializeRenderingResources();
}

USkyboxComponent::~USkyboxComponent()
{
}

void USkyboxComponent::InitializeRenderingResources()
{
	// 스카이박스 셰이더 로드
	SkyboxShader = UResourceManager::GetInstance().Load<UShader>("Shaders/Skybox/Skybox.hlsl");

	// 스카이박스용 뒤집힌 큐브 메시 생성 (내부에서 볼 수 있도록)
	// 이 메시는 ResourceManager에 캐시됨
	SkyboxMesh = UResourceManager::GetInstance().Get<UStaticMesh>("__SkyboxCube__");
	if (!SkyboxMesh)
	{
		// 뒤집힌 큐브 메시 생성 - 내부에서 바라보므로 winding이 반대
		// UV 좌표가 포함된 정점 데이터
		struct FSkyboxVertex
		{
			float X, Y, Z;    // Position
			float U, V;       // UV
		};

		// 6면 큐브 (각 면 4개 정점, 총 24개)
		// 내부에서 보이도록 winding 설정
		// LH Z-up 좌표계: +X=Forward, +Y=Right, +Z=Up
		// 순서: Front(+X), Back(-X), Top(+Z), Bottom(-Z), Right(+Y), Left(-Y)

		// UV bias로 텍스처 가장자리 bleeding 방지
		constexpr float UVMin = 0.001f;
		constexpr float UVMax = 0.999f;

		FSkyboxVertex Vertices[] = {
			// Front (+X) - 카메라가 +X를 바라볼 때 (up=+Z, right=+Y)
			{  1.0f, -1.0f,  1.0f, UVMin, UVMin },  // top-left
			{  1.0f,  1.0f,  1.0f, UVMax, UVMin },  // top-right
			{  1.0f,  1.0f, -1.0f, UVMax, UVMax },  // bottom-right
			{  1.0f, -1.0f, -1.0f, UVMin, UVMax },  // bottom-left

			// Back (-X) - 카메라가 -X를 바라볼 때 (up=+Z, right=-Y)
			{ -1.0f,  1.0f,  1.0f, UVMin, UVMin },  // top-left
			{ -1.0f, -1.0f,  1.0f, UVMax, UVMin },  // top-right
			{ -1.0f, -1.0f, -1.0f, UVMax, UVMax },  // bottom-right
			{ -1.0f,  1.0f, -1.0f, UVMin, UVMax },  // bottom-left

			// Top (+Z) - UV 시계방향 90도 회전
			{  1.0f, -1.0f,  1.0f, UVMax, UVMax },
			{  1.0f,  1.0f,  1.0f, UVMax, UVMin },
			{ -1.0f,  1.0f,  1.0f, UVMin, UVMin },
			{ -1.0f, -1.0f,  1.0f, UVMin, UVMax },

			// Bottom (-Z) - UV 시계방향 90도 회전
			{  1.0f, -1.0f, -1.0f, UVMax, UVMin },
			{  1.0f,  1.0f, -1.0f, UVMax, UVMax },
			{ -1.0f,  1.0f, -1.0f, UVMin, UVMax },
			{ -1.0f, -1.0f, -1.0f, UVMin, UVMin },

			// Right (+Y) - 카메라가 +Y를 바라볼 때 (up=+Z, right=-X)
			{  1.0f,  1.0f,  1.0f, UVMin, UVMin },  // top-left
			{ -1.0f,  1.0f,  1.0f, UVMax, UVMin },  // top-right
			{ -1.0f,  1.0f, -1.0f, UVMax, UVMax },  // bottom-right
			{  1.0f,  1.0f, -1.0f, UVMin, UVMax },  // bottom-left

			// Left (-Y) - 카메라가 -Y를 바라볼 때 (up=+Z, right=+X)
			{ -1.0f, -1.0f,  1.0f, UVMin, UVMin },  // top-left
			{  1.0f, -1.0f,  1.0f, UVMax, UVMin },  // top-right
			{  1.0f, -1.0f, -1.0f, UVMax, UVMax },  // bottom-right
			{ -1.0f, -1.0f, -1.0f, UVMin, UVMax },  // bottom-left
		};

		// 인덱스 (각 면 2개 삼각형, 총 36개)
		uint32 Indices[] = {
			// Front (+X)
			0, 1, 2, 0, 2, 3,
			// Back (-X)
			4, 5, 6, 4, 6, 7,
			// Top (+Z)
			8, 9, 10, 8, 10, 11,
			// Bottom (-Z)
			12, 13, 14, 12, 14, 15,
			// Right (+Y)
			16, 17, 18, 16, 18, 19,
			// Left (-Y)
			20, 21, 22, 20, 22, 23
		};

		ID3D11Device* Device = UResourceManager::GetInstance().GetDevice();

		// 정점 버퍼 생성
		D3D11_BUFFER_DESC VBDesc = {};
		VBDesc.Usage = D3D11_USAGE_IMMUTABLE;
		VBDesc.ByteWidth = sizeof(Vertices);
		VBDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

		D3D11_SUBRESOURCE_DATA VBData = {};
		VBData.pSysMem = Vertices;

		ID3D11Buffer* VertexBuffer = nullptr;
		Device->CreateBuffer(&VBDesc, &VBData, &VertexBuffer);

		// 인덱스 버퍼 생성
		D3D11_BUFFER_DESC IBDesc = {};
		IBDesc.Usage = D3D11_USAGE_IMMUTABLE;
		IBDesc.ByteWidth = sizeof(Indices);
		IBDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

		D3D11_SUBRESOURCE_DATA IBData = {};
		IBData.pSysMem = Indices;

		ID3D11Buffer* IndexBuffer = nullptr;
		Device->CreateBuffer(&IBDesc, &IBData, &IndexBuffer);

		// StaticMesh 생성
		SkyboxMesh = NewObject<UStaticMesh>();
		SkyboxMesh->SetBuffers(VertexBuffer, IndexBuffer, 24, 36, sizeof(FSkyboxVertex));
		SkyboxMesh->SetFilePath("__SkyboxCube__");

		UResourceManager::GetInstance().Add<UStaticMesh>("__SkyboxCube__", SkyboxMesh);
	}
}

void USkyboxComponent::SetTexture(ESkyboxFace Face, const FString& TexturePath)
{
	UTexture* Texture = UResourceManager::GetInstance().Load<UTexture>(TexturePath);
	SetTexture(Face, Texture);
}

void USkyboxComponent::SetTexture(ESkyboxFace Face, UTexture* InTexture)
{
	switch (Face)
	{
	case ESkyboxFace::Front:  TextureFront = InTexture; break;
	case ESkyboxFace::Back:   TextureBack = InTexture; break;
	case ESkyboxFace::Top:    TextureTop = InTexture; break;
	case ESkyboxFace::Bottom: TextureBottom = InTexture; break;
	case ESkyboxFace::Right:  TextureRight = InTexture; break;
	case ESkyboxFace::Left:   TextureLeft = InTexture; break;
	}
}

UTexture* USkyboxComponent::GetTexture(ESkyboxFace Face) const
{
	switch (Face)
	{
	case ESkyboxFace::Front:  return TextureFront;
	case ESkyboxFace::Back:   return TextureBack;
	case ESkyboxFace::Top:    return TextureTop;
	case ESkyboxFace::Bottom: return TextureBottom;
	case ESkyboxFace::Right:  return TextureRight;
	case ESkyboxFace::Left:   return TextureLeft;
	}
	return nullptr;
}

void USkyboxComponent::SetAllTextures(const FString& BasePath, const FString& Extension)
{
	// 일반적인 스카이박스 네이밍 컨벤션 사용
	// BasePath + "_front", "_back", "_top", "_bottom", "_right", "_left"
	SetTexture(ESkyboxFace::Front, BasePath + "_front" + Extension);
	SetTexture(ESkyboxFace::Back, BasePath + "_back" + Extension);
	SetTexture(ESkyboxFace::Top, BasePath + "_top" + Extension);
	SetTexture(ESkyboxFace::Bottom, BasePath + "_bottom" + Extension);
	SetTexture(ESkyboxFace::Right, BasePath + "_right" + Extension);
	SetTexture(ESkyboxFace::Left, BasePath + "_left" + Extension);
}

void USkyboxComponent::OnRegister(UWorld* InWorld)
{
	Super::OnRegister(InWorld);

	// 에디터 아이콘 (PIE 모드가 아닐 때만)
	if (!SpriteComponent && !InWorld->bPie)
	{
		CREATE_EDITOR_COMPONENT(SpriteComponent, UBillboardComponent);
		SpriteComponent->SetTexture(GDataDir + "/UI/Icons/S_SkyLight.dds");
	}
}

void USkyboxComponent::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	Super::Serialize(bInIsLoading, InOutHandle);

	auto SerializeTexture = [&](const char* Key, UTexture*& Texture)
	{
		if (bInIsLoading)
		{
			if (InOutHandle.hasKey(Key))
			{
				FString Path = InOutHandle[Key].ToString();
				if (!Path.empty())
				{
					Texture = UResourceManager::GetInstance().Load<UTexture>(Path.c_str());
				}
			}
		}
		else
		{
			if (Texture)
			{
				InOutHandle[Key] = Texture->GetFilePath();
			}
		}
	};

	SerializeTexture("TextureFront", TextureFront);
	SerializeTexture("TextureBack", TextureBack);
	SerializeTexture("TextureTop", TextureTop);
	SerializeTexture("TextureBottom", TextureBottom);
	SerializeTexture("TextureRight", TextureRight);
	SerializeTexture("TextureLeft", TextureLeft);

	if (bInIsLoading)
	{
		FJsonSerializer::ReadFloat(InOutHandle, "Intensity", Intensity, 1.0f);
		FJsonSerializer::ReadBool(InOutHandle, "bEnabled", bEnabled, true);
	}
	else
	{
		InOutHandle["Intensity"] = Intensity;
		InOutHandle["bEnabled"] = bEnabled;
	}
}

void USkyboxComponent::DuplicateSubObjects()
{
	Super::DuplicateSubObjects();
}
