#include "pch.h"
#include "UI/StatsOverlayD2D.h"

struct FConstants
{
    FVector WorldPosition;
    float Scale;
};
// b0 in VS
struct ModelBufferType
{
    FMatrix Model;
};

// b0 in PS
struct FMaterialInPs
{
    FVector DiffuseColor; // Kd
    float OpticalDensity; // Ni

    FVector AmbientColor; // Ka
    float Transparency; // Tr Or d

    FVector SpecularColor; // Ks
    float SpecularExponent; // Ns

    FVector EmissiveColor; // Ke
    uint32 IlluminationModel; // illum. Default illumination model to Phong for non-Pbr materials

    FVector TransmissionFilter; // Tf
    float dummy; // 4 bytes padding
};

struct FPixelConstBufferType
{
    FMaterialInPs Material;
    bool bHasMaterial; // 1 bytes
    bool Dummy[3]; // 3 bytes padding
    bool bHasTexture; // 1 bytes
    bool Dummy2[11]; // 11 bytes padding
};

static_assert(sizeof(FPixelConstBufferType) % 16 == 0, "PixelConstData size mismatch!");

// b1
struct ViewProjBufferType
{
    FMatrix View;
    FMatrix Proj;
};

// b2
struct HighLightBufferType
{
    uint32 Picked;
    FVector Color;
    uint32 X;
    uint32 Y;
    uint32 Z;
    uint32 Gizmo;
};

struct ColorBufferType
{
    FVector4 Color;
};


struct BillboardBufferType
{
    FVector pos;
    FMatrix View;
    FMatrix Proj;
    FMatrix InverseViewMat;
    /*FVector cameraRight;
    FVector cameraUp;*/
};

void D3D11RHI::Initialize(HWND hWindow)
{
    // 이곳에서 Device, DeviceContext, viewport, swapchain를 초기화한다
    CreateDeviceAndSwapChain(hWindow);
    CreateFrameBuffer();
    CreateRasterizerState();
    CreateBlendState();
    CreateConstantBuffer();
	CreateDepthStencilState();
	CreateSamplerState();
    UResourceManager::GetInstance().Initialize(Device,DeviceContext);

    // Initialize Direct2D overlay after device/swapchain ready
    UStatsOverlayD2D::Get().Initialize(Device, DeviceContext, SwapChain);
}

void D3D11RHI::Release()
{
    if (DeviceContext)
    {
        // 파이프라인에서 바인딩된 상태/리소스를 명시적으로 해제
        DeviceContext->OMSetRenderTargets(0, nullptr, nullptr);
        DeviceContext->OMSetDepthStencilState(nullptr, 0);
        DeviceContext->RSSetState(nullptr);
        DeviceContext->OMSetBlendState(nullptr, nullptr, 0xffffffff);

        DeviceContext->ClearState();
        DeviceContext->Flush();
    }

    ReleaseSamplerState();

    // 상수버퍼
    if (HighLightCB) { HighLightCB->Release(); HighLightCB = nullptr; }
    if (ModelCB) { ModelCB->Release(); ModelCB = nullptr; }
    if (ColorCB) { ColorCB->Release(); ColorCB = nullptr; }
    if (ViewProjCB) { ViewProjCB->Release(); ViewProjCB = nullptr; }
    if (BillboardCB) { BillboardCB->Release(); BillboardCB = nullptr; }
    if (PixelConstCB) { PixelConstCB->Release(); PixelConstCB = nullptr; }
    if (UVScrollCB) { UVScrollCB->Release(); UVScrollCB = nullptr; }
    if (ConstantBuffer) { ConstantBuffer->Release(); ConstantBuffer = nullptr; }

    // 상태 객체
    if (DepthStencilState) { DepthStencilState->Release(); DepthStencilState = nullptr; }
    if (DepthStencilStateLessEqualWrite) { DepthStencilStateLessEqualWrite->Release(); DepthStencilStateLessEqualWrite = nullptr; }
    if (DepthStencilStateLessEqualReadOnly) { DepthStencilStateLessEqualReadOnly->Release(); DepthStencilStateLessEqualReadOnly = nullptr; }
    if (DepthStencilStateAlwaysNoWrite) { DepthStencilStateAlwaysNoWrite->Release(); DepthStencilStateAlwaysNoWrite = nullptr; }
    if (DepthStencilStateDisable) { DepthStencilStateDisable->Release(); DepthStencilStateDisable = nullptr; }
    if (DepthStencilStateGreaterEqualWrite) { DepthStencilStateGreaterEqualWrite->Release(); DepthStencilStateGreaterEqualWrite = nullptr; }
    if (DepthStencilStateOverlayWriteStencil) { DepthStencilStateOverlayWriteStencil->Release(); DepthStencilStateOverlayWriteStencil = nullptr; }
    if (DepthStencilStateStencilRejectOverlay) { DepthStencilStateStencilRejectOverlay->Release(); DepthStencilStateStencilRejectOverlay = nullptr; }

    if (DefaultRasterizerState) { DefaultRasterizerState->Release();   DefaultRasterizerState = nullptr; }
    if (WireFrameRasterizerState) { WireFrameRasterizerState->Release();   WireFrameRasterizerState = nullptr; }
    if (NoCullRasterizerState) { NoCullRasterizerState->Release();   NoCullRasterizerState = nullptr; }
    if (BlendState) { BlendState->Release();        BlendState = nullptr; }

    // RTV/DSV/FrameBuffer
    ReleaseFrameBuffer();

    // Device + SwapChain
    ReleaseDeviceAndSwapChain();
}

