#include "pch.h"
#include "SkeletalMesh.h"

#include "FbxLoader.h"

IMPLEMENT_CLASS(USkeletalMesh)

USkeletalMesh::USkeletalMesh()
{
}

USkeletalMesh::~USkeletalMesh()
{
    ReleaseResources();
}

void USkeletalMesh::Load(const FString& InFilePath, ID3D11Device* InDevice)
{
    Data = UFbxLoader::GetInstance().LoadFbxMesh(InFilePath);
    if (Data->Vertices.empty() || Data->Indices.empty()) { return; }
    
    CreateVertexBuffer(Data, InDevice);
    CreateIndexBuffer(Data, InDevice);
    VertexCount = static_cast<uint32>(Data->Vertices.size());
    IndexCount = static_cast<uint32>(Data->Indices.size());
    VertexStride = sizeof(FVertexDynamic);
}

void USkeletalMesh::ReleaseResources()
{
    if (VertexBuffer)
    {
        VertexBuffer->Release();
        VertexBuffer = nullptr;
    }

    if (IndexBuffer)
    {
        IndexBuffer->Release();
        IndexBuffer = nullptr;
    }

    if (Data)
    {
        delete Data;
        Data = nullptr;
    }
}

void USkeletalMesh::SetSkeletalMeshAsset(FSkeletalMeshData* InSkeletalMesh, ID3D11Device* InDevice)
{
    if (Data)
    {
        ReleaseResources();
    }
    Data = InSkeletalMesh;  

    CreateVertexBuffer(Data, InDevice);
    CreateIndexBuffer(Data, InDevice);
    VertexCount = static_cast<uint32>(Data->Vertices.size());
    IndexCount = static_cast<uint32>(Data->Indices.size());
}

void USkeletalMesh::UpdateVertexBuffer(const TArray<FNormalVertex>& SkinnedVertices)
{
    if (!VertexBuffer) { return; }

    GEngine.GetRHIDevice()->VertexBufferUpdate(VertexBuffer, SkinnedVertices);
}

void USkeletalMesh::CreateVertexBuffer(FSkeletalMeshData* InSkeletalMesh, ID3D11Device* InDevice)
{
    HRESULT hr = D3D11RHI::CreateVertexBuffer<FVertexDynamic>(InDevice, InSkeletalMesh->Vertices, &VertexBuffer);
    assert(SUCCEEDED(hr));
}

void USkeletalMesh::CreateIndexBuffer(FSkeletalMeshData* InSkeletalMesh, ID3D11Device* InDevice)
{
    HRESULT hr = D3D11RHI::CreateIndexBuffer(InDevice, InSkeletalMesh, &IndexBuffer);
    assert(SUCCEEDED(hr));
}
