#pragma once
#include "ObjectFactory.h"
#include "Object.h"
#include "Shader.h"
#include "StaticMesh.h"
#include "Material.h"
#include "Texture.h"
#include "DynamicMesh.h"
#include "Quad.h"
#include "LineDynamicMesh.h"

class UStaticMesh;
class FMeshBVH;

class UResourceBase;
class UStaticMesh;
class UMaterial;

struct FShaderDesc
{
    std::wstring Filename;
    std::string EntryVS;
    std::string EntryPS;
    std::vector<D3D11_INPUT_ELEMENT_DESC> InputLayout;
};

class UResourceManager :public UObject
{
public:
    DECLARE_CLASS(UResourceManager, UObject)
    static UResourceManager& GetInstance();
    void Initialize(ID3D11Device* InDevice, ID3D11DeviceContext* InContext);

    ID3D11Device* GetDevice() { return Device; }

    //font 렌더링을 위함(dynamicVertexBuffer 만듦.)
    //FResourceData* CreateOrGetResourceData(const FString& Name, uint32 Size, const TArray<uint32>& Indicies);
    //    FTextureData* GetOrCreateTexture

    UMaterial* GetOrCreateMaterial(const FString& Name,  EVertexLayoutType layoutType);

    void CreateTextBillboardTexture();

    void UpdateDynamicVertexBuffer(const FString& name, TArray<FBillboardVertexInfo_GPU>& vertices);
    FTextureData* CreateOrGetTextureData(const FWideString& FilePath);

    // 전체 해제
    void Clear();

    void CreateAxisMesh(float Length, const FString& FilePath);
    void CreateTextBillboardMesh();
    void CreateBillboardMesh();
    void CreateGridMesh(int N, const FString& FilePath);
    void CreateBoxWireframeMesh(const FVector& Min, const FVector& Max, const FString& FilePath);
    //FMeshData* CreateWireBoxMesh(const FVector& Min, const FVector& Max, const FString& FilePath);
   // void CreateBoxMesh(const FVector& Min, const FVector& Max, const FString& FilePath);
    void CreateDefaultShader();
    void InitShaderILMap();
    void InitTexToShaderMap();

    template<typename T>
    bool Add(const FString& InFilePath, UObject* InObject);
    template<typename T>
    T* Get(const FString& InFilePath);
    template<typename T, typename ... Args>
    T* Load(const FString& InFilePath, Args&& ...);
    template<typename T>
    ResourceType GetResourceType();

    // Enumeration helpers
    template<typename T>
    TArray<T*> GetAll();
    template<typename T>
    TArray<FString> GetAllFilePaths();
    // Convenience for UStaticMesh
    TArray<UStaticMesh*> GetAllStaticMeshes() { return GetAll<UStaticMesh>(); }
    TArray<FString> GetAllStaticMeshFilePaths() { return GetAllFilePaths<UStaticMesh>(); }
    TArray<D3D11_INPUT_ELEMENT_DESC>& GetProperInputLayout(const FString& InShaderName);
    FString& GetProperShader(const FString& InTextureName);

    // Mesh BVH cache (OBJ path -> built BVH)
    FMeshBVH* GetMeshBVH(const FString& ObjPath);
    FMeshBVH* GetOrBuildMeshBVH(const FString& ObjPath, const struct FStaticMesh* StaticMeshAsset);

    // MeshCache (for material sorting)
    void SetStaticMeshs();
    const TArray<UStaticMesh*>& GetStaticMeshs() { return StaticMeshs; }

public:
    UResourceManager() = default;
protected:
    ~UResourceManager() override;

    UResourceManager(const UResourceManager&) = delete;
    UResourceManager& operator=(const UResourceManager&) = delete;

    ID3D11Device* Device = nullptr;
    ID3D11DeviceContext* Context = nullptr;

    TMap<FString, FResourceData*> ResourceMap;

    //Deprecated
    TMap<FWideString, FTextureData*> TextureMap;
    // TMap<FString, UStaticMesh*> StaticMeshMap;

    //Resource Type의 개수만큼 Array 생성 및 저장
    TArray<TMap<FString, UResourceBase*>> Resources;