void D3D11RHI::ClearBackBuffer()
{
    float ClearColor[4] = { 0.025f, 0.025f, 0.025f, 1.0f };
    DeviceContext->ClearRenderTargetView(RenderTargetView, ClearColor);
}

void D3D11RHI::ClearDepthBuffer(float Depth, UINT Stencil)
{
    DeviceContext->ClearDepthStencilView(DepthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, Depth, Stencil);

}

void D3D11RHI::CreateBlendState()
{
    // Create once; reuse every frame
    if (BlendState)
        return;

    D3D11_BLEND_DESC bd = {};
    auto& rt = bd.RenderTarget[0];
    rt.BlendEnable = TRUE;
    rt.SrcBlend = D3D11_BLEND_SRC_ALPHA;      // 스트레이트 알파
    rt.DestBlend = D3D11_BLEND_INV_SRC_ALPHA;  // (프리멀티면 ONE / INV_SRC_ALPHA)
    rt.BlendOp = D3D11_BLEND_OP_ADD;
    rt.SrcBlendAlpha = D3D11_BLEND_ONE;
    rt.DestBlendAlpha = D3D11_BLEND_ZERO;
    rt.BlendOpAlpha = D3D11_BLEND_OP_ADD;
    rt.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    Device->CreateBlendState(&bd, &BlendState);
}

void D3D11RHI::CreateDepthStencilState()
{
    D3D11_DEPTH_STENCIL_DESC desc = {};
    desc.StencilEnable = FALSE;

    // 1) 기본: LessEqual + Write ALL
    desc.DepthEnable = TRUE;
    desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    desc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
    Device->CreateDepthStencilState(&desc, &DepthStencilStateLessEqualWrite);

    // 2) ReadOnly: LessEqual + Write ZERO
    desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    Device->CreateDepthStencilState(&desc, &DepthStencilStateLessEqualReadOnly);

    // 3) AlwaysNoWrite: Always + Write ZERO (기즈모/오버레이 용)
    desc.DepthFunc = D3D11_COMPARISON_ALWAYS;
    // DepthEnable은 TRUE 유지 (읽기 의미는 없지만 상태 일관성을 위해)
    Device->CreateDepthStencilState(&desc, &DepthStencilStateAlwaysNoWrite);

    // 4) Disable: DepthEnable FALSE (테스트/쓰기 모두 무시)
    desc.DepthEnable = FALSE;
    // DepthWriteMask/Func는 무시되지만 값은 그대로 둬도 됨
    Device->CreateDepthStencilState(&desc, &DepthStencilStateDisable);

    // 5) (선택) GreaterEqual + Write ALL
    desc.DepthEnable = TRUE;
    desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    desc.DepthFunc = D3D11_COMPARISON_GREATER_EQUAL;
    Device->CreateDepthStencilState(&desc, &DepthStencilStateGreaterEqualWrite);

    // 6) OverlayWriteStencil: Always + NoWriteDepth + Stencil=REPLACE 1
    ZeroMemory(&desc, sizeof(desc));
    desc.DepthEnable = TRUE;
    desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    desc.DepthFunc = D3D11_COMPARISON_ALWAYS;
    desc.StencilEnable = TRUE;
    desc.StencilReadMask = 0xFF;
    desc.StencilWriteMask = 0xFF;
    desc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
    desc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
    desc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_REPLACE; // write ref
    desc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
    desc.BackFace = desc.FrontFace;
    Device->CreateDepthStencilState(&desc, &DepthStencilStateOverlayWriteStencil);

    // 7) StencilRejectOverlay: LessEqual + DepthWrite ALL + Stencil EQUAL 0 (keep)
    ZeroMemory(&desc, sizeof(desc));
    desc.DepthEnable = TRUE;
    desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    desc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
    desc.StencilEnable = TRUE;
    desc.StencilReadMask = 0xFF;
    desc.StencilWriteMask = 0x00; // no stencil write
    desc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
    desc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
    desc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
    desc.FrontFace.StencilFunc = D3D11_COMPARISON_EQUAL; // pass only when stencil==ref
    desc.BackFace = desc.FrontFace;
    Device->CreateDepthStencilState(&desc, &DepthStencilStateStencilRejectOverlay);
}

void D3D11RHI::CreateSamplerState()
{
    D3D11_SAMPLER_DESC SampleDesc = {};
    SampleDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    SampleDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
    SampleDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
    SampleDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    SampleDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    SampleDesc.MinLOD = 0;
    SampleDesc.MaxLOD = D3D11_FLOAT32_MAX;

	HRESULT HR = Device->CreateSamplerState(&SampleDesc, &DefaultSamplerState);
}

HRESULT D3D11RHI::CreateIndexBuffer(ID3D11Device* device, const FMeshData* meshData, ID3D11Buffer** outBuffer)
{
    if (!meshData || meshData->Indices.empty())
        return E_FAIL;

    D3D11_BUFFER_DESC ibd = {};
    ibd.Usage = D3D11_USAGE_DEFAULT;
    ibd.ByteWidth = static_cast<UINT>(sizeof(uint32) * meshData->Indices.size());
    ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;
    ibd.CPUAccessFlags = 0;

    D3D11_SUBRESOURCE_DATA iinitData = {};
    iinitData.pSysMem = meshData->Indices.data();

    return device->CreateBuffer(&ibd, &iinitData, outBuffer);
}

HRESULT D3D11RHI::CreateIndexBuffer(ID3D11Device* device, const FStaticMesh* mesh, ID3D11Buffer** outBuffer)
{
    if (!mesh || mesh->Indices.empty())
        return E_FAIL;

    D3D11_BUFFER_DESC ibd = {};
    ibd.Usage = D3D11_USAGE_DEFAULT;
    ibd.ByteWidth = static_cast<UINT>(sizeof(uint32) * mesh->Indices.size());
    ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;
    ibd.CPUAccessFlags = 0;

    D3D11_SUBRESOURCE_DATA iinitData = {};
    iinitData.pSysMem = mesh->Indices.data();

    return device->CreateBuffer(&ibd, &iinitData, outBuffer);
}

//이거 두개를 나눔
void D3D11RHI::UpdateConstantBuffers(const FMatrix& ModelMatrix, const FMatrix& ViewMatrix, const FMatrix& ProjMatrix)
{
    UpdateModelBuffer(ModelMatrix);
    UpdateViewProjectionBuffers(ViewMatrix, ProjMatrix);
}

