#include "pch.h"
#include "TextRenderComponent.h"
#include "Shader.h"
#include "StaticMesh.h"
#include "Quad.h"
#include "StaticMeshComponent.h"
#include "BillboardComponent.h"
#include "FViewportClient.h"
#include "FViewport.h"
#include "World.h"
#include "RenderManager.h"
#include "WorldPartitionManager.h"
#include "Renderer.h"
#include "CameraActor.h"
#include "CameraComponent.h"
#include "PrimitiveComponent.h"
#include "StaticMeshActor.h"
#include "Gizmo/GizmoActor.h"
#include "Grid/GridActor.h"
#include "Octree.h"
#include "BVHierarchy.h"
#include "Occlusion.h"
#include "Frustum.h"
#include "ResourceManager.h"
#include "RHIDevice.h"
#include "Material.h"
#include "Texture.h"
#include "RenderSettings.h"
#include "EditorEngine.h"
#include "DecalComponent.h"
#include "DecalStatManager.h"
#include "SceneRenderer.h"
#include "SceneView.h"

#include <Windows.h>

URenderer::URenderer(D3D11RHI* InDevice) : RHIDevice(InDevice)
{
	InitializeLineBatch();
}

URenderer::~URenderer()
{
	if (LineBatchData)
	{
		delete LineBatchData;
	}
}

void URenderer::BeginFrame()
{
	RHIDevice->IASetPrimitiveTopology();

	RHIDevice->OMSetRenderTargets(ERTVMode::BackBufferWithDepth);

	// 프레임별 데칼 통계를 추적하기 위해 초기화
	FDecalStatManager::GetInstance().ResetFrameStats();

	RHIDevice->ClearAllBuffer();
}

void URenderer::EndFrame()
{
	RHIDevice->Present();
}

void URenderer::RenderSceneForView(UWorld* World, ACameraActor* Camera, FViewport* Viewport)
{
	// 1. 렌더에 필요한 정보를 모은 FSceneView를 생성합니다.
	FSceneView View(Camera, Viewport, World->GetRenderSettings().GetViewModeIndex());	// NOTE: 현재 viewport에 해당하는 ViewMode가 적용되는지 확인 필요

	// 2. FSceneRenderer 생성자에 'View'의 주소(&View)를 전달합니다.
	FSceneRenderer SceneRenderer(World, &View, this);

	// 3. 실제로 렌더를 수행합니다.
	SceneRenderer.Render();
}

UPrimitiveComponent* URenderer::GetPrimitiveCollided(int MouseX, int MouseY) const
{
	//GPU와 동기화 문제 때문에 Map이 호출될때까지 기다려야해서 피킹 하는 프레임에 엄청난 프레임 드랍이 일어남.
   //******비동기 방식으로 무조건 바꿔야함****************
	uint32 PickedId = 0;

	ID3D11DeviceContext* DeviceContext = RHIDevice->GetDeviceContext();
	//스테이징 버퍼를 가져와야 하는데 이걸 Device 추상 클래스가 Getter로 가지고 있는게 좋은 설계가 아닌 것 같아서 일단 캐스팅함


	D3D11_BOX Box{};
	Box.left = MouseX;
	Box.right = MouseX + 1;
	Box.top = MouseY;
	Box.bottom = MouseY + 1;
	Box.front = 0;
	Box.back = 1;

	DeviceContext->CopySubresourceRegion(
		RHIDevice->GetIdStagingBuffer(),
		0,
		0, 0, 0,
		RHIDevice->GetIdBuffer(),
		0,
		&Box);
	D3D11_MAPPED_SUBRESOURCE MapResource{};
	if (SUCCEEDED(DeviceContext->Map(RHIDevice->GetIdStagingBuffer(), 0, D3D11_MAP_READ, 0, &MapResource)))
	{
		PickedId = *static_cast<uint32*>(MapResource.pData);
		DeviceContext->Unmap(RHIDevice->GetIdStagingBuffer(), 0);
	}

	if (PickedId == 0)
		return nullptr;
	return Cast<UPrimitiveComponent>(GUObjectArray[PickedId]);
}

