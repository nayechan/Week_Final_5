#include "pch.h"
#include "TextRenderComponent.h"
#include "Shader.h"
#include "StaticMesh.h"
#include "Quad.h"
#include "StaticMeshComponent.h"
#include "BillboardComponent.h"
#include <Windows.h>


URenderer::URenderer(URHIDevice* InDevice) : RHIDevice(InDevice)
{
	InitializeLineBatch();

	/* // 오클루전 관련 초기화
	CreateDepthOnlyStates();
	CreateUnitCube();
	CreateOcclusionCB();
	*/
	//RHIDevice->OMSetRenderTargets();

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
	// 백버퍼/깊이버퍼를 클리어
	RHIDevice->ClearBackBuffer();  // 배경색
	RHIDevice->ClearDepthBuffer(1.0f, 0);                 // 깊이값 초기화
	//RHIDevice->CreateBlendState();
	RHIDevice->IASetPrimitiveTopology();
	// RS
	RHIDevice->RSSetViewport();

	//OM
	//RHIDevice->OMSetBlendState();
	RHIDevice->OMSetRenderTargets();

	// TODO - 한 종류 메쉬만 스폰했을 때 깨지는 현상 방지 임시이므로 고쳐야합니다
	// ★ 캐시 무효화
	//PreShader = nullptr;
	//PreUMaterial = nullptr; // 이거 주석 처리 시: 피킹하면 그 UStatucjMesh의 텍스쳐가 전부 사라짐
	//PreStaticMesh = nullptr; // 이거 주석 처리 시: 메시가 이상해짐
	//PreViewModeIndex = EViewModeIndex::VMI_Wireframe; // 어차피 SetViewModeType이 다시 셋
}

void URenderer::PrepareShader(FShader& InShader)
{
	RHIDevice->GetDeviceContext()->VSSetShader(InShader.SimpleVertexShader, nullptr, 0);
	RHIDevice->GetDeviceContext()->PSSetShader(InShader.SimplePixelShader, nullptr, 0);
	RHIDevice->GetDeviceContext()->IASetInputLayout(InShader.SimpleInputLayout);
}

void URenderer::PrepareShader(UShader* InShader)
{
	if (PreShader != InShader)
	{
		/*const FString& ShaderFilePath = InShader->GetFilePath();
		UE_LOG("change to new Shader: \'%s\'", ShaderFilePath);*/
		RHIDevice->GetDeviceContext()->VSSetShader(InShader->GetVertexShader(), nullptr, 0);
		RHIDevice->GetDeviceContext()->PSSetShader(InShader->GetPixelShader(), nullptr, 0);
		RHIDevice->GetDeviceContext()->IASetInputLayout(InShader->GetInputLayout());
		PreShader = InShader;
	}
	/*RHIDevice->GetDeviceContext()->VSSetShader(InShader->GetVertexShader(), nullptr, 0);
	RHIDevice->GetDeviceContext()->PSSetShader(InShader->GetPixelShader(), nullptr, 0);
	RHIDevice->GetDeviceContext()->IASetInputLayout(InShader->GetInputLayout());*/
}

void URenderer::OMSetBlendState(bool bIsChecked)
{
	if (bIsChecked == true)
	{
		RHIDevice->OMSetBlendState(true);
	}
	else
	{
		RHIDevice->OMSetBlendState(false);
	}
}

void URenderer::RSSetState(EViewModeIndex ViewModeIndex)
{
	RHIDevice->RSSetState(ViewModeIndex);
}

void URenderer::RSSetNoCullState()
{
	RHIDevice->RSSetNoCullState();
}

void URenderer::UpdateConstantBuffer(const FMatrix& ModelMatrix, const FMatrix& ViewMatrix, const FMatrix& ProjMatrix)
{
	RHIDevice->UpdateConstantBuffers(ModelMatrix, ViewMatrix, ProjMatrix);
}

void URenderer::UpdateHighLightConstantBuffer(const uint32 InPicked, const FVector& InColor, const uint32 X, const uint32 Y, const uint32 Z, const uint32 Gizmo)
{
	RHIDevice->UpdateHighLightConstantBuffers(InPicked, InColor, X, Y, Z, Gizmo);
}

void URenderer::UpdateBillboardConstantBuffers(const FVector& pos, const FMatrix& ViewMatrix, const FMatrix& ProjMatrix, const FVector& CameraRight, const FVector& CameraUp)
{
	RHIDevice->UpdateBillboardConstantBuffers(pos, ViewMatrix, ProjMatrix, CameraRight, CameraUp);
}

void URenderer::UpdatePixelConstantBuffers(const FObjMaterialInfo& InMaterialInfo, bool bHasMaterial, bool bHasTexture)
{
	RHIDevice->UpdatePixelConstantBuffers(InMaterialInfo, bHasMaterial, bHasTexture);
}

void URenderer::UpdateColorBuffer(const FVector4& Color)
{
	RHIDevice->UpdateColorConstantBuffers(Color);
}

void URenderer::UpdateUVScroll(const FVector2D& Speed, float TimeSec)
{
	RHIDevice->UpdateUVScrollConstantBuffers(Speed, TimeSec);
}