// 뷰/프로젝션은 프레임당 한 번만 업데이트
void D3D11RHI::UpdateViewProjectionBuffers(const FMatrix& ViewMatrix, const FMatrix& ProjMatrix)
{
    static FMatrix LastViewMatrix;
    static FMatrix LastProjMatrix;
    static bool bFirstTime = true;
    
    // 뷰/프로젝션이 변경되었을 때만 업데이트
    if (bFirstTime || ViewMatrix != LastViewMatrix || ProjMatrix != LastProjMatrix)
    {
        D3D11_MAPPED_SUBRESOURCE mapped;
        DeviceContext->Map(ViewProjCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
        auto* dataPtr = reinterpret_cast<ViewProjBufferType*>(mapped.pData);

        dataPtr->View = ViewMatrix;
        dataPtr->Proj = ProjMatrix;

        DeviceContext->Unmap(ViewProjCB, 0);
        DeviceContext->VSSetConstantBuffers(1, 1, &ViewProjCB); // b1 슬롯
        
        LastViewMatrix = ViewMatrix;
        LastProjMatrix = ProjMatrix;
        bFirstTime = false;
    }
}

// 모델 행렬만 업데이트
void D3D11RHI::UpdateModelBuffer(const FMatrix& ModelMatrix)
{
    D3D11_MAPPED_SUBRESOURCE mapped;
    DeviceContext->Map(ModelCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    auto* dataPtr = reinterpret_cast<ModelBufferType*>(mapped.pData);

    dataPtr->Model = ModelMatrix;

    DeviceContext->Unmap(ModelCB, 0);
    DeviceContext->VSSetConstantBuffers(0, 1, &ModelCB); // b0 슬롯
}

void D3D11RHI::UpdateBillboardConstantBuffers(const FVector& pos, const FMatrix& ViewMatrix, const FMatrix& ProjMatrix,
    const FVector& CameraRight, const FVector& CameraUp)
{
    D3D11_MAPPED_SUBRESOURCE mapped;
    DeviceContext->Map(BillboardCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    auto* dataPtr = reinterpret_cast<BillboardBufferType*>(mapped.pData);

    // HLSL 기본 row-major와 맞추기 위해 전치
    dataPtr->pos = pos;
    dataPtr->View = ViewMatrix;
    dataPtr->Proj = ProjMatrix;

    // TODO 09/27 11:44 (Dongmin) - 문제 생기면 InverseAffine()으로 수정
	// 현재 빌보드 안보이게 해둬서 테스트 못함
    dataPtr->InverseViewMat = ViewMatrix.InverseAffineFast();
    //dataPtr->cameraRight = CameraRight;
    //dataPtr->cameraUp = CameraUp;

    DeviceContext->Unmap(BillboardCB, 0);
    DeviceContext->VSSetConstantBuffers(0, 1, &BillboardCB); // b0 슬롯
}

void D3D11RHI::UpdatePixelConstantBuffers(const FObjMaterialInfo& InMaterialInfo, bool bHasMaterial, bool bHasTexture)
{
    D3D11_MAPPED_SUBRESOURCE mapped;
    DeviceContext->Map(PixelConstCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    FPixelConstBufferType* dataPtr = reinterpret_cast<FPixelConstBufferType*>(mapped.pData);

    // 이후 다양한 material들이 맵핑될 수도 있음.
    dataPtr->bHasMaterial = bHasMaterial;
    dataPtr->bHasTexture = bHasTexture;
    dataPtr->Material.DiffuseColor = InMaterialInfo.DiffuseColor;
    dataPtr->Material.AmbientColor = InMaterialInfo.AmbientColor;

    DeviceContext->Unmap(PixelConstCB, 0);
    DeviceContext->PSSetConstantBuffers(4, 1, &PixelConstCB); // b4 슬롯
}

void D3D11RHI::UpdateHighLightConstantBuffers(const uint32 InPicked, const FVector& InColor, const uint32 X, const uint32 Y, const uint32 Z, const uint32 Gizmo)
{
    // b2 : 색 강조
    {
        D3D11_MAPPED_SUBRESOURCE mapped;
        DeviceContext->Map(HighLightCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
        auto* dataPtr = reinterpret_cast<HighLightBufferType*>(mapped.pData);

        dataPtr->Picked = InPicked;
        dataPtr->Color = InColor;
        dataPtr->X = X;
        dataPtr->Y = Y;
        dataPtr->Z = Z;
        dataPtr->Gizmo = Gizmo;
        DeviceContext->Unmap(HighLightCB, 0);
        DeviceContext->VSSetConstantBuffers(2, 1, &HighLightCB); // b2 슬롯
    }
}

void D3D11RHI::UpdateColorConstantBuffers(const FVector4& InColor)
{
    // b3 : 색 설정
    {
        D3D11_MAPPED_SUBRESOURCE mapped;
        DeviceContext->Map(ColorCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
        auto* dataPtr = reinterpret_cast<ColorBufferType*>(mapped.pData);
        {
            dataPtr->Color = InColor;
        }
        DeviceContext->Unmap(ColorCB, 0);
        DeviceContext->PSSetConstantBuffers(3, 1, &ColorCB); // b3 슬롯
    }
}

void D3D11RHI::IASetPrimitiveTopology()
{
    DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

void D3D11RHI::RSSetState(EViewModeIndex ViewModeIndex)
{
    if (ViewModeIndex == EViewModeIndex::VMI_Wireframe)
    {
        DeviceContext->RSSetState(WireFrameRasterizerState);
    }
    else
    {
        DeviceContext->RSSetState(DefaultRasterizerState);
    }
}

void D3D11RHI::RSSetNoCullState()
{
    DeviceContext->RSSetState(NoCullRasterizerState);
}

void D3D11RHI::RSSetViewport()
{
    DeviceContext->RSSetViewports(1, &ViewportInfo);
}

void D3D11RHI::OMSetRenderTargets()
{
    DeviceContext->OMSetRenderTargets(1, &RenderTargetView, DepthStencilView);
}

void D3D11RHI::OMSetBlendState(bool bIsBlendMode)
{
    if (bIsBlendMode == true)
    {
        float blendFactor[4] = { 0,0,0,0 };
        DeviceContext->OMSetBlendState(BlendState, blendFactor, 0xffffffff);
    }
    else
    {
        DeviceContext->OMSetBlendState(nullptr, nullptr, 0xffffffff);
    }
}

void D3D11RHI::Present()
{
    // Draw any Direct2D overlays before present
    UStatsOverlayD2D::Get().Draw();
    SwapChain->Present(0, 0); // vsync on
}

void D3D11RHI::CreateDeviceAndSwapChain(HWND hWindow)
{
    // 지원하는 Direct3D 기능 레벨을 정의
    D3D_FEATURE_LEVEL featurelevels[] = { D3D_FEATURE_LEVEL_11_0 };

    // 스왑 체인 설정 구조체 초기화
    DXGI_SWAP_CHAIN_DESC swapchaindesc = {};
    swapchaindesc.BufferDesc.Width = 0; // 창 크기에 맞게 자동으로 설정
    swapchaindesc.BufferDesc.Height = 0; // 창 크기에 맞게 자동으로 설정
    swapchaindesc.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM; // 색상 포맷
    swapchaindesc.SampleDesc.Count = 1; // 멀티 샘플링 비활성화
    swapchaindesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; // 렌더 타겟으로 사용
    swapchaindesc.BufferCount = 2; // 더블 버퍼링
    swapchaindesc.OutputWindow = hWindow; // 렌더링할 창 핸들
    swapchaindesc.Windowed = TRUE; // 창 모드
    swapchaindesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD; // 스왑 방식

    // Direct3D 장치와 스왑 체인을 생성
    UINT createDeviceFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#ifdef _DEBUG
    createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    HRESULT hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
        createDeviceFlags,
        featurelevels, ARRAYSIZE(featurelevels), D3D11_SDK_VERSION,
        &swapchaindesc, &SwapChain, &Device, nullptr, &DeviceContext);
    // 생성된 스왑 체인의 정보 가져오기
    SwapChain->GetDesc(&swapchaindesc);

    // 뷰포트 정보 설정
    ViewportInfo = { 0.0f, 0.0f, (float)swapchaindesc.BufferDesc.Width, (float)swapchaindesc.BufferDesc.Height, 0.0f, 1.0f };
}

void D3D11RHI::CreateFrameBuffer()
{
    // 백 버퍼 가져오기
    SwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&FrameBuffer);

    // 렌더 타겟 뷰 생성
    D3D11_RENDER_TARGET_VIEW_DESC framebufferRTVdesc = {};
    framebufferRTVdesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
    framebufferRTVdesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;

    Device->CreateRenderTargetView(FrameBuffer, &framebufferRTVdesc, &RenderTargetView);

    // =====================================
    // 깊이/스텐실 버퍼 생성
    // =====================================
    DXGI_SWAP_CHAIN_DESC swapDesc;
    SwapChain->GetDesc(&swapDesc);

    D3D11_TEXTURE2D_DESC depthDesc = {};
    depthDesc.Width = swapDesc.BufferDesc.Width;
    depthDesc.Height = swapDesc.BufferDesc.Height;
    depthDesc.MipLevels = 1;
    depthDesc.ArraySize = 1;
    depthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT; // 깊이 24비트 + 스텐실 8비트
    depthDesc.SampleDesc.Count = 1;
    depthDesc.Usage = D3D11_USAGE_DEFAULT;
    depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

    ID3D11Texture2D* depthBuffer = nullptr;
    Device->CreateTexture2D(&depthDesc, nullptr, &depthBuffer);

    // DepthStencilView 생성
    D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
    dsvDesc.Format = depthDesc.Format;
    dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
    dsvDesc.Texture2D.MipSlice = 0;

    Device->CreateDepthStencilView(depthBuffer, &dsvDesc, &DepthStencilView);

    depthBuffer->Release(); // 뷰만 참조 유지
}

void D3D11RHI::CreateRasterizerState()
{
    D3D11_RASTERIZER_DESC deafultrasterizerdesc = {};
    deafultrasterizerdesc.FillMode = D3D11_FILL_SOLID; // 채우기 모드
    deafultrasterizerdesc.CullMode = D3D11_CULL_BACK; // 백 페이스 컬링
    deafultrasterizerdesc.DepthClipEnable = TRUE; // 근/원거리 평면 클리핑

    Device->CreateRasterizerState(&deafultrasterizerdesc, &DefaultRasterizerState);

    D3D11_RASTERIZER_DESC wireframerasterizerdesc = {};
    wireframerasterizerdesc.FillMode = D3D11_FILL_WIREFRAME; // 채우기 모드
    wireframerasterizerdesc.CullMode = D3D11_CULL_BACK; // 백 페이스 컬링
    wireframerasterizerdesc.DepthClipEnable = TRUE; // 근/원거리 평면 클리핑

    Device->CreateRasterizerState(&wireframerasterizerdesc, &WireFrameRasterizerState);

    D3D11_RASTERIZER_DESC nocullRasterizerDesc = {};
    nocullRasterizerDesc.FillMode = D3D11_FILL_SOLID;
    nocullRasterizerDesc.CullMode = D3D11_CULL_NONE;
    nocullRasterizerDesc.DepthClipEnable = TRUE;

    Device->CreateRasterizerState(&nocullRasterizerDesc, &NoCullRasterizerState);
}

void D3D11RHI::CreateConstantBuffer()
{
    D3D11_BUFFER_DESC modelDesc = {};
    modelDesc.Usage = D3D11_USAGE_DYNAMIC;
    modelDesc.ByteWidth = sizeof(ModelBufferType);
    modelDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    modelDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    Device->CreateBuffer(&modelDesc, nullptr, &ModelCB);

    // b0 in StaticMeshPS
    D3D11_BUFFER_DESC pixelConstDesc = {};
    pixelConstDesc.Usage = D3D11_USAGE_DYNAMIC;
    pixelConstDesc.ByteWidth = sizeof(FPixelConstBufferType);
    pixelConstDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    pixelConstDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    HRESULT hr = Device->CreateBuffer(&pixelConstDesc, nullptr, &PixelConstCB);
    if (FAILED(hr))
    {
        assert(FAILED(hr));
    }

    // b1 : ViewProjBuffer
    D3D11_BUFFER_DESC vpDesc = {};
    vpDesc.Usage = D3D11_USAGE_DYNAMIC;
    vpDesc.ByteWidth = sizeof(ViewProjBufferType);
    vpDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    vpDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    Device->CreateBuffer(&vpDesc, nullptr, &ViewProjCB);

    // b2 : HighLightBuffer  (← 기존 코드에서 vpDesc를 다시 써서 버그났던 부분)
    D3D11_BUFFER_DESC hlDesc = {};
    hlDesc.Usage = D3D11_USAGE_DYNAMIC;
    hlDesc.ByteWidth = sizeof(HighLightBufferType);
    hlDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    hlDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    Device->CreateBuffer(&hlDesc, nullptr, &HighLightCB);

    // b0
    D3D11_BUFFER_DESC billboardDesc = {};
    billboardDesc.Usage = D3D11_USAGE_DYNAMIC;
    billboardDesc.ByteWidth = sizeof(BillboardBufferType);
    billboardDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    billboardDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    Device->CreateBuffer(&billboardDesc, nullptr, &BillboardCB);

    // b3
    D3D11_BUFFER_DESC ColorDesc = {};
    ColorDesc.Usage = D3D11_USAGE_DYNAMIC;
    ColorDesc.ByteWidth = sizeof(ColorBufferType);
    ColorDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    ColorDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    Device->CreateBuffer(&ColorDesc, nullptr, &ColorCB);

    D3D11_BUFFER_DESC uvScrollDesc = {};
    uvScrollDesc.Usage = D3D11_USAGE_DYNAMIC;
    uvScrollDesc.ByteWidth = sizeof(float) * 4; // float2 speed + float time + pad
    uvScrollDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    uvScrollDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    Device->CreateBuffer(&uvScrollDesc, nullptr, &UVScrollCB);
    if (UVScrollCB)
    {
        D3D11_MAPPED_SUBRESOURCE mapped{};
        if (SUCCEEDED(DeviceContext->Map(UVScrollCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
        {
            float init[4] = { 0,0,0,0 };
            memcpy(mapped.pData, init, sizeof(init));
            DeviceContext->Unmap(UVScrollCB, 0);
        }
        DeviceContext->PSSetConstantBuffers(5, 1, &UVScrollCB);
    }
}

void D3D11RHI::UpdateUVScrollConstantBuffers(const FVector2D& Speed, float TimeSec)
{
    if (!UVScrollCB) return;

    struct { float x; float y; float t; float pad; } data { Speed.X, Speed.Y, TimeSec, 0.0f };

    D3D11_MAPPED_SUBRESOURCE mapped;
    if (SUCCEEDED(DeviceContext->Map(UVScrollCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
    {
        memcpy(mapped.pData, &data, sizeof(data));
        DeviceContext->Unmap(UVScrollCB, 0);
        DeviceContext->PSSetConstantBuffers(5, 1, &UVScrollCB);
    }
}


void D3D11RHI::ReleaseSamplerState()
{
    if (DefaultSamplerState)
    {
        DefaultSamplerState->Release();
        DefaultSamplerState = nullptr;
	}
}

void D3D11RHI::ReleaseBlendState()
{
    if (BlendState)
    {
        BlendState->Release();
        BlendState = nullptr;
    }
}

void D3D11RHI::ReleaseRasterizerState()
{
    if (DefaultRasterizerState)
    {
        DefaultRasterizerState->Release();
        DefaultRasterizerState = nullptr;
    }
    if (WireFrameRasterizerState)
    {
        WireFrameRasterizerState->Release();
        WireFrameRasterizerState = nullptr;
    }
    if (NoCullRasterizerState)
    {
        NoCullRasterizerState->Release();
        NoCullRasterizerState = nullptr;
    }
    DeviceContext->OMSetRenderTargets(0, nullptr, nullptr);
}

void D3D11RHI::ReleaseFrameBuffer()
{
    if (FrameBuffer)
    {
        FrameBuffer->Release();
        FrameBuffer = nullptr;
    }
    if (RenderTargetView)
    {
        RenderTargetView->Release();
        RenderTargetView = nullptr;
    }

    if (DepthStencilView)
    {
        DepthStencilView->Release();
        DepthStencilView = nullptr;
    }
}

void D3D11RHI::ReleaseDeviceAndSwapChain()
{
    if (SwapChain)
    {
        SwapChain->Release();
        SwapChain = nullptr;
    }

    if (DeviceContext)
    {
        DeviceContext->Release();
        DeviceContext = nullptr;
    }

#if defined(_DEBUG)
    ID3D11Debug* pDebug = nullptr;
    if (SUCCEEDED(Device->QueryInterface(__uuidof(ID3D11Debug), (void**)&pDebug)))
    {
        pDebug->ReportLiveDeviceObjects(D3D11_RLDO_DETAIL);
        pDebug->Release();
    }
#endif

    if (Device)
    {
        Device->Release();
        Device = nullptr;
    }

}

void D3D11RHI::OmSetDepthStencilState(EComparisonFunc Func)
{
    switch (Func)
    {
    case EComparisonFunc::Always:
        DeviceContext->OMSetDepthStencilState(DepthStencilStateAlwaysNoWrite, 0);
        break;
    case EComparisonFunc::LessEqual:
        DeviceContext->OMSetDepthStencilState(DepthStencilStateLessEqualWrite, 0);
        break;
    case EComparisonFunc::GreaterEqual:
        DeviceContext->OMSetDepthStencilState(DepthStencilStateGreaterEqualWrite, 0);
        break;
    }
}

void D3D11RHI::OMSetDepthStencilState_OverlayWriteStencil()
{
    // Stencil ref = 1 (overlay marks)
    DeviceContext->OMSetDepthStencilState(DepthStencilStateOverlayWriteStencil, 1);
}

void D3D11RHI::OMSetDepthStencilState_StencilRejectOverlay()
{
    // Stencil ref = 0 (draw only where overlay not marked)
    DeviceContext->OMSetDepthStencilState(DepthStencilStateStencilRejectOverlay, 0);
}

void D3D11RHI::CreateShader(ID3D11InputLayout** SimpleInputLayout, ID3D11VertexShader** SimpleVertexShader, ID3D11PixelShader** SimplePixelShader)
{
    ID3DBlob* vertexshaderCSO;
    ID3DBlob* pixelshaderCSO;

    D3DCompileFromFile(L"ShaderW0.hlsl", nullptr, nullptr, "mainVS", "vs_5_0", 0, 0, &vertexshaderCSO, nullptr);

    Device->CreateVertexShader(vertexshaderCSO->GetBufferPointer(), vertexshaderCSO->GetBufferSize(), nullptr, SimpleVertexShader);

    D3DCompileFromFile(L"ShaderW0.hlsl", nullptr, nullptr, "mainPS", "ps_5_0", 0, 0, &pixelshaderCSO, nullptr);

    Device->CreatePixelShader(pixelshaderCSO->GetBufferPointer(), pixelshaderCSO->GetBufferSize(), nullptr, SimplePixelShader);

    D3D11_INPUT_ELEMENT_DESC layout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };

    Device->CreateInputLayout(layout, ARRAYSIZE(layout), vertexshaderCSO->GetBufferPointer(), vertexshaderCSO->GetBufferSize(), SimpleInputLayout);

    vertexshaderCSO->Release();
    pixelshaderCSO->Release();
}
void D3D11RHI::OnResize(UINT NewWidth, UINT NewHeight)
{
    if (!Device || !DeviceContext || !SwapChain)
        return;

    // 기존 리소스 해제
    ReleaseFrameBuffer();

    // 스왑체인 버퍼 리사이즈
    HRESULT hr = SwapChain->ResizeBuffers(
        0,                 // 버퍼 개수 (0 = 기존 유지)
        NewWidth,
        NewHeight,
        DXGI_FORMAT_UNKNOWN, // 기존 포맷 유지
        0
    );
    if (FAILED(hr))
    {
        UE_LOG("SwapChain->ResizeBuffers failed!\n");
        return;
    }

    // 새 프레임버퍼/RTV/DSV 생성
    CreateFrameBuffer();

    // 뷰포트 갱신
    ViewportInfo.TopLeftX = 0.0f;
    ViewportInfo.TopLeftY = 0.0f;
    ViewportInfo.Width = static_cast<float>(NewWidth);
    ViewportInfo.Height = static_cast<float>(NewHeight);
    ViewportInfo.MinDepth = 0.0f;
    ViewportInfo.MaxDepth = 1.0f;

    DeviceContext->RSSetViewports(1, &ViewportInfo);
}
void D3D11RHI::CreateBackBufferAndDepthStencil(UINT width, UINT height)
{
    // 기존 바인딩 해제 후 뷰 해제
    if (RenderTargetView) { DeviceContext->OMSetRenderTargets(0, nullptr, nullptr); RenderTargetView->Release(); RenderTargetView = nullptr; }
    if (DepthStencilView) { DepthStencilView->Release(); DepthStencilView = nullptr; }

    // 1) 백버퍼에서 RTV 생성
    ID3D11Texture2D* backBuffer = nullptr;
    HRESULT hr = SwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&backBuffer);
    if (FAILED(hr) || !backBuffer) {
        UE_LOG("GetBuffer(0) failed.\n");
        return;
    }

    D3D11_RENDER_TARGET_VIEW_DESC framebufferRTVdesc = {};
    framebufferRTVdesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
    framebufferRTVdesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
    hr = Device->CreateRenderTargetView(backBuffer, &framebufferRTVdesc, &RenderTargetView);
    backBuffer->Release();
    if (FAILED(hr) || !RenderTargetView) {
        UE_LOG("CreateRenderTargetView failed.\n");
        return;
    }

    // 2) DepthStencil 텍스처/뷰 생성
    ID3D11Texture2D* depthTex = nullptr;
    D3D11_TEXTURE2D_DESC depthDesc{};
    depthDesc.Width = width;
    depthDesc.Height = height;
    depthDesc.MipLevels = 1;
    depthDesc.ArraySize = 1;
    depthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    depthDesc.SampleDesc.Count = 1;               // 멀티샘플링 끄는 경우
    depthDesc.SampleDesc.Quality = 0;
    depthDesc.Usage = D3D11_USAGE_DEFAULT;
    depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

    hr = Device->CreateTexture2D(&depthDesc, nullptr, &depthTex);
    if (FAILED(hr) || !depthTex) {
        UE_LOG("CreateTexture2D(depth) failed.\n");
        return;
    }

    D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
    dsvDesc.Format = depthDesc.Format;
    dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
    dsvDesc.Texture2D.MipSlice = 0;

    hr = Device->CreateDepthStencilView(depthTex, &dsvDesc, &DepthStencilView);
    depthTex->Release();
    if (FAILED(hr) || !DepthStencilView) {
        UE_LOG("CreateDepthStencilView failed.\n");
        return;
    }

    // 3) OM 바인딩
    DeviceContext->OMSetRenderTargets(1, &RenderTargetView, DepthStencilView);

    // 4) 뷰포트 갱신
    SetViewport(width, height);
}

// ──────────────────────────────────────────────────────
// Helper: Viewport 갱신
// ──────────────────────────────────────────────────────
void D3D11RHI::SetViewport(UINT width, UINT height)
{
    ViewportInfo.TopLeftX = 0.0f;
    ViewportInfo.TopLeftY = 0.0f;
    ViewportInfo.Width = static_cast<float>(width);
    ViewportInfo.Height = static_cast<float>(height);
    ViewportInfo.MinDepth = 0.0f;
    ViewportInfo.MaxDepth = 1.0f;

    DeviceContext->RSSetViewports(1, &ViewportInfo);
}

// ──────────────────────────────────────────────────────
// 기존 오타 호출 호환용 래퍼 (선택)
// ──────────────────────────────────────────────────────
void D3D11RHI::setviewort(UINT width, UINT height)
{
    SetViewport(width, height);
}
void D3D11RHI::ResizeSwapChain(UINT width, UINT height)
{
    if (!SwapChain) return;

    // 렌더링 완료까지 대기 (중요!)
    if (DeviceContext) {
        DeviceContext->Flush();
    }

    // 현재 렌더 타겟 언바인딩
    if (DeviceContext) {
        DeviceContext->OMSetRenderTargets(0, nullptr, nullptr);
    }

    // 기존 뷰 해제
    if (RenderTargetView) { RenderTargetView->Release(); RenderTargetView = nullptr; }
    if (DepthStencilView) { DepthStencilView->Release(); DepthStencilView = nullptr; }
    if (FrameBuffer) { FrameBuffer->Release(); FrameBuffer = nullptr; }

    // 스왑체인 버퍼 리사이즈
    HRESULT hr = SwapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);
    if (FAILED(hr)) { UE_LOG("ResizeBuffers failed!\n"); return; }

    // 다시 RTV/DSV 만들기
    CreateBackBufferAndDepthStencil(width, height);

    // 뷰포트도 갱신
    setviewort(width, height);
}

void D3D11RHI::PSSetDefaultSampler(UINT StartSlot)
{
	DeviceContext->PSSetSamplers(StartSlot, 1, &DefaultSamplerState);
}
