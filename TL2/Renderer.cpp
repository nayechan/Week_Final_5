#include "pch.h"
#include "TextRenderComponent.h"
#include "Shader.h"
#include "StaticMesh.h"
#include "TextQuad.h"
#include "StaticMeshComponent.h"


URenderer::URenderer(URHIDevice* InDevice) : RHIDevice(InDevice)
{
    InitializeLineBatch();

	/* // 오클루전 관련 초기화
    CreateDepthOnlyStates();
    CreateUnitCube();
    CreateOcclusionCB();
    */
}

URenderer::~URenderer()
{
    if (LineBatchData)
    {
        delete LineBatchData;
    }
	// 오클루전 관련 해제
    /*
    if (DepthLEqual) 
    { 
        DepthLEqual->Release();  
        DepthLEqual = nullptr; 
    }
    if (RS_Solid) 
    {
        RS_Solid->Release();    
        RS_Solid = nullptr;
    }
    if (ColorMaskOff) 
    {
        ColorMaskOff->Release(); 
        ColorMaskOff = nullptr; 
    }
    if (UnitCubeVB) 
    { 
        UnitCubeVB->Release(); 
        UnitCubeVB = nullptr;
    }
    if (UnitCubeIB)
    { 
        UnitCubeIB->Release();  
        UnitCubeIB = nullptr; 
    }
    if (OcclusionCB)
    { 
        OcclusionCB->Release(); 
        OcclusionCB = nullptr; 
    }

    if (DepthLEqualNoWrite) 
    { 
        DepthLEqualNoWrite->Release();
        DepthLEqualNoWrite = nullptr; 
    }
    if (DepthOnlyIL) 
    { 
        DepthOnlyIL->Release();
        DepthOnlyIL = nullptr; 
    }
    */
}

void URenderer::BeginFrame()
{
    // 백버퍼/깊이버퍼를 클리어
    RHIDevice->ClearBackBuffer();  // 배경색
    RHIDevice->ClearDepthBuffer(1.0f, 0);                 // 깊이값 초기화
    RHIDevice->CreateBlendState();
    RHIDevice->IASetPrimitiveTopology();
    // RS
    RHIDevice->RSSetViewport();

    //OM
    //RHIDevice->OMSetBlendState();
    RHIDevice->OMSetRenderTargets();
}

void URenderer::PrepareShader(FShader& InShader)
{
    RHIDevice->GetDeviceContext()->VSSetShader(InShader.SimpleVertexShader, nullptr, 0);
    RHIDevice->GetDeviceContext()->PSSetShader(InShader.SimplePixelShader, nullptr, 0);
    RHIDevice->GetDeviceContext()->IASetInputLayout(InShader.SimpleInputLayout);
}