void URenderer::DrawIndexedPrimitiveComponent(UStaticMesh* InMesh, D3D11_PRIMITIVE_TOPOLOGY InTopology, const TArray<FMaterialSlot>& InComponentMaterialSlots)
{
	UINT stride = 0;
	switch (InMesh->GetVertexType())
	{
	case EVertexLayoutType::PositionColor:
		stride = sizeof(FVertexSimple);
		break;
	case EVertexLayoutType::PositionColorTexturNormal:
		stride = sizeof(FVertexDynamic);
		break;
	case EVertexLayoutType::PositionTextBillBoard:
		stride = sizeof(FBillboardVertexInfo_GPU);
		break;
	case EVertexLayoutType::PositionBillBoard:
		stride = sizeof(FBillboardVertex);
		break;
	default:
		// Handle unknown or unsupported vertex types
		assert(false && "Unknown vertex type!");
		return; // or log an error
	}
	UINT offset = 0;

	ID3D11Buffer* VertexBuffer = InMesh->GetVertexBuffer();
	ID3D11Buffer* IndexBuffer = InMesh->GetIndexBuffer();
	uint32 VertexCount = InMesh->GetVertexCount();
	uint32 IndexCount = InMesh->GetIndexCount();

	RHIDevice->GetDeviceContext()->IASetVertexBuffers(
		0, 1, &VertexBuffer, &stride, &offset
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
			const FObjMaterialInfo& MaterialInfo = Material->GetMaterialInfo();
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
			RHIDevice->UpdatePixelConstantBuffers(MaterialInfo, true, bHasTexture); // 성공 여부 기반
			RHIDevice->GetDeviceContext()->DrawIndexed(MeshGroupInfos[i].IndexCount, MeshGroupInfos[i].StartIndex, 0);
		}
	}
	else
	{
		FObjMaterialInfo ObjMaterialInfo;
		RHIDevice->UpdatePixelConstantBuffers(ObjMaterialInfo, false, false); // PSSet도 해줌
		RHIDevice->GetDeviceContext()->DrawIndexed(IndexCount, 0, 0);
	}
}

// TEXT BillBoard 용 
void URenderer::DrawIndexedPrimitiveComponent(UTextRenderComponent* Comp, D3D11_PRIMITIVE_TOPOLOGY InTopology)
{
	UINT Stride = sizeof(FBillboardVertexInfo_GPU);
	ID3D11Buffer* VertexBuff = Comp->GetStaticMesh()->GetVertexBuffer();
	ID3D11Buffer* IndexBuff = Comp->GetStaticMesh()->GetIndexBuffer();

	RHIDevice->GetDeviceContext()->IASetInputLayout(Comp->GetMaterial()->GetShader()->GetInputLayout());

	UINT offset = 0;
	RHIDevice->GetDeviceContext()->IASetVertexBuffers(
		0, 1, &VertexBuff, &Stride, &offset
	);
	RHIDevice->GetDeviceContext()->IASetIndexBuffer(
		IndexBuff, DXGI_FORMAT_R32_UINT, 0
	);
	ID3D11ShaderResourceView* TextureSRV = Comp->GetMaterial()->GetTexture()->GetShaderResourceView();
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
	RHIDevice->GetDeviceContext()->IASetInputLayout(Comp->GetMaterial()->GetShader()->GetInputLayout());

	UINT offset = 0;
	RHIDevice->GetDeviceContext()->IASetVertexBuffers(0, 1, &VertexBuff, &Stride, &offset);
	RHIDevice->GetDeviceContext()->IASetIndexBuffer(IndexBuff, DXGI_FORMAT_R32_UINT, 0);

    // Bind texture via ResourceManager to support DDS/PNG
    ID3D11ShaderResourceView* srv = nullptr;
    if (Comp->GetMaterial())
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
    OMSetBlendState(true);

	RHIDevice->GetDeviceContext()->IASetPrimitiveTopology(InTopology);
	const uint32 indexCnt = Comp->GetStaticMesh()->GetIndexCount();
	RHIDevice->GetDeviceContext()->DrawIndexed(indexCnt, 0, 0);

    // Restore blend state so others aren't affected
    OMSetBlendState(false);
}
void URenderer::SetViewModeType(EViewModeIndex ViewModeIndex)
{
	if (PreViewModeIndex != ViewModeIndex)
	{
		//UE_LOG("Change ViewMode");
		RHIDevice->RSSetState(ViewModeIndex);
		if (ViewModeIndex == EViewModeIndex::VMI_Wireframe)
			RHIDevice->UpdateColorConstantBuffers(FVector4{ 1.f, 0.f, 0.f, 1.f });
		else
			RHIDevice->UpdateColorConstantBuffers(FVector4{ 1.f, 1.f, 1.f, 0.f });
		PreViewModeIndex = ViewModeIndex;
	}
}

void URenderer::EndFrame()
{

	RHIDevice->Present();
}

void URenderer::OMSetDepthStencilState(EComparisonFunc Func)
{
	RHIDevice->OmSetDepthStencilState(Func);
}

void URenderer::OMSetDepthStencilStateOverlayWriteStencil()
{
    RHIDevice->OMSetDepthStencilState_OverlayWriteStencil();
}

void URenderer::OMSetDepthStencilStateStencilRejectOverlay()
{
    RHIDevice->OMSetDepthStencilState_StencilRejectOverlay();
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
	LineShader = UResourceManager::GetInstance().Load<UShader>("ShaderLine.hlsl", EVertexLayoutType::PositionColor);
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
    UpdateConstantBuffer(ModelMatrix, ViewMatrix, ProjectionMatrix);
    PrepareShader(LineShader);
    
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
        OMSetDepthStencilStateStencilRejectOverlay();
        RHIDevice->GetDeviceContext()->DrawIndexed(DynamicLineMesh->GetCurrentIndexCount(), 0, 0);
        // 상태 복구
        OMSetDepthStencilState(EComparisonFunc::LessEqual);
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
