#include "pch.h"
#include "StaticMesh.h"
#include "ObjManager.h"
#include "ResourceManager.h"

UStaticMesh::~UStaticMesh()
{
    ReleaseResources();
}

void UStaticMesh::Load(const FString& InFilePath, ID3D11Device* InDevice, EVertexLayoutType InVertexType)
{
    assert(InDevice);

    VertexType = InVertexType;

    StaticMeshAsset = FObjManager::LoadObjStaticMeshAsset(InFilePath);
    CreateVertexBuffer(StaticMeshAsset, InDevice, InVertexType);
    CreateIndexBuffer(StaticMeshAsset, InDevice);
    VertexCount = static_cast<uint32>(StaticMeshAsset->Vertices.size());
    IndexCount = static_cast<uint32>(StaticMeshAsset->Indices.size());

    // Cache or build BVH once per OBJ asset and keep reference
    //if (StaticMeshAsset)
    //{
    //    const FString& Key = StaticMeshAsset->PathFileName;
    //    MeshBVH = UResourceManager::GetInstance().GetOrBuildMeshBVH(Key, StaticMeshAsset);
    //}
}

void UStaticMesh::Load(FMeshData* InData, ID3D11Device* InDevice, EVertexLayoutType InVertexType)
{
    VertexType = InVertexType;

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

    CreateVertexBuffer(InData, InDevice, InVertexType);
    CreateIndexBuffer(InData, InDevice);

    VertexCount = static_cast<uint32>(InData->Vertices.size());
    IndexCount = static_cast<uint32>(InData->Indices.size());
}

bool UStaticMesh::EraseUsingComponets(UStaticMeshComponent* InStaticMeshComponent)
{
    auto it = std::find(UsingComponents.begin(), UsingComponents.end(), InStaticMeshComponent);
    if (it != UsingComponents.end())
    {
        UsingComponents.erase(it);
        return true;
    }
    return false;
}

bool UStaticMesh::AddUsingComponents(UStaticMeshComponent* InStaticMeshComponent)
{
    UsingComponents.Add(InStaticMeshComponent);
    return true;
}

void UStaticMesh::CreateVertexBuffer(FMeshData* InMeshData, ID3D11Device* InDevice, EVertexLayoutType InVertexType)
{
    HRESULT hr;
    hr = D3D11RHI::CreateVertexBuffer<FVertexDynamic>(InDevice, *InMeshData, &VertexBuffer);
    assert(SUCCEEDED(hr));
}

void UStaticMesh::CreateVertexBuffer(FStaticMesh* InStaticMesh, ID3D11Device* InDevice, EVertexLayoutType InVertexType)
{
    HRESULT hr;
    hr = D3D11RHI::CreateVertexBuffer<FVertexDynamic>(InDevice, InStaticMesh->Vertices, &VertexBuffer);
    assert(SUCCEEDED(hr));
}

void UStaticMesh::CreateIndexBuffer(FMeshData* InMeshData, ID3D11Device* InDevice)
{
    HRESULT hr = D3D11RHI::CreateIndexBuffer(InDevice, InMeshData, &IndexBuffer);
    assert(SUCCEEDED(hr));
}

void UStaticMesh::CreateIndexBuffer(FStaticMesh* InStaticMesh, ID3D11Device* InDevice)
{
    HRESULT hr = D3D11RHI::CreateIndexBuffer(InDevice, InStaticMesh, &IndexBuffer);
    assert(SUCCEEDED(hr));
}

void UStaticMesh::ReleaseResources()
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
}

