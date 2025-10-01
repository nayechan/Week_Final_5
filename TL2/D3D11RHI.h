#pragma once
#include "RHIDevice.h"
#include "ResourceManager.h"
#include "VertexData.h"
class D3D11RHI : public URHIDevice
{
public:
    D3D11RHI() {};
    ~D3D11RHI()
    {
        Release();
    }



public:
    void Initialize(HWND hWindow) override;

    void Release()override;


public:
    // clear
    void ClearBackBuffer() override;
    void ClearDepthBuffer(float Depth, UINT Stencil) override;
    void CreateBlendState() override;

    template<typename TVertex>
    static HRESULT CreateVertexBufferImpl(ID3D11Device* device, const FMeshData& mesh, ID3D11Buffer** outBuffer, D3D11_USAGE usage, UINT cpuAccessFlags);

    template<typename TVeretex>
    static HRESULT CreateVertexBuffer(ID3D11Device* device, const FMeshData& mesh, ID3D11Buffer** outBuffer);

    template<typename TVertex>
    static HRESULT CreateVertexBufferImpl(ID3D11Device* device, const std::vector<FNormalVertex>& srcVertices, ID3D11Buffer** outBuffer, D3D11_USAGE usage, UINT cpuAccessFlags);

    template<typename TVertex>
    static HRESULT CreateVertexBuffer(ID3D11Device* device, const std::vector<FNormalVertex>& srcVertices, ID3D11Buffer** outBuffer);

    static HRESULT CreateIndexBuffer(ID3D11Device* device, const FMeshData* meshData, ID3D11Buffer** outBuffer);

    static HRESULT CreateIndexBuffer(ID3D11Device* device, const FStaticMesh* mesh, ID3D11Buffer** outBuffer);



    void UpdateConstantBuffers(const FMatrix& ModelMatrix, const FMatrix& ViewMatrix, const FMatrix& ProjMatrix) override;
    void UpdateViewProjectionBuffers(const FMatrix& ViewMatrix, const FMatrix& ProjMatrix);
    void UpdateModelBuffer(const FMatrix& ModelMatrix);
    void UpdateBillboardConstantBuffers(const FVector& pos, const FMatrix& ViewMatrix, const FMatrix& ProjMatrix, const FVector& CameraRight, const FVector& CameraUp) override;
    void UpdatePixelConstantBuffers(const FObjMaterialInfo& InMaterialInfo, bool bHasMaterial, bool bHasTexture) override;
    void UpdateHighLightConstantBuffers(const uint32 InPicked, const FVector& InColor, const uint32 X, const uint32 Y, const uint32 Z, const uint32 Gizmo) override;
    void UpdateColorConstantBuffers(const FVector4& InColor) override;
    void UpdateUVScrollConstantBuffers(const FVector2D& Speed, float TimeSec) override;

    void IASetPrimitiveTopology() override;
    void RSSetState(EViewModeIndex ViewModeIndex) override;
    void RSSetViewport() override;
    void OMSetRenderTargets() override;
    void OMSetBlendState(bool bIsBlendMode) override;
    void Present() override;
	void PSSetDefaultSampler(UINT StartSlot) override;
    void RSSetNoCullState() override;

    // Overlay precedence helpers
    void OMSetDepthStencilState_OverlayWriteStencil() override;
    void OMSetDepthStencilState_StencilRejectOverlay() override;

    void CreateShader(ID3D11InputLayout** OutSimpleInputLayout, ID3D11VertexShader** OutSimpleVertexShader, ID3D11PixelShader** OutSimplePixelShader) override;

    void OnResize(UINT NewWidth, UINT NewHeight);

    void CreateBackBufferAndDepthStencil(UINT width, UINT height);

    void SetViewport(UINT width, UINT height);

    void setviewort(UINT width, UINT height);

    void ResizeSwapChain(UINT width, UINT height);