    FShader PrimitiveShader;
    TMap<FWideString,FShader*> ShaderList;

    TMap<FString, TArray<D3D11_INPUT_ELEMENT_DESC>> ShaderToInputLayoutMap;
    TMap<FString, FString> TextureToShaderMap;

    TArray<UStaticMesh*> StaticMeshs;
private:
    TMap<FString, UMaterial*> MaterialMap;

    // Cache for per-mesh BVHs to avoid rebuilding for identical OBJ assets
    TMap<FString, FMeshBVH*> MeshBVHCache;
};
//-----definition
// 리소스 매니저에 새로운 리소스 등록하는 함수이다. 
template<typename T>
bool UResourceManager::Add(const FString& InFilePath, UObject* InObject)
{
    uint8 typeIndex = static_cast<uint8>(GetResourceType<T>());
    auto iter = Resources[typeIndex].find(InFilePath);
    if (iter == Resources[typeIndex].end())
    {
        Resources[typeIndex][InFilePath] = static_cast<T*>(InObject);
        // 경로 저장 
        Resources[typeIndex][InFilePath]->SetFilePath(InFilePath);
        return true;
    }
    return false;
}

template<typename T>
T* UResourceManager::Get(const FString& InFilePath)
{
    uint8 typeIndex = static_cast<uint8>(GetResourceType<T>());
    auto iter = Resources[typeIndex].find(InFilePath);
    if (iter != Resources[typeIndex].end())
    {
        return static_cast<T*>(iter->second);
    }

    return nullptr;
}

template<typename T, typename ...Args>
inline T* UResourceManager::Load(const FString& InFilePath, Args&&... InArgs)//있으면 긁어오고 없으면 만듦
{
    uint8 typeIndex = static_cast<uint8>(GetResourceType<T>());
    auto iter = Resources[typeIndex].find(InFilePath);
    if (iter != Resources[typeIndex].end())
    {
        return static_cast<T*>((*iter).second);
    }
    else//없으면 해당 리소스의 Load실행
    {
        T* Resource = NewObject<T>();
        Resource->Load(InFilePath, Device);
        Resource->SetFilePath(InFilePath);
        Resources[typeIndex][InFilePath] = Resource;
        return Resource;
    }
}

template<typename T>
ResourceType UResourceManager::GetResourceType()
{
    if (T::StaticClass() == UStaticMesh::StaticClass())
        return ResourceType::StaticMesh;
    if (T::StaticClass() == UQuad::StaticClass())
        return ResourceType::Quad;
    if (T::StaticClass() == UDynamicMesh::StaticClass())
        return ResourceType::DynamicMesh;
    if (T::StaticClass() == ULineDynamicMesh::StaticClass())
        return ResourceType::DynamicMesh; // share bucket with DynamicMesh
    if (T::StaticClass() == UShader::StaticClass())
        return ResourceType::Shader;
    if (T::StaticClass() == UTexture::StaticClass())
        return ResourceType::Texture;
    if (T::StaticClass() == UMaterial::StaticClass())
        return ResourceType::Material;

    return ResourceType::None;
}

// Enumerate all resources of a type T
template<typename T>
TArray<T*> UResourceManager::GetAll()
{
    TArray<T*> Result;
    uint8 TypeIndex = static_cast<uint8>(GetResourceType<T>());
    if (TypeIndex >= Resources.size())
    {
        return Result;
    }

    for (auto& Pair : Resources[TypeIndex])
    {
        if (Pair.second)
        {
            Result.push_back(static_cast<T*>(Pair.second));
        }
    }
    return Result;
}

// Collect non-empty FilePath of all resources of type T
template<typename T>
TArray<FString> UResourceManager::GetAllFilePaths()
{
    TArray<FString> Paths;
    uint8 TypeIndex = static_cast<uint8>(GetResourceType<T>());
    if (TypeIndex >= Resources.size())
    {
        return Paths;
    }

    for (auto& Pair : Resources[TypeIndex])
    {
        if (Pair.second)
        {
            const FString& Path = Pair.second->GetFilePath();
            if (!Path.empty())
            {
                Paths.push_back(Path);
            }
        }
    }
    return Paths;
}