void URenderer::PrepareShader(UShader* InShader)
{
    RHIDevice->GetDeviceContext()->VSSetShader(InShader->GetVertexShader(), nullptr, 0);
    RHIDevice->GetDeviceContext()->PSSetShader(InShader->GetPixelShader(), nullptr, 0);
    RHIDevice->GetDeviceContext()->IASetInputLayout(InShader->GetInputLayout());
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

void URenderer::UpdateConstantBuffer(const FMatrix& ModelMatrix, const FMatrix& ViewMatrix, const FMatrix& ProjMatrix)
{
    RHIDevice->UpdateConstantBuffers(ModelMatrix, ViewMatrix, ProjMatrix);
}

void URenderer::UpdateHighLightConstantBuffer(const uint32 InPicked, const FVector& InColor, const uint32 X, const uint32 Y, const uint32 Z, const uint32 Gizmo)
{
    RHIDevice->UpdateHighLightConstantBuffers(InPicked, InColor, X, Y, Z, Gizmo);
}

void URenderer::UpdateBillboardConstantBuffers(const FVector& pos,const FMatrix& ViewMatrix, const FMatrix& ProjMatrix, const FVector& CameraRight, const FVector& CameraUp)
{
    RHIDevice->UpdateBillboardConstantBuffers(pos,ViewMatrix, ProjMatrix, CameraRight, CameraUp);
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
    case EVertexLayoutType::PositionBillBoard:
        stride = sizeof(FBillboardVertexInfo_GPU);
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
            const UMaterial* const Material = UResourceManager::GetInstance().Get<UMaterial>(InComponentMaterialSlots[i].MaterialName);
            const FObjMaterialInfo& MaterialInfo = Material->GetMaterialInfo();
            bool bHasTexture = !(MaterialInfo.DiffuseTextureFileName.empty());
            if (bHasTexture)
            {
                FWideString WTextureFileName(MaterialInfo.DiffuseTextureFileName.begin(), MaterialInfo.DiffuseTextureFileName.end()); // 단순 ascii라고 가정
                FTextureData* TextureData = UResourceManager::GetInstance().CreateOrGetTextureData(WTextureFileName);
                RHIDevice->GetDeviceContext()->PSSetShaderResources(0, 1, &(TextureData->TextureSRV));
            }
            RHIDevice->UpdatePixelConstantBuffers(MaterialInfo, true, bHasTexture); // PSSet도 해줌
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
    RHIDevice->GetDeviceContext()->DrawIndexed(Comp->GetStaticMesh()->GetIndexCount(), 0, 0);
}

void URenderer::SetViewModeType(EViewModeIndex ViewModeIndex)
{
    RHIDevice->RSSetState(ViewModeIndex);
    if(ViewModeIndex == EViewModeIndex::VMI_Wireframe)
        RHIDevice->UpdateColorConstantBuffers(FVector4{ 1.f, 0.f, 0.f, 1.f });
    else
        RHIDevice->UpdateColorConstantBuffers(FVector4{ 1.f, 1.f, 1.f, 0.f });
}

void URenderer::EndFrame()
{
    
    RHIDevice->Present();
}

void URenderer::OMSetDepthStencilState(EComparisonFunc Func)
{
    RHIDevice->OmSetDepthStencilState(Func);
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
        RHIDevice->GetDeviceContext()->DrawIndexed(DynamicLineMesh->GetCurrentIndexCount(), 0, 0);
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


// ========================== 오클루전 관련 메소드 ==========================
// 상태/버퍼 생성
/*
void URenderer::CreateDepthOnlyStates()
{
    ID3D11Device* Dev = RHIDevice->GetDevice();

    // 기존: 쓰기 ON (다른 곳에서 쓸 수 있음)
    {
        D3D11_DEPTH_STENCIL_DESC dsd = {};
        dsd.DepthEnable = TRUE;
        dsd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;   // ← ON
        dsd.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
        Dev->CreateDepthStencilState(&dsd, &DepthLEqual);
    }

    // ★ 오클루전 전용: 쓰기 OFF
    {
        D3D11_DEPTH_STENCIL_DESC dsd = {};
        dsd.DepthEnable = TRUE;
        dsd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;  // ← OFF (핵심)
        dsd.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
        Dev->CreateDepthStencilState(&dsd, &DepthLEqualNoWrite);
    }
    D3D11_RASTERIZER_DESC rsd = {};
    rsd.FillMode = D3D11_FILL_SOLID;
    rsd.CullMode = D3D11_CULL_BACK;
    rsd.DepthClipEnable = TRUE;
    Dev->CreateRasterizerState(&rsd, &RS_Solid);

    D3D11_BLEND_DESC bd = {};
    bd.RenderTarget[0].BlendEnable = FALSE;
    bd.RenderTarget[0].RenderTargetWriteMask = 0; // color write off
    Dev->CreateBlendState(&bd, &ColorMaskOff);
}

void URenderer::CreateUnitCube()
{
    struct V { float x, y, z; };
    const V Verts[8] = {
        {-1,-1,-1},{+1,-1,-1},{+1,+1,-1},{-1,+1,-1},
        {-1,-1,+1},{+1,-1,+1},{+1,+1,+1},{-1,+1,+1},
    };
    const uint32 Idx[36] = {
        0,1,2, 0,2,3,
        4,6,5, 4,7,6,
        4,5,1, 4,1,0,
        3,2,6, 3,6,7,
        4,0,3, 4,3,7,
        1,5,6, 1,6,2
    };

    ID3D11Device* Dev = RHIDevice->GetDevice();

    D3D11_BUFFER_DESC vb = {};
    vb.ByteWidth = sizeof(Verts);
    vb.Usage = D3D11_USAGE_IMMUTABLE;
    vb.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    D3D11_SUBRESOURCE_DATA sdv = { Verts };
    Dev->CreateBuffer(&vb, &sdv, &UnitCubeVB);

    D3D11_BUFFER_DESC ib = {};
    ib.ByteWidth = sizeof(Idx);
    ib.Usage = D3D11_USAGE_IMMUTABLE;
    ib.BindFlags = D3D11_BIND_INDEX_BUFFER;
    D3D11_SUBRESOURCE_DATA sdi = { Idx };
    Dev->CreateBuffer(&ib, &sdi, &UnitCubeIB);
}

void URenderer::CreateOcclusionCB()
{
    D3D11_BUFFER_DESC bd = {};
    bd.ByteWidth = sizeof(float) * 16 * 3; // gModel/gView/gProj
    bd.Usage = D3D11_USAGE_DYNAMIC;
    bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    RHIDevice->GetDevice()->CreateBuffer(&bd, nullptr, &OcclusionCB);
}

void URenderer::BeginDepthOnly()
{
    auto* Ctx = RHIDevice->GetDeviceContext();
    float factor[4] = { 0,0,0,0 };

    Ctx->OMSetBlendState(ColorMaskOff, factor, 0xffffffff);
    Ctx->OMSetDepthStencilState(DepthLEqualNoWrite, 0);  // ★ 쓰기 OFF 상태 사용
    Ctx->RSSetState(RS_Solid);

    if (DepthOnlyIL) Ctx->IASetInputLayout(DepthOnlyIL);  // ★ IL 바인딩
    Ctx->VSSetShader(DepthOnlyVS, nullptr, 0);
    Ctx->PSSetShader(DepthOnlyPS, nullptr, 0);
}

void URenderer::EndDepthOnly()
{
    auto* Ctx = RHIDevice->GetDeviceContext();

    // ★★ 상태 원복을 확실히 ★★
    Ctx->IASetInputLayout(nullptr);           // 레이아웃 클리어
    Ctx->VSSetShader(nullptr, nullptr, 0);    // 셰이더 언바인드
    Ctx->PSSetShader(nullptr, nullptr, 0);

    Ctx->OMSetBlendState(nullptr, nullptr, 0xffffffff);
    Ctx->OMSetDepthStencilState(nullptr, 0);  // 기본으로 되돌림
    Ctx->RSSetState(nullptr);
}

void URenderer::SetPredication(ID3D11Predicate* Pred, BOOL OpEqualTrue)
{
    RHIDevice->GetDeviceContext()->SetPredication(Pred, OpEqualTrue);
}

void URenderer::SetDepthOnlyShaders(ID3D11VertexShader* VS, ID3D11PixelShader* PS)
{
    DepthOnlyVS = VS;
    DepthOnlyPS = PS;
}

ID3D11Device* URenderer::GetDevice() { return RHIDevice->GetDevice(); }
ID3D11DeviceContext* URenderer::GetDeviceContext() { return RHIDevice->GetDeviceContext(); }

static inline void CopyMatrixRowMajor(const FMatrix& M, float* Out16)
{
    for (int r = 0; r < 4; ++r)
        for (int c = 0; c < 4; ++c)
            Out16[r * 4 + c] = M.M[r][c];
}

void URenderer::DrawOcclusionBox(const FBound& B, const FMatrix& View, const FMatrix& Proj)
{
    const FVector C = (B.Min + B.Max) * 0.5f;
    const FVector E = (B.Max - B.Min) * 0.5f;

    FMatrix S = FMatrix::MakeScale(E);
    FMatrix T = FMatrix::MakeTranslation(C);
    FMatrix Model = S * T; // row-vector, Z-up

    D3D11_MAPPED_SUBRESOURCE map;
    auto* Ctx = RHIDevice->GetDeviceContext();
    if (SUCCEEDED(Ctx->Map(OcclusionCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &map)))
    {
        float* Ptr = reinterpret_cast<float*>(map.pData);
        CopyMatrixRowMajor(Model, Ptr + 16 * 0);
        CopyMatrixRowMajor(View, Ptr + 16 * 1);
        CopyMatrixRowMajor(Proj, Ptr + 16 * 2);
        Ctx->Unmap(OcclusionCB, 0);
    }
    Ctx->VSSetConstantBuffers(0, 1, &OcclusionCB);

    UINT stride = sizeof(float) * 3, offset = 0;
    Ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    Ctx->IASetVertexBuffers(0, 1, &UnitCubeVB, &stride, &offset);
    Ctx->IASetIndexBuffer(UnitCubeIB, DXGI_FORMAT_R32_UINT, 0);

    Ctx->DrawIndexed(36, 0, 0);
}

void URenderer::SetDepthOnlyInputLayout(ID3D11InputLayout* IL)
{
    DepthOnlyIL = IL;
}
*/