    // Viewport query
    UINT GetViewportWidth() const { return (UINT)ViewportInfo.Width; }
    UINT GetViewportHeight() const { return (UINT)ViewportInfo.Height; }

public:
    // getter
    inline ID3D11Device* GetDevice()
    {
        return Device;
    }
    inline ID3D11DeviceContext* GetDeviceContext()
    {
        return DeviceContext;
    }
    inline IDXGISwapChain* GetSwapChain()
    {
        return SwapChain;
    }

private:
    void CreateDeviceAndSwapChain(HWND hWindow)override; // 여기서 디바이스, 디바이스 컨택스트, 스왑체인, 뷰포트를 초기화한다
    void CreateFrameBuffer() override;
    void CreateRasterizerState() override;
    void CreateConstantBuffer() override;
    void CreateDepthStencilState() override;
	void CreateSamplerState();

    // release
	void ReleaseSamplerState();
    void ReleaseBlendState();
    void ReleaseRasterizerState(); // rs
    void ReleaseFrameBuffer(); // fb, rtv
    void ReleaseDeviceAndSwapChain();
 
	void OmSetDepthStencilState(EComparisonFunc Func) override;
    
private:
    //24
    D3D11_VIEWPORT ViewportInfo{};

    //8
    ID3D11Device* Device{};//
    ID3D11DeviceContext* DeviceContext{};//
    IDXGISwapChain* SwapChain{};//

    ID3D11RasterizerState* DefaultRasterizerState{};//
    ID3D11RasterizerState* WireFrameRasterizerState{};//
    ID3D11RasterizerState* NoCullRasterizerState{};//

    ID3D11DepthStencilState* DepthStencilState{};
    ID3D11DepthStencilState* DepthStencilStateLessEqualWrite = nullptr;      // 기본
    ID3D11DepthStencilState* DepthStencilStateLessEqualReadOnly = nullptr;   // 읽기 전용
    ID3D11DepthStencilState* DepthStencilStateAlwaysNoWrite = nullptr;       // 기즈모/오버레이
    ID3D11DepthStencilState* DepthStencilStateDisable = nullptr;              // 깊이 테스트/쓰기 모두 끔
    ID3D11DepthStencilState* DepthStencilStateGreaterEqualWrite = nullptr;   // 선택사항
    // Stencil-based overlay control
    ID3D11DepthStencilState* DepthStencilStateOverlayWriteStencil = nullptr;   // overlay writes stencil=1
    ID3D11DepthStencilState* DepthStencilStateStencilRejectOverlay = nullptr;  // draw only where stencil==0

    ID3D11BlendState* BlendState{};

    ID3D11Texture2D* FrameBuffer{};//
    ID3D11RenderTargetView* RenderTargetView{};//
    ID3D11DepthStencilView* DepthStencilView{};//

    // 버퍼 핸들
    ID3D11Buffer* ModelCB{};
    ID3D11Buffer* ViewProjCB{};
    ID3D11Buffer* HighLightCB{};
    ID3D11Buffer* BillboardCB{};
    ID3D11Buffer* ColorCB{};
    ID3D11Buffer* PixelConstCB{};
    ID3D11Buffer* UVScrollCB{};

    ID3D11Buffer* ConstantBuffer{};

    ID3D11SamplerState* DefaultSamplerState = nullptr;
};


template<typename TVertex>
inline HRESULT D3D11RHI::CreateVertexBufferImpl(ID3D11Device* device, const FMeshData& mesh, ID3D11Buffer** outBuffer, D3D11_USAGE usage, UINT cpuAccessFlags)
{
    std::vector<TVertex> vertexArray;
    vertexArray.reserve(mesh.Vertices.size());

    for (size_t i = 0; i < mesh.Vertices.size(); ++i) {
        TVertex vtx{};
        vtx.FillFrom(mesh, i);
        vertexArray.push_back(vtx);
    }

    D3D11_BUFFER_DESC vbd = {};
    vbd.Usage = usage;
    vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    vbd.CPUAccessFlags = cpuAccessFlags;
    vbd.ByteWidth = static_cast<UINT>(sizeof(TVertex) * vertexArray.size());

    D3D11_SUBRESOURCE_DATA vinitData = {};
    vinitData.pSysMem = vertexArray.data();

    return device->CreateBuffer(&vbd, &vinitData, outBuffer);
}