void URenderer::DrawIndexedPrimitiveComponent(UStaticMesh* InMesh, D3D11_PRIMITIVE_TOPOLOGY InTopology, const TArray<FMaterialSlot>& InComponentMaterialSlots)
{
	UINT Stride = 0;
	switch (InMesh->GetVertexType())
	{
	case EVertexLayoutType::PositionColor:
		Stride = sizeof(FVertexSimple);
		break;
	case EVertexLayoutType::PositionColorTexturNormal:
		Stride = sizeof(FVertexDynamic);
		break;
	case EVertexLayoutType::PositionTextBillBoard:
		Stride = sizeof(FBillboardVertexInfo_GPU);
		break;
	case EVertexLayoutType::PositionBillBoard:
		Stride = sizeof(FBillboardVertex);
		break;
	default:
		// Handle unknown or unsupported vertex types
		assert(false && "Unknown vertex type!");
		return; // or log an error
	}
	UINT Offset = 0;

	ID3D11Buffer* VertexBuffer = InMesh->GetVertexBuffer();
	ID3D11Buffer* IndexBuffer = InMesh->GetIndexBuffer();
	uint32 VertexCount = InMesh->GetVertexCount();
	uint32 IndexCount = InMesh->GetIndexCount();

	RHIDevice->GetDeviceContext()->IASetVertexBuffers(
		0, 1, &VertexBuffer, &Stride, &Offset
	);

	RHIDevice->GetDeviceContext()->IASetIndexBuffer(
		IndexBuffer, DXGI_FORMAT_R32_UINT, 0
	);

	RHIDevice->GetDeviceContext()->IASetPrimitiveTopology(InTopology);
	RHIDevice->PSSetDefaultSampler(0);

	if (InMesh->HasMaterial())
	{
		const TArray<FGroupInfo>& MeshGroupInfos = InMesh->GetMeshGroupInfo();
		const uint32 NumMeshGroupInfos = static_cast<uint32>(MeshGroupInfos.size());
		for (uint32 i = 0; i < NumMeshGroupInfos; ++i)
		{
			UMaterial* const Material = UResourceManager::GetInstance().Get<UMaterial>(InComponentMaterialSlots[i].MaterialName.ToString());
			const FMaterialParameters& MaterialInfo = Material->GetMaterialInfo();
			ID3D11ShaderResourceView* srv = nullptr;
			bool bHasTexture = false;
			if (!MaterialInfo.DiffuseTextureFileName.empty())
			{
				// UTF-8 -> UTF-16 변환 (Windows)
				int needW = ::MultiByteToWideChar(CP_UTF8, 0, MaterialInfo.DiffuseTextureFileName.c_str(), -1, nullptr, 0);
				std::wstring WTextureFileName;
				if (needW > 0)
				{
					WTextureFileName.resize(needW - 1);
					::MultiByteToWideChar(CP_UTF8, 0, MaterialInfo.DiffuseTextureFileName.c_str(), -1, WTextureFileName.data(), needW);
				}
				// 반환 여기서 로드 
				if (FTextureData* TextureData = UResourceManager::GetInstance().CreateOrGetTextureData(WTextureFileName))
				{
					if (TextureData->TextureSRV)
					{
						srv = TextureData->TextureSRV;
						bHasTexture = true;
					}
				}
			}
			RHIDevice->GetDeviceContext()->PSSetShaderResources(0, 1, &srv);
			//RHIDevice->UpdatePixelConstantBuffers(MaterialInfo, true, bHasTexture); // 성공 여부 기반
			FPixelConstBufferType PixelConst{ FPixelConstBufferType(FMaterialInPs(MaterialInfo), true) };
			PixelConst.bHasTexture = bHasTexture;
			PixelConst.Padding = FVector2D(0.0f, 0.0f);
			RHIDevice->SetAndUpdateConstantBuffer(PixelConst);
			RHIDevice->GetDeviceContext()->DrawIndexed(MeshGroupInfos[i].IndexCount, MeshGroupInfos[i].StartIndex, 0);
		}
	}
	else
	{
		FMaterialParameters ObjMaterialInfo;
		FPixelConstBufferType PixelConst{ FPixelConstBufferType(FMaterialInPs(ObjMaterialInfo)) };
		PixelConst.bHasTexture = false;
		PixelConst.bHasMaterial = false;
		PixelConst.Padding = FVector2D(0.0f, 0.0f);
		//RHIDevice->UpdatePixelConstantBuffers(ObjMaterialInfo, false, false); // PSSet도 해줌
		RHIDevice->SetAndUpdateConstantBuffer(PixelConst);
		RHIDevice->GetDeviceContext()->DrawIndexed(IndexCount, 0, 0);
	}
}

// TEXT BillBoard 용 
void URenderer::DrawIndexedPrimitiveComponent(UTextRenderComponent* Comp, D3D11_PRIMITIVE_TOPOLOGY InTopology)
{
	UINT Stride = sizeof(FBillboardVertexInfo_GPU);
	ID3D11Buffer* VertexBuff = Comp->GetStaticMesh()->GetVertexBuffer();
	ID3D11Buffer* IndexBuff = Comp->GetStaticMesh()->GetIndexBuffer();

	RHIDevice->GetDeviceContext()->IASetInputLayout(Comp->GetMaterial(0)->GetShader()->GetInputLayout());

	UINT offset = 0;
	RHIDevice->GetDeviceContext()->IASetVertexBuffers(
		0, 1, &VertexBuff, &Stride, &offset
	);
	RHIDevice->GetDeviceContext()->IASetIndexBuffer(
		IndexBuff, DXGI_FORMAT_R32_UINT, 0
	);
	ID3D11ShaderResourceView* TextureSRV = Comp->GetMaterial(0)->GetDiffuseTexture()->GetShaderResourceView();
	RHIDevice->PSSetDefaultSampler(0);
	RHIDevice->GetDeviceContext()->PSSetShaderResources(0, 1, &TextureSRV);
	RHIDevice->GetDeviceContext()->IASetPrimitiveTopology(InTopology);
	const uint32 indexCnt = Comp->GetStaticMesh()->GetIndexCount();
	RHIDevice->GetDeviceContext()->DrawIndexed(indexCnt, 0, 0);
}

// 단일 Quad 용 -> Billboard에서 호출 
void URenderer::DrawIndexedPrimitiveComponent(UBillboardComponent* Comp, D3D11_PRIMITIVE_TOPOLOGY InTopology)
{
	UINT Stride = sizeof(FBillboardVertex);
	ID3D11Buffer* VertexBuff = Comp->GetStaticMesh()->GetVertexBuffer();
	ID3D11Buffer* IndexBuff = Comp->GetStaticMesh()->GetIndexBuffer();

	// Input layout comes from the shader bound to the material
	RHIDevice->GetDeviceContext()->IASetInputLayout(Comp->GetMaterial(0)->GetShader()->GetInputLayout());

	UINT offset = 0;
	RHIDevice->GetDeviceContext()->IASetVertexBuffers(0, 1, &VertexBuff, &Stride, &offset);
	RHIDevice->GetDeviceContext()->IASetIndexBuffer(IndexBuff, DXGI_FORMAT_R32_UINT, 0);

	// Bind texture via ResourceManager to support DDS/PNG
	ID3D11ShaderResourceView* srv = nullptr;
	if (Comp->GetMaterial(0))
	{
		const FString& TextName = Comp->GetTextureName();
		if (!TextName.empty())
		{
			int needW = ::MultiByteToWideChar(CP_UTF8, 0, TextName.c_str(), -1, nullptr, 0);
			std::wstring WTextureFileName;
			if (needW > 0)
			{
				WTextureFileName.resize(needW - 1);
				::MultiByteToWideChar(CP_UTF8, 0, TextName.c_str(), -1, WTextureFileName.data(), needW);
			}
			if (FTextureData* TextureData = UResourceManager::GetInstance().CreateOrGetTextureData(WTextureFileName))
			{
				if (TextureData->TextureSRV)
				{
					srv = TextureData->TextureSRV;
				}
			}
		}
	}
	RHIDevice->PSSetDefaultSampler(0);
	RHIDevice->GetDeviceContext()->PSSetShaderResources(0, 1, &srv);

	// Ensure correct alpha blending just for this draw
	RHIDevice->OMSetBlendState(true);

	RHIDevice->GetDeviceContext()->IASetPrimitiveTopology(InTopology);
	const uint32 indexCnt = Comp->GetStaticMesh()->GetIndexCount();
	RHIDevice->GetDeviceContext()->DrawIndexed(indexCnt, 0, 0);

	// Restore blend state so others aren't affected
	RHIDevice->OMSetBlendState(false);
}

void URenderer::InitializeLineBatch()
{
	// Create UDynamicMesh for efficient line batching
	DynamicLineMesh = UResourceManager::GetInstance().Load<ULineDynamicMesh>("Line");

	// Initialize with maximum capacity (MAX_LINES * 2 vertices, MAX_LINES * 2 indices)
	uint32 maxVertices = MAX_LINES * 2;
	uint32 maxIndices = MAX_LINES * 2;
	DynamicLineMesh->Load(maxVertices, maxIndices, RHIDevice->GetDevice());

	// Create FMeshData for accumulating line data
	LineBatchData = new FMeshData();

	// Load line shader
	LineShader = UResourceManager::GetInstance().Load<UShader>("Shaders/UI/ShaderLine.hlsl");
	//LineShader = UResourceManager::GetInstance().Load<UShader>("StaticMeshShader.hlsl", EVertexLayoutType::PositionColorTexturNormal);
}

void URenderer::BeginLineBatch()
{
	if (!LineBatchData) return;

	bLineBatchActive = true;

	// Clear previous batch data
	LineBatchData->Vertices.clear();
	LineBatchData->Color.clear();
	LineBatchData->Indices.clear();
}

