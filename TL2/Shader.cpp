#include "pch.h"
#include "Shader.h"

UShader::~UShader()
{
    ReleaseResources();
}

// 두 개의 셰이더 파일을 받는 주요 Load 함수
void UShader::Load(const FString& InShaderPath, ID3D11Device* InDevice)
{
    assert(InDevice);

    std::wstring WFilePath;
    WFilePath = std::wstring(InShaderPath.begin(), InShaderPath.end());

    HRESULT hr;
    ID3DBlob* errorBlob = nullptr;
    hr = D3DCompileFromFile(WFilePath.c_str(), nullptr, nullptr, "mainVS", "vs_5_0", 0, 0, &VSBlob, &errorBlob);
    if (FAILED(hr))
    {
        char* msg = (char*)errorBlob->GetBufferPointer();
        UE_LOG("shader \'%s\'compile error: %s", InShaderPath, msg);
        if (errorBlob) errorBlob->Release();
        return;
    }

    hr = InDevice->CreateVertexShader(VSBlob->GetBufferPointer(), VSBlob->GetBufferSize(), nullptr, &VertexShader);

    hr = D3DCompileFromFile(WFilePath.c_str(), nullptr, nullptr, "mainPS", "ps_5_0", 0, 0, &PSBlob, nullptr);

    hr = InDevice->CreatePixelShader(PSBlob->GetBufferPointer(), PSBlob->GetBufferSize(), nullptr, &PixelShader);

    CreateInputLayout(InDevice, InShaderPath);
}

void UShader::CreateInputLayout(ID3D11Device* Device, const FString& InShaderPath)
{
    TArray<D3D11_INPUT_ELEMENT_DESC> descArray = UResourceManager::GetInstance().GetProperInputLayout(InShaderPath);
    const D3D11_INPUT_ELEMENT_DESC* layout = descArray.data();
    uint32 layoutCount = static_cast<uint32>(descArray.size());

    HRESULT hr = Device->CreateInputLayout(
        layout,
        layoutCount,
        VSBlob->GetBufferPointer(), 
        VSBlob->GetBufferSize(),
        &InputLayout);
    assert(SUCCEEDED(hr));
}

void UShader::ReleaseResources()
{
    if (VSBlob)
    {
        VSBlob->Release();
        VSBlob = nullptr;
    }
    if (PSBlob)
    {
        PSBlob->Release();
        PSBlob = nullptr;
    }
    if (InputLayout)
    {
        InputLayout->Release();
        InputLayout = nullptr;
    }
    if (VertexShader)
    {
        VertexShader->Release();
        VertexShader = nullptr;
    }
    if (PixelShader)
    {
        PixelShader->Release();
        PixelShader = nullptr;
    }
}
// ========================== 오클루전 관련 메소드들 ==========================
/*
static bool CompileShaderBlob(const wchar_t* FilePath, const char* Entry, const char* Target, ID3DBlob** OutBlob)
{
    UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined(_DEBUG)
    flags |= D3D_COMPILE_DEBUG | D3D_COMPILE_SKIP_OPTIMIZATION;
#else
    flags |= D3DCOMPILE_OPTIMIZATION_LEVEL3;
#endif

    ID3DBlob* bytecode = nullptr;
    ID3DBlob* errors = nullptr;
    HRESULT hr = D3DCompileFromFile(
        FilePath, nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
        Entry, Target, flags, 0, &bytecode, &errors);

    if (FAILED(hr))
    {
        if (errors)
        {
            OutputDebugStringA((const char*)errors->GetBufferPointer());
            errors->Release();
        }
        if (bytecode) bytecode->Release();
        return false;
    }

    *OutBlob = bytecode;
    if (errors) errors->Release();
    return true;
}

bool CompileVS(ID3D11Device* Dev, const wchar_t* FilePath, const char* Entry,
    ID3D11VertexShader** OutVS, ID3DBlob** OutVSBytecode)
{
    if (!Dev || !OutVS) return false;

    ID3DBlob* vsBlob = nullptr;
    if (!CompileShaderBlob(FilePath, Entry, "vs_5_0", &vsBlob)) return false;

    ID3D11VertexShader* VS = nullptr;
    HRESULT hr = Dev->CreateVertexShader(
        vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &VS);
    if (FAILED(hr))
    {
        vsBlob->Release();
        return false;
    }

    *OutVS = VS;
    if (OutVSBytecode) *OutVSBytecode = vsBlob;
    else vsBlob->Release();

    return true;
}

bool CompilePS(ID3D11Device* Dev, const wchar_t* FilePath, const char* Entry,
    ID3D11PixelShader** OutPS)
{
    if (!Dev || !OutPS) return false;

    ID3DBlob* psBlob = nullptr;
    if (!CompileShaderBlob(FilePath, Entry, "ps_5_0", &psBlob)) return false;

    ID3D11PixelShader* PS = nullptr;
    HRESULT hr = Dev->CreatePixelShader(
        psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &PS);
    psBlob->Release();

    if (FAILED(hr)) return false;

    *OutPS = PS;
    return true;
}
*/

// ========================================================================