// PositionColor → Static
template<>
inline HRESULT D3D11RHI::CreateVertexBuffer<FVertexSimple>(ID3D11Device* device, const FMeshData& mesh, ID3D11Buffer** outBuffer)
{
    return CreateVertexBufferImpl<FVertexSimple>(device, mesh, outBuffer,
        D3D11_USAGE_DEFAULT, 0);
}

// PositionColorTextureNormal → Static
template<>
inline HRESULT D3D11RHI::CreateVertexBuffer<FVertexDynamic>(ID3D11Device* device, const FMeshData& mesh, ID3D11Buffer** outBuffer)
{
    return CreateVertexBufferImpl<FVertexDynamic>(device, mesh, outBuffer,
        D3D11_USAGE_DEFAULT, 0);
}

// Billboard → Dynamic
template<>
inline HRESULT D3D11RHI::CreateVertexBuffer<FBillboardVertexInfo_GPU>(ID3D11Device* device, const FMeshData& mesh, ID3D11Buffer** outBuffer)
{
    return CreateVertexBufferImpl<FBillboardVertexInfo_GPU>(device, mesh, outBuffer,
        D3D11_USAGE_DYNAMIC,
        D3D11_CPU_ACCESS_WRITE);
}


template<>
inline HRESULT D3D11RHI::CreateVertexBuffer<FBillboardVertex>(ID3D11Device* device, const FMeshData& mesh, ID3D11Buffer** outBuffer)
{
    return CreateVertexBufferImpl<FBillboardVertex>(device, mesh, outBuffer,
        D3D11_USAGE_DYNAMIC,
        D3D11_CPU_ACCESS_WRITE);
}


template<typename TVertex>
inline HRESULT D3D11RHI::CreateVertexBufferImpl(ID3D11Device* device, const std::vector<FNormalVertex>& srcVertices, ID3D11Buffer** outBuffer, D3D11_USAGE usage, UINT cpuAccessFlags)
{
    std::vector<TVertex> vertexArray;
    vertexArray.reserve(srcVertices.size());

    for (size_t i = 0; i < srcVertices.size(); ++i)
    {
        TVertex vtx{};
        vtx.FillFrom(srcVertices[i]); // 각 TVertex에서 FillFrom 구현 필요
        vertexArray.push_back(vtx);
    }

    D3D11_BUFFER_DESC vbd = {};
    vbd.Usage = usage;
    vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    vbd.CPUAccessFlags = cpuAccessFlags;
    vbd.ByteWidth = static_cast<UINT>(sizeof(TVertex) * vertexArray.size());

    D3D11_SUBRESOURCE_DATA vinitData = {};
    vinitData.pSysMem = vertexArray.data();

    return device->CreateBuffer(&vbd, &vinitData, outBuffer);
}

template<>
inline HRESULT D3D11RHI::CreateVertexBuffer<FVertexSimple>(ID3D11Device* device, const std::vector<FNormalVertex>& srcVertices, ID3D11Buffer** outBuffer)
{
    return CreateVertexBufferImpl<FVertexSimple>(device, srcVertices, outBuffer, D3D11_USAGE_DEFAULT, 0);
}

// PositionColorTextureNormal
template<>
inline HRESULT D3D11RHI::CreateVertexBuffer<FVertexDynamic>(ID3D11Device* device, const std::vector<FNormalVertex>& srcVertices, ID3D11Buffer** outBuffer)
{
    return CreateVertexBufferImpl<FVertexDynamic>(device, srcVertices, outBuffer, D3D11_USAGE_DEFAULT, 0);
}

// Billboard
template<>
inline HRESULT D3D11RHI::CreateVertexBuffer<FBillboardVertexInfo_GPU>(ID3D11Device* device, const std::vector<FNormalVertex>& srcVertices, ID3D11Buffer** outBuffer)
{
    return CreateVertexBufferImpl<FBillboardVertexInfo_GPU>(device, srcVertices, outBuffer, D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE);
}