void URenderer::AddLine(const FVector& Start, const FVector& End, const FVector4& Color)
{
	if (!bLineBatchActive || !LineBatchData) return;

	uint32 startIndex = static_cast<uint32>(LineBatchData->Vertices.size());

	// Add vertices
	LineBatchData->Vertices.push_back(Start);
	LineBatchData->Vertices.push_back(End);

	// Add colors
	LineBatchData->Color.push_back(Color);
	LineBatchData->Color.push_back(Color);

	// Add indices for line (2 vertices per line)
	LineBatchData->Indices.push_back(startIndex);
	LineBatchData->Indices.push_back(startIndex + 1);
}

void URenderer::AddLines(const TArray<FVector>& StartPoints, const TArray<FVector>& EndPoints, const TArray<FVector4>& Colors)
{
	if (!bLineBatchActive || !LineBatchData) return;

	// Validate input arrays have same size
	if (StartPoints.size() != EndPoints.size() || StartPoints.size() != Colors.size())
		return;

	uint32 startIndex = static_cast<uint32>(LineBatchData->Vertices.size());

	// Reserve space for efficiency
	size_t lineCount = StartPoints.size();
	LineBatchData->Vertices.reserve(LineBatchData->Vertices.size() + lineCount * 2);
	LineBatchData->Color.reserve(LineBatchData->Color.size() + lineCount * 2);
	LineBatchData->Indices.reserve(LineBatchData->Indices.size() + lineCount * 2);

	// Add all lines at once
	for (size_t i = 0; i < lineCount; ++i)
	{
		uint32 currentIndex = startIndex + static_cast<uint32>(i * 2);

		// Add vertices
		LineBatchData->Vertices.push_back(StartPoints[i]);
		LineBatchData->Vertices.push_back(EndPoints[i]);

		// Add colors
		LineBatchData->Color.push_back(Colors[i]);
		LineBatchData->Color.push_back(Colors[i]);

		// Add indices for line (2 vertices per line)
		LineBatchData->Indices.push_back(currentIndex);
		LineBatchData->Indices.push_back(currentIndex + 1);
	}
}

void URenderer::EndLineBatch(const FMatrix& ModelMatrix, const FMatrix& ViewMatrix, const FMatrix& ProjectionMatrix)
{
	if (!bLineBatchActive || !LineBatchData || !DynamicLineMesh || LineBatchData->Vertices.empty())
	{
		bLineBatchActive = false;
		return;
	}

	// Clamp to GPU buffer capacity to avoid full drop when overflowing
	const uint32 totalLines = static_cast<uint32>(LineBatchData->Indices.size() / 2);
	if (totalLines > MAX_LINES)
	{
		const uint32 clampedLines = MAX_LINES;
		const uint32 clampedVerts = clampedLines * 2;
		const uint32 clampedIndices = clampedLines * 2;
		LineBatchData->Vertices.resize(clampedVerts);
		LineBatchData->Color.resize(clampedVerts);
		LineBatchData->Indices.resize(clampedIndices);
	}

	// Efficiently update dynamic mesh data (no buffer recreation!)
	if (!DynamicLineMesh->UpdateData(LineBatchData, RHIDevice->GetDeviceContext()))
	{
		bLineBatchActive = false;
		return;
	}

	// Set up rendering state
	FMatrix ModelInvTranspose = ModelMatrix.InverseAffine().Transpose();
	RHIDevice->SetAndUpdateConstantBuffer(ModelBufferType(ModelMatrix, ModelInvTranspose));
	RHIDevice->SetAndUpdateConstantBuffer(ViewProjBufferType(ViewMatrix, ProjectionMatrix));
	RHIDevice->PrepareShader(LineShader);

	// Render using dynamic mesh
	if (DynamicLineMesh->GetCurrentVertexCount() > 0 && DynamicLineMesh->GetCurrentIndexCount() > 0)
	{
		UINT stride = sizeof(FVertexSimple);
		UINT offset = 0;

		ID3D11Buffer* vertexBuffer = DynamicLineMesh->GetVertexBuffer();
		ID3D11Buffer* indexBuffer = DynamicLineMesh->GetIndexBuffer();

		RHIDevice->GetDeviceContext()->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);
		RHIDevice->GetDeviceContext()->IASetIndexBuffer(indexBuffer, DXGI_FORMAT_R32_UINT, 0);
		RHIDevice->GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
		// Overlay 스텐실(=1) 영역은 그리지 않도록 스텐실 테스트 설정
		RHIDevice->OMSetDepthStencilState_StencilRejectOverlay();
		RHIDevice->GetDeviceContext()->DrawIndexed(DynamicLineMesh->GetCurrentIndexCount(), 0, 0);
		// 상태 복구
		RHIDevice->GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		RHIDevice->OMSetDepthStencilState(EComparisonFunc::LessEqual);
	}

	bLineBatchActive = false;
}

void URenderer::ClearLineBatch()
{
	if (!LineBatchData) return;

	LineBatchData->Vertices.clear();
	LineBatchData->Color.clear();
	LineBatchData->Indices.clear();

	bLineBatchActive = false;
}
