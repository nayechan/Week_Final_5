#include "pch.h"
#include "MeshLoader.h"
#include "ObjectFactory.h"
#include "DDSTextureLoader.h"
#include "WICTextureLoader.h"
#include "ObjManager.h"
#include "Quad.h"
#include "MeshBVH.h"
#include "Enums.h"

#include <filesystem>
#include <cwctype>
const char* UResourceManager::FullScreenVSPath = "Shaders/Utility/FullScreenTriangle_VS.hlsl";
const char* UResourceManager::BlitPSPath = "Shaders/Utility/Blit_PS.hlsl";
IMPLEMENT_CLASS(UResourceManager)

#define GRIDNUM 100
#define AXISLENGTH 100

UResourceManager::~UResourceManager()
{
    Clear();
}

UResourceManager& UResourceManager::GetInstance()
{
    static UResourceManager* Instance = nullptr;
        if (Instance == nullptr)
    {
        Instance = NewObject<UResourceManager>();
    }
    return *Instance;
}

void UResourceManager::Initialize(ID3D11Device* InDevice, ID3D11DeviceContext* InContext)
{
    Device = InDevice;
    Resources.SetNum(static_cast<uint8>(EResourceType::End));

    Context = InContext;
    //CreateGridMesh(GRIDNUM,"Grid");
    //CreateAxisMesh(AXISLENGTH,"Axis");

    InitShaderILMap();

    InitTexToShaderMap();

    CreateTextBillboardMesh();//"TextBillboard"
    CreateBillboardMesh(); // Billboard
    CreateTextBillboardTexture();
    CreateDefaultShader();
    CreateDefaultMaterial();
}

// 전체 해제
void UResourceManager::Clear()
{
    {////////////// Deprecated //////////////
        for (auto& [Key, Data] : ResourceMap)
        {
            if (Data)
            {
                if (Data->VertexBuffer)
                {
                    Data->VertexBuffer->Release();
                    Data->VertexBuffer = nullptr;
                }
                if (Data->IndexBuffer)
                {
                    Data->IndexBuffer->Release();
                    Data->IndexBuffer = nullptr;
                }
                delete Data;
            }
        }
        ResourceMap.clear();


        // TextureMap 해제
        for (auto& [Key, Data] : TextureMap)
        {
            if (Data)
            {
                if (Data->Texture) { Data->Texture->Release(); Data->Texture = nullptr; }
                if (Data->TextureSRV) { Data->TextureSRV->Release(); Data->TextureSRV = nullptr; }
                if (Data->BlendState) { Data->BlendState->Release(); Data->BlendState = nullptr; }
                delete Data;
            }
        }
        TextureMap.clear();

        // MaterialMap 해제
        for (auto& [Key, Mat] : MaterialMap)
        {
            if (Mat)
            {
                ObjectFactory::DeleteObject(Mat);
            }
        }
        MaterialMap.clear();

        // Mesh BVH cache clear
        for (auto& Pair : MeshBVHCache)
        {
            delete Pair.second;
        }
        MeshBVHCache.clear();
    }

    for (auto& Array : Resources)
    {
        for (auto& Resource : Array)
        {
            if(Resource.second)
            {
                DeleteObject(Resource.second);
                Resource.second = nullptr;
            }
        }
        Array.Empty();
    }
    Resources.Empty();

    // Instance lifetime is managed by ObjectFactory
}

FMeshBVH* UResourceManager::GetMeshBVH(const FString& ObjPath)
{
    if (auto* Found = MeshBVHCache.Find(ObjPath))
        return *Found;
    return nullptr;
}

FMeshBVH* UResourceManager::GetOrBuildMeshBVH(const FString& ObjPath, const FStaticMesh* StaticMeshAsset)
{
    if (auto* Found = MeshBVHCache.Find(ObjPath))
        return *Found;

    if (!StaticMeshAsset)
        return nullptr;

    FMeshBVH* NewBVH = new FMeshBVH();
    NewBVH->Build(StaticMeshAsset->Vertices, StaticMeshAsset->Indices);
    MeshBVHCache.Add(ObjPath, NewBVH);
    return NewBVH;
}

void UResourceManager::SetStaticMeshs()
{
    StaticMeshs = GetAll<UStaticMesh>();
}

void UResourceManager::SetSkeletalMeshs()
{
    SkeletalMeshs = GetAll<USkeletalMesh>();
}

void UResourceManager::SetAnimations()
{
    Animations = GetAll<UAnimSequence>();
}

void UResourceManager::SetAudioFiles()
{ 
    Sounds = GetAll<USound>();
}


void UResourceManager::CreateAxisMesh(float Length, const FString& FilePath)
{
    // 이미 있으면 패스
    if (ResourceMap[FilePath])
    {
        return;
    }

    TArray<FVector> axisVertices;
    TArray<FVector4> axisColors;
    TArray<uint32> axisIndices;

    // X축 (빨강)
    axisVertices.push_back(FVector(0.0f, 0.0f, 0.0f));       // 원점
    axisVertices.push_back(FVector(Length, 0.0f, 0.0f));     // +X
    axisColors.push_back(FVector4(1.0f, 0.0f, 0.0f, 1.0f));  // 빨강
    axisColors.push_back(FVector4(1.0f, 0.0f, 0.0f, 1.0f));  // 빨강
    axisIndices.push_back(0);
    axisIndices.push_back(1);

    // Y축 (초록)
    axisVertices.push_back(FVector(0.0f, 0.0f, 0.0f));       // 원점
    axisVertices.push_back(FVector(0.0f, Length, 0.0f));     // +Y
    axisColors.push_back(FVector4(0.0f, 1.0f, 0.0f, 1.0f));  // 초록
    axisColors.push_back(FVector4(0.0f, 1.0f, 0.0f, 1.0f));  // 초록
    axisIndices.push_back(2);
    axisIndices.push_back(3);

    // Z축 (파랑)
    axisVertices.push_back(FVector(0.0f, 0.0f, 0.0f));       // 원점
    axisVertices.push_back(FVector(0.0f, 0.0f, Length));     // +Z
    axisColors.push_back(FVector4(0.0f, 0.0f, 1.0f, 1.0f));  // 파랑
    axisColors.push_back(FVector4(0.0f, 0.0f, 1.0f, 1.0f));  // 파랑
    axisIndices.push_back(4);
    axisIndices.push_back(5);

    FMeshData* MeshData = new FMeshData();
    MeshData->Vertices = axisVertices;
    MeshData->Color = axisColors;
    MeshData->Indices = axisIndices;

    UStaticMesh* Mesh = NewObject<UStaticMesh>();
    Mesh->Load(MeshData, Device);
    Add<UStaticMesh>("Axis", Mesh);

    UMeshLoader::GetInstance().AddMeshData("Axis", MeshData);
}

void UResourceManager::CreateTextBillboardMesh()
{
    TArray<uint32> Indices;
    for (uint32 i = 0;i < 100;i++)
    {
        Indices.push_back(i * 4 + 0);
        Indices.push_back(i * 4 + 1);
        Indices.push_back(i * 4 + 2);

        Indices.push_back(i * 4 + 2);
        Indices.push_back(i * 4 + 1);
        Indices.push_back(i * 4 + 3);
    }


    //if(UResourceManager::GetInstance().GetInstance<UMaterial>())
    const uint32 MaxQuads = 100; // capacity
    FMeshData* BillboardData = new FMeshData;
    BillboardData->Indices = Indices;
    // Reserve capacity for MaxQuads (4 vertices per quad)
    BillboardData->Vertices.resize(MaxQuads * 4);
    BillboardData->Color.resize(MaxQuads * 4);
    BillboardData->UV.resize(MaxQuads * 4);

    UQuad* Mesh = NewObject<UQuad>();
    Mesh->Load(BillboardData, Device, true);
    Add<UQuad>("TextBillboard", Mesh);
    UMeshLoader::GetInstance().AddMeshData("TextBillboard", BillboardData);
}

// 단일 Quad
void UResourceManager::CreateBillboardMesh()
{
    // Quad 인덱스 (삼각형 2개)
    TArray<uint32> Indices;
    Indices.push_back(0); Indices.push_back(1); Indices.push_back(2);
    Indices.push_back(0); Indices.push_back(2); Indices.push_back(3);

    // 메시 데이터 준비
    FMeshData* BillboardData = new FMeshData;
    BillboardData->Indices = Indices;

    // 정점 4개 (Quad)
    BillboardData->Vertices.resize(4);
    BillboardData->Color.resize(4);
    BillboardData->UV.resize(4);

    // 로컬 좌표계 기준 Quad (-0.5~0.5)
    BillboardData->Vertices[0] = { -0.5f, -0.5f, 0.0f }; // left-bottom
    BillboardData->Vertices[1] = { -0.5f,  0.5f, 0.0f }; // left-top
    BillboardData->Vertices[2] = { 0.5f,  0.5f, 0.0f }; // right-top
    BillboardData->Vertices[3] = { 0.5f, -0.5f, 0.0f }; // right-bottom

    // UV (0~1)
    BillboardData->UV[0] = { 0.0f, 1.0f };
    BillboardData->UV[1] = { 0.0f, 0.0f };
    BillboardData->UV[2] = { 1.0f, 0.0f };
    BillboardData->UV[3] = { 1.0f, 1.0f };

    // 색상 (기본 흰색)
    for (int i = 0; i < 4; i++)
        BillboardData->Color[i] = { 1.0f, 1.0f, 1.0f, 1.0f };

    // GPU 리소스 생성
    UQuad* Mesh = NewObject<UQuad>();
    Mesh->Load(BillboardData, Device);

    // 리소스 매니저에 등록
    Add<UQuad>("BillboardQuad", Mesh);

    // CPU 데이터 캐싱 (선택)
    UMeshLoader::GetInstance().AddMeshData("BillboardQuad", BillboardData);
}

void UResourceManager::CreateGridMesh(int N, const FString& FilePath)
{
    if (ResourceMap[FilePath])
    {
        return;
    }
    TArray<FVector> gridVertices;
    TArray<FVector4> gridColors;
    TArray<uint32> gridIndices;
    // Z축 방향 선
    for (int i = -N; i <= N; i++)
    {
        if (i == 0)
        {
            continue;
        }
        // 색 결정: 5의 배수면 흰색, 아니면 회색
        float r = 0.1f, g = 0.1f, b = 0.1f;
        if (i % 5 == 0) { r = g = b = 0.4f; }
        if (i % 10 == 0) { r = g = b = 1.0f; }

        // 정점 2개 추가 (Z축 방향 라인)
        gridVertices.push_back(FVector((float)i, 0.0f, (float)-N));
        gridVertices.push_back(FVector((float)i, 0.0f, (float)N));
        gridColors.push_back(FVector4(r, g, b, 1.0f));
        gridColors.push_back(FVector4(r, g, b, 1.0f));

        // 인덱스 추가
        uint32 base = static_cast<uint32>(gridVertices.size());
        gridIndices.push_back(base - 2);
        gridIndices.push_back(base - 1);
    }

    // X축 방향 선
    for (int j = -N; j <= N; j++)
    {
        if (j == 0)
        {
            continue;
        }
        // 색 결정: 5의 배수면 흰색, 아니면 회색
        float r = 0.1f, g = 0.1f, b = 0.1f;

        if (j % 5 == 0) { r = g = b = 0.4f; }
        if (j % 10 == 0) { r = g = b = 1.0f; }

        // 정점 2개 추가 (X축 방향 라인)
        gridVertices.push_back(FVector((float)-N, 0.0f, (float)j));
        gridVertices.push_back(FVector((float)N, 0.0f, (float)j));
        gridColors.push_back(FVector4(r, g, b, 1.0f));
        gridColors.push_back(FVector4(r, g, b, 1.0f));

        // 인덱스 추가
        uint32 base = static_cast<uint32>(gridVertices.size());
        gridIndices.push_back(base - 2);
        gridIndices.push_back(base - 1);
    }

    gridVertices.push_back(FVector(0.0f, 0.0f, (float)-N));
    gridVertices.push_back(FVector(0.0f, 0.0f, 0.0f));
    gridColors.push_back(FVector4(1.0f, 1.0f, 1.0f, 1.0f));
    gridColors.push_back(FVector4(1.0f, 1.0f, 1.0f, 1.0f));
    uint32 base = static_cast<uint32>(gridVertices.size());
    gridIndices.push_back(base - 2);
    gridIndices.push_back(base - 1);

    gridVertices.push_back(FVector((float)-N, 0.0f, 0.0f));
    gridVertices.push_back(FVector(0.0f, 0.0f, 0.0f));
    gridColors.push_back(FVector4(1.0f, 1.0f, 1.0f, 1.0f));
    gridColors.push_back(FVector4(1.0f, 1.0f, 1.0f, 1.0f));
    base = static_cast<uint32>(gridVertices.size());
    gridIndices.push_back(base - 2);
    gridIndices.push_back(base - 1);

    FMeshData* MeshData = new FMeshData();
    MeshData->Vertices = gridVertices;
    MeshData->Color = gridColors;
    MeshData->Indices = gridIndices;

    UStaticMesh* Mesh = NewObject<UStaticMesh>();
    Mesh->Load(MeshData, Device);
    Add<UStaticMesh>("Grid", Mesh);

    UMeshLoader::GetInstance().AddMeshData("Grid", MeshData);
}

void UResourceManager::CreateBoxWireframeMesh(const FVector& Min, const FVector& Max, const FString& FilePath)
{
    // 이미 있으면 패스
    if (ResourceMap[FilePath])
    {
        return;
    }

    TArray<FVector> vertices;
    TArray<FVector4> colors;
    TArray<uint32> indices;

    // ─────────────────────────────
    // 8개의 꼭짓점 (AABB)
    // ─────────────────────────────
    vertices.push_back(FVector(Min.X, Min.Y, Min.Z)); // 0
    vertices.push_back(FVector(Max.X, Min.Y, Min.Z)); // 1
    vertices.push_back(FVector(Max.X, Max.Y, Min.Z)); // 2
    vertices.push_back(FVector(Min.X, Max.Y, Min.Z)); // 3
    vertices.push_back(FVector(Min.X, Min.Y, Max.Z)); // 4
    vertices.push_back(FVector(Max.X, Min.Y, Max.Z)); // 5
    vertices.push_back(FVector(Max.X, Max.Y, Max.Z)); // 6
    vertices.push_back(FVector(Min.X, Max.Y, Max.Z)); // 7

    // 색상 (디버깅용 흰색)
    for (int i = 0; i < 8; i++)
    {
        colors.push_back(FVector4(1.0f, 1.0f, 1.0f, 1.0f));
    }

    // ─────────────────────────────
    // 12개의 선 (24 인덱스)
    // ─────────────────────────────
    uint32 boxIndices[] = {
        // 바닥
        0, 1, 1, 2, 2, 3, 3, 0,
        // 천장
        4, 5, 5, 6, 6, 7, 7, 4,
        // 기둥
        0, 4, 1, 5, 2, 6, 3, 7
    };

    indices.insert(indices.end(), std::begin(boxIndices), std::end(boxIndices));

    // ─────────────────────────────
    // MeshData 생성 및 등록
    // ─────────────────────────────
    FMeshData* MeshData = new FMeshData();
    MeshData->Vertices = vertices;
    MeshData->Color = colors;
    MeshData->Indices = indices;

    UStaticMesh* Mesh = NewObject<UStaticMesh>();
    Mesh->Load(MeshData, Device);
    //Mesh->SetTopology(EPrimitiveTopology::LineList); // ✅ 꼭 LineList로 설정

    Add<UStaticMesh>(FilePath, Mesh);

    UMeshLoader::GetInstance().AddMeshData(FilePath, MeshData);
}

void UResourceManager::CreateDefaultShader()
{
    // 템플릿 Load 멤버함수 호출해서 Resources[UShader의 typeIndex][shader 파일 이름]에 UShader 포인터 할당
    Load<UShader>("Shaders/Primitives/Primitive.hlsl");
    Load<UShader>("Shaders/UI/Gizmo.hlsl");
    Load<UShader>("Shaders/UI/TextBillboard.hlsl");
    Load<UShader>("Shaders/UI/Billboard.hlsl");
    Load<UShader>("Shaders/Materials/Fireball.hlsl");
}

void UResourceManager::CreateDefaultMaterial()
{
    // 1. NewObject<UMaterial>()로 인스턴스 생성
    DefaultMaterialInstance = NewObject<UMaterial>();
    // 2. 엔진이 사용할 고유 경로(이름) 설정
    FString ShaderPath = "Shaders/Materials/UberLit.hlsl";
    UShader* DefaultShader = UResourceManager::GetInstance().Load<UShader>(ShaderPath);
    FString NewName = ShaderPath;
    DefaultMaterialInstance->SetMaterialName(NewName);
    DefaultMaterialInstance->SetShader(DefaultShader);

    // (참고: UMaterial에 SetMaterialName 같은 함수가 없다면 MaterialInfo.MaterialName을 직접 설정)
    // DefaultMaterialInstance->GetMaterialInfo().MaterialName = DefaultMaterialName;
    // 3. 리소스 매니저에 "UMaterial" 타입으로 등록 (핵심)
    Add<UMaterial>(NewName, DefaultMaterialInstance);
}

void UResourceManager::InitShaderILMap()
{
    TArray<D3D11_INPUT_ELEMENT_DESC> layout;

    layout.Add({ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 });
    layout.Add({ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 });
    ShaderToInputLayoutMap["Shaders/UI/Gizmo.hlsl"] = layout;
	layout.clear();

    layout.Add({ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 });
    layout.Add({ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0 });
    ShaderToInputLayoutMap["Shaders/UI/ShaderLine.hlsl"] = layout;
    ShaderToInputLayoutMap["Shaders/Primitives/Primitive.hlsl"] = layout;
    layout.clear();

    layout.Add({ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 });
    layout.Add({ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 });
    layout.Add({ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 });
    layout.Add({ "TANGENT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 32, D3D11_INPUT_PER_VERTEX_DATA, 0 });
    layout.Add({ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 48, D3D11_INPUT_PER_VERTEX_DATA, 0 });
   
    ShaderToInputLayoutMap["Shaders/Effects/Decal.hlsl"] = layout;
	ShaderToInputLayoutMap["Shaders/Materials/UberLit.hlsl"] = layout;
	ShaderToInputLayoutMap["Shaders/Materials/Fireball.hlsl"] = layout; // Use same vertex format as UberLit
	ShaderToInputLayoutMap["Shaders/Shadow/PointLightShadow.hlsl"] = layout;  // Shadow map rendering uses same vertex format
	ShaderToInputLayoutMap["Shaders/Shadows/DepthOnly_VS.hlsl"] = layout;
	ShaderToInputLayoutMap["Shaders/Debug/DebugPrimitive.hlsl"] = layout;  // Debug primitive rendering (Physics Body visualization)
    layout.clear();

    layout.Add({ "WORLDPOSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 });
    layout.Add({ "SIZE", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 });
    layout.Add({ "UVRECT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 20, D3D11_INPUT_PER_VERTEX_DATA, 0 });
    ShaderToInputLayoutMap["Shaders/UI/TextBillboard.hlsl"] = layout;
    layout.clear();

    // ────────────────────────────────
    // 일반 빌보드 (Position + UV)
    // ────────────────────────────────
    layout.Add({ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
                 D3D11_INPUT_PER_VERTEX_DATA, 0 });
    layout.Add({ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 12,
                 D3D11_INPUT_PER_VERTEX_DATA, 0 });
    ShaderToInputLayoutMap["Shaders/UI/Billboard.hlsl"] = layout;
    ShaderToInputLayoutMap["Shaders/Skybox/Skybox.hlsl"] = layout;  // 스카이박스도 동일한 레이아웃 사용
    layout.clear();
    

    // ────────────────────────────────
    // Quad 렌더링을 쓰는 Shader들 (Position + UV)
    // ────────────────────────────────
    layout.Add({ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
                 D3D11_INPUT_PER_VERTEX_DATA, 0 });
    layout.Add({ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 48,
                 D3D11_INPUT_PER_VERTEX_DATA, 0 });
    ShaderToInputLayoutMap["Shaders/PostProcess/HeightFog_PS.hlsl"] = layout;
    ShaderToInputLayoutMap["Shaders/Utility/SceneDepth_PS.hlsl"] = layout;
    layout.clear();
    
    ShaderToInputLayoutMap[UResourceManager::FullScreenVSPath] = {};  // FullScreenTriangle 는 InputLayout을 사용하지 않는다

    // ────────────────────────────────
    // 파티클 스프라이트 인스턴싱
    // 슬롯 0: 쿼드 버텍스 (FSpriteQuadVertex - UV만 포함)
    // 슬롯 1: 인스턴스 데이터 (FSpriteParticleInstanceVertex - 48 bytes)
    // ────────────────────────────────
    // 슬롯 0: 쿼드 버텍스 (Per-Vertex)
    layout.Add({ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 });       // UV
    // 슬롯 1: 인스턴스 데이터 (Per-Instance) - 표준 시맨틱 사용
    layout.Add({ "TEXCOORD", 1, DXGI_FORMAT_R32G32B32_FLOAT, 1, 0, D3D11_INPUT_PER_INSTANCE_DATA, 1 });   // WorldPosition (12 bytes)
    layout.Add({ "TEXCOORD", 2, DXGI_FORMAT_R32_FLOAT, 1, 12, D3D11_INPUT_PER_INSTANCE_DATA, 1 });        // Rotation (4 bytes)
    layout.Add({ "TEXCOORD", 3, DXGI_FORMAT_R32G32_FLOAT, 1, 16, D3D11_INPUT_PER_INSTANCE_DATA, 1 });     // Size (8 bytes)
    layout.Add({ "COLOR", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 24, D3D11_INPUT_PER_INSTANCE_DATA, 1 });  // Color (16 bytes)
    layout.Add({ "TEXCOORD", 4, DXGI_FORMAT_R32_FLOAT, 1, 40, D3D11_INPUT_PER_INSTANCE_DATA, 1 });        // RelativeTime (4 bytes)
    layout.Add({ "TEXCOORD", 5, DXGI_FORMAT_R32_FLOAT, 1, 44, D3D11_INPUT_PER_INSTANCE_DATA, 1 });        // SubImageIndex (4 bytes) - Sub-UV
    ShaderToInputLayoutMap["Shaders/Particle/ParticleSprite.hlsl"] = layout;
    layout.clear();

    // ────────────────────────────────
    // 파티클 메시 인스턴싱
    // 슬롯 0: 메시 버텍스 (FVertexDynamic)
    // 슬롯 1: 인스턴스 데이터 (FMeshParticleInstanceVertex)
    // ────────────────────────────────
    // 슬롯 0: 메시 버텍스 (Per-Vertex)
    layout.Add({ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 });     // Position
    layout.Add({ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 });      // Normal
    layout.Add({ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 });       // UV
    layout.Add({ "TANGENT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 32, D3D11_INPUT_PER_VERTEX_DATA, 0 });  // Tangent
    layout.Add({ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 48, D3D11_INPUT_PER_VERTEX_DATA, 0 });    // VertexColor
    // 슬롯 1: 인스턴스 데이터 (Per-Instance) - 표준 시맨틱 사용
    layout.Add({ "COLOR", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 0, D3D11_INPUT_PER_INSTANCE_DATA, 1 });       // InstanceColor (16 bytes)
    layout.Add({ "TEXCOORD", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 16, D3D11_INPUT_PER_INSTANCE_DATA, 1 }); // Transform[0] (16 bytes)
    layout.Add({ "TEXCOORD", 2, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 32, D3D11_INPUT_PER_INSTANCE_DATA, 1 }); // Transform[1] (16 bytes)
    layout.Add({ "TEXCOORD", 3, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 48, D3D11_INPUT_PER_INSTANCE_DATA, 1 }); // Transform[2] (16 bytes)
    layout.Add({ "TEXCOORD", 4, DXGI_FORMAT_R32_FLOAT, 1, 64, D3D11_INPUT_PER_INSTANCE_DATA, 1 });                // RelativeTime (4 bytes)
    ShaderToInputLayoutMap["Shaders/Particle/ParticleMesh.hlsl"] = layout;
    layout.clear();

    // ────────────────────────────────
    // 파티클 Beam 전용 InputLayout
    // FParticleBeamVertex 구조체와 1:1 매칭
    // ────────────────────────────────
    layout.Add({ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 });
    layout.Add({ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 });
    layout.Add({ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 20, D3D11_INPUT_PER_VERTEX_DATA, 0 });
    layout.Add({ "TEXCOORD", 1, DXGI_FORMAT_R32_FLOAT, 0, 36, D3D11_INPUT_PER_VERTEX_DATA, 0 });
    ShaderToInputLayoutMap["Shaders/Particle/ParticleBeam.hlsl"] = layout;
    layout.clear();

    // ────────────────────────────────
    // 파티클 Ribbon 전용 InputLayout
    // FParticleRibbonVertex 구조체와 1:1 매칭
    // ────────────────────────────────
    layout.Add({ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 });
    layout.Add({ "CONTROLPOINT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 });
    layout.Add({ "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 });
    layout.Add({ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 36, D3D11_INPUT_PER_VERTEX_DATA, 0 });
    layout.Add({ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 52, D3D11_INPUT_PER_VERTEX_DATA, 0 });
    ShaderToInputLayoutMap["Shaders/Particle/ParticleRibbon.hlsl"] = layout;
    layout.clear();
}

TArray<D3D11_INPUT_ELEMENT_DESC>& UResourceManager::GetProperInputLayout(const FString& InShaderName)
{
    auto it = ShaderToInputLayoutMap.find(InShaderName);

    if (it == ShaderToInputLayoutMap.end())
    {
        throw std::runtime_error("Proper input layout not found for " + InShaderName);
    }
    
    return ShaderToInputLayoutMap[InShaderName];
}

FString& UResourceManager::GetProperShader(const FString& InTextureName)
{
    auto it = TextureToShaderMap.find(InTextureName);

    if (it == TextureToShaderMap.end())
    {
        throw std::runtime_error("Proper shader not found for " + InTextureName);
    }

    return TextureToShaderMap[InTextureName];
}

void UResourceManager::InitTexToShaderMap()
{
    TextureToShaderMap["TextBillboard.dds"] = "TextBillboard.hlsl";
}


void UResourceManager::CreateTextBillboardTexture()
{
    UTexture* TextBillboardTexture = NewObject<UTexture>();
    TextBillboardTexture->Load("TextBillboard.dds",Device);
    Add<UTexture>("TextBillboard.dds", TextBillboardTexture);
}

void UResourceManager::CheckAndReloadShaders(float DeltaTime)
{
    // Throttle check frequency to reduce file system overhead
    ShaderCheckTimer += DeltaTime;
    if (ShaderCheckTimer < ShaderCheckInterval)
    {
        return;
    }
    ShaderCheckTimer = 0.0f;

    // Get all shader resources
    uint8 ShaderTypeIndex = static_cast<uint8>(EResourceType::Shader);
    if (ShaderTypeIndex >= Resources.size())
    {
        return;
    }

    // Check each shader for modifications
    TArray<UShader*> ShadersToReload;
    for (auto& Pair : Resources[ShaderTypeIndex])
    {
        UShader* Shader = static_cast<UShader*>(Pair.second);
        if (Shader && Shader->IsOutdated())
        {
            ShadersToReload.push_back(Shader);
        }
    }

    // Early exit if no shaders need reloading
    if (ShadersToReload.empty())
    {
        return;
    }

    // Ensure device context flushes before reloading
    if (Context)
    {
        Context->Flush();
    }

    // Reload outdated shaders
    for (UShader* Shader : ShadersToReload)
    {
        // Store old state to check if reload was successful
        bool bHadVertexShader = (Shader->GetVertexShader() != nullptr);
        bool bHadPixelShader = (Shader->GetPixelShader() != nullptr);

        if (Shader->Reload(Device))
        {
            // Verify the reload actually created the expected shader stages
            bool bHasVertexShader = (Shader->GetVertexShader() != nullptr);
            bool bHasPixelShader = (Shader->GetPixelShader() != nullptr);
            
            if ((bHadVertexShader && !bHasVertexShader) || (bHadPixelShader && !bHasPixelShader))
            {
                UE_LOG("Shader Hot Reload Warning: Some stages failed for %s", Shader->GetFilePath().c_str());
            }
            else
            {
                UE_LOG("Shader Hot Reload Successful: %s", Shader->GetFilePath().c_str());
            }
        }
        else
        {
            UE_LOG("Shader Hot Reload Failed: %s (keeping old shader)", Shader->GetFilePath().c_str());
        }
    }
}

void UResourceManager::UpdateDynamicVertexBuffer(const FString& Name, TArray<FBillboardVertexInfo_GPU>& vertices)
{
    UQuad* Mesh = Get<UQuad>(Name);

    const uint32_t quadCount = static_cast<uint32_t>(vertices.size() / 4);
    Mesh->SetIndexCount(quadCount * 6);

    D3D11_MAPPED_SUBRESOURCE mappedResource;
    Context->Map(Mesh->GetVertexBuffer(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
    memcpy(mappedResource.pData, vertices.data(), sizeof(FBillboardVertexInfo_GPU) * vertices.size()); //vertices.size()만큼의 Character info를 vertices에서 pData로 복사해가라
    Context->Unmap(Mesh->GetVertexBuffer(), 0);
}

UMaterial* UResourceManager::GetDefaultMaterial()
{
    return DefaultMaterialInstance;
}

// 여기서 텍스처 데이터 로드 및 
FTextureData* UResourceManager::CreateOrGetTextureData(const FWideString& FilePath)
{
    auto it = TextureMap.find(FilePath);
    if (it != TextureMap.end())
    {
        return it->second;
    }

    FTextureData* Data = new FTextureData();

    // 확장자 판별 (안전)
    std::filesystem::path realPath(FilePath);
    std::wstring ext = realPath.has_extension() ? realPath.extension().wstring() : L"";
for (auto& ch : ext) ch = static_cast<wchar_t>(::towlower(ch));

    HRESULT hr = E_FAIL;
    if (ext == L".dds")
    {
        hr = DirectX::CreateDDSTextureFromFile(Device, FilePath.c_str(), &Data->Texture, &Data->TextureSRV, 0, nullptr);
    }
    else
    {
        hr = DirectX::CreateWICTextureFromFile(Device, Context, FilePath.c_str(), &Data->Texture, &Data->TextureSRV);
    }

    if (FAILED(hr) || Data->TextureSRV == nullptr)
    {
        // Fallback: Data 디렉토리 아래에서 파일명 일치 검색
        std::filesystem::path fname = std::filesystem::path(FilePath).filename();
        std::filesystem::path dataRoot = std::filesystem::absolute(UTF8ToWide(GDataDir));
        bool retried = false;
        try {
            for (auto& entry : std::filesystem::recursive_directory_iterator(dataRoot))
            {
                if (!entry.is_regular_file()) continue;
                if (entry.path().filename() == fname)
                {
                    retried = true;
                    if (ext == L".dds")
                        hr = DirectX::CreateDDSTextureFromFile(Device, entry.path().c_str(), &Data->Texture, &Data->TextureSRV, 0, nullptr);
                    else
                        hr = DirectX::CreateWICTextureFromFile(Device, Context, entry.path().c_str(), &Data->Texture, &Data->TextureSRV);
                    if (SUCCEEDED(hr) && Data->TextureSRV) break;
                }
            }
        } catch(...) {}

        if (!retried || FAILED(hr) || Data->TextureSRV == nullptr)
        {
            if (Data->Texture) { Data->Texture->Release(); Data->Texture = nullptr; }
            if (Data->BlendState) { Data->BlendState->Release(); Data->BlendState = nullptr; }
            delete Data;
            UE_LOG("CreateOrGetTextureData failed: %ls\r\n", FilePath.c_str());
            return nullptr; // 실패 시 맵에 넣지 않음
        }
    }

    D3D11_BLEND_DESC blendDesc{};
    blendDesc.RenderTarget[0].BlendEnable = TRUE;
    blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
    blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

    if (FAILED(Device->CreateBlendState(&blendDesc, &Data->BlendState)))
    {
        Data->BlendState = nullptr;
    }

    TextureMap[FilePath] = Data;
    return Data;
}

// ============================================================
// Physics Shape 프리미티브 메시 생성 함수들
// ============================================================

static const float PRIM_PI = 3.14159265358979323846f;

UStaticMesh* UResourceManager::CreateUnitSphereMesh(int32 Segments, int32 Rings)
{
    FMeshData* MeshData = new FMeshData();

    // 정점 생성 (UV sphere)
    for (int32 ring = 0; ring <= Rings; ++ring)
    {
        float phi = PRIM_PI * ring / Rings;  // 0 ~ PI
        float y = cosf(phi);
        float ringRadius = sinf(phi);

        for (int32 seg = 0; seg <= Segments; ++seg)
        {
            float theta = 2.0f * PRIM_PI * seg / Segments;  // 0 ~ 2PI
            float x = ringRadius * cosf(theta);
            float z = ringRadius * sinf(theta);

            MeshData->Vertices.Add(FVector(x, y, z));
            MeshData->Normal.Add(FVector(x, y, z));  // 단위구이므로 정점=노말
            MeshData->UV.Add(FVector2D((float)seg / Segments, (float)ring / Rings));
            MeshData->Color.Add(FVector4(1.0f, 1.0f, 1.0f, 1.0f));
        }
    }

    // 인덱스 생성 (삼각형)
    for (int32 ring = 0; ring < Rings; ++ring)
    {
        for (int32 seg = 0; seg < Segments; ++seg)
        {
            int32 current = ring * (Segments + 1) + seg;
            int32 next = current + Segments + 1;

            // 첫 번째 삼각형
            MeshData->Indices.Add(current);
            MeshData->Indices.Add(next);
            MeshData->Indices.Add(current + 1);

            // 두 번째 삼각형
            MeshData->Indices.Add(current + 1);
            MeshData->Indices.Add(next);
            MeshData->Indices.Add(next + 1);
        }
    }

    UStaticMesh* Mesh = NewObject<UStaticMesh>();
    Mesh->Load(MeshData, Device, EVertexLayoutType::PositionColorTexturNormal);
    Add<UStaticMesh>("__PrimitiveSphere", Mesh);

    delete MeshData;
    return Mesh;
}

UStaticMesh* UResourceManager::CreateUnitCapsuleMesh(int32 Segments, int32 Rings)
{
    FMeshData* MeshData = new FMeshData();

    // 캡슐 = 상단 반구 + 실린더 + 하단 반구
    // 단위 캡슐: 반지름 1.0, 실린더 반높이 1.0 - Scale = (Radius, HalfHeight, Radius)로 직관적 사용
    const float Radius = 1.0f;
    const float HalfHeight = 1.0f;  // 실린더 반높이

    int32 HemiRings = Rings / 2;

    // --- 상단 반구 ---
    for (int32 ring = 0; ring <= HemiRings; ++ring)
    {
        float phi = (PRIM_PI / 2.0f) * ring / HemiRings;  // 0 ~ PI/2
        float y = cosf(phi) * Radius + HalfHeight;
        float ringRadius = sinf(phi) * Radius;

        for (int32 seg = 0; seg <= Segments; ++seg)
        {
            float theta = 2.0f * PRIM_PI * seg / Segments;
            float x = ringRadius * cosf(theta);
            float z = ringRadius * sinf(theta);

            FVector normal(sinf(phi) * cosf(theta), cosf(phi), sinf(phi) * sinf(theta));

            MeshData->Vertices.Add(FVector(x, y, z));
            MeshData->Normal.Add(normal);
            MeshData->UV.Add(FVector2D((float)seg / Segments, (float)ring / (Rings + 2)));
            MeshData->Color.Add(FVector4(1.0f, 1.0f, 1.0f, 1.0f));
        }
    }

    int32 topCapEnd = (int32)MeshData->Vertices.Num();

    // --- 하단 반구 ---
    for (int32 ring = 0; ring <= HemiRings; ++ring)
    {
        float phi = (PRIM_PI / 2.0f) + (PRIM_PI / 2.0f) * ring / HemiRings;  // PI/2 ~ PI
        float y = cosf(phi) * Radius - HalfHeight;
        float ringRadius = sinf(phi) * Radius;

        for (int32 seg = 0; seg <= Segments; ++seg)
        {
            float theta = 2.0f * PRIM_PI * seg / Segments;
            float x = ringRadius * cosf(theta);
            float z = ringRadius * sinf(theta);

            FVector normal(sinf(phi) * cosf(theta), cosf(phi), sinf(phi) * sinf(theta));

            MeshData->Vertices.Add(FVector(x, y, z));
            MeshData->Normal.Add(normal);
            MeshData->UV.Add(FVector2D((float)seg / Segments, (float)(ring + HemiRings + 1) / (Rings + 2)));
            MeshData->Color.Add(FVector4(1.0f, 1.0f, 1.0f, 1.0f));
        }
    }

    // --- 인덱스 생성 ---
    // 상단 반구
    for (int32 ring = 0; ring < HemiRings; ++ring)
    {
        for (int32 seg = 0; seg < Segments; ++seg)
        {
            int32 current = ring * (Segments + 1) + seg;
            int32 next = current + Segments + 1;

            MeshData->Indices.Add(current);
            MeshData->Indices.Add(next);
            MeshData->Indices.Add(current + 1);

            MeshData->Indices.Add(current + 1);
            MeshData->Indices.Add(next);
            MeshData->Indices.Add(next + 1);
        }
    }

    // 실린더 연결 (상단 반구 끝 ~ 하단 반구 시작)
    int32 topRingStart = HemiRings * (Segments + 1);
    int32 bottomRingStart = topCapEnd;
    for (int32 seg = 0; seg < Segments; ++seg)
    {
        int32 t0 = topRingStart + seg;
        int32 t1 = topRingStart + seg + 1;
        int32 b0 = bottomRingStart + seg;
        int32 b1 = bottomRingStart + seg + 1;

        MeshData->Indices.Add(t0);
        MeshData->Indices.Add(b0);
        MeshData->Indices.Add(t1);

        MeshData->Indices.Add(t1);
        MeshData->Indices.Add(b0);
        MeshData->Indices.Add(b1);
    }

    // 하단 반구
    for (int32 ring = 0; ring < HemiRings; ++ring)
    {
        for (int32 seg = 0; seg < Segments; ++seg)
        {
            int32 current = topCapEnd + ring * (Segments + 1) + seg;
            int32 next = current + Segments + 1;

            MeshData->Indices.Add(current);
            MeshData->Indices.Add(next);
            MeshData->Indices.Add(current + 1);

            MeshData->Indices.Add(current + 1);
            MeshData->Indices.Add(next);
            MeshData->Indices.Add(next + 1);
        }
    }

    UStaticMesh* Mesh = NewObject<UStaticMesh>();
    Mesh->Load(MeshData, Device, EVertexLayoutType::PositionColorTexturNormal);
    Add<UStaticMesh>("__PrimitiveCapsule", Mesh);

    delete MeshData;
    return Mesh;
}

UStaticMesh* UResourceManager::CreateUnitBoxMesh()
{
    FMeshData* MeshData = new FMeshData();

    // 단위 박스 (-1 ~ 1) - Scale = HalfExtent로 직관적으로 사용 가능
    const float h = 1.0f;

    // 6면 x 4정점 = 24정점 (각 면마다 다른 노말)
    FVector positions[24] = {
        // Front (+Z)
        {-h, -h, h}, {h, -h, h}, {h, h, h}, {-h, h, h},
        // Back (-Z)
        {h, -h, -h}, {-h, -h, -h}, {-h, h, -h}, {h, h, -h},
        // Left (-X)
        {-h, -h, -h}, {-h, -h, h}, {-h, h, h}, {-h, h, -h},
        // Right (+X)
        {h, -h, h}, {h, -h, -h}, {h, h, -h}, {h, h, h},
        // Top (+Y)
        {-h, h, h}, {h, h, h}, {h, h, -h}, {-h, h, -h},
        // Bottom (-Y)
        {-h, -h, -h}, {h, -h, -h}, {h, -h, h}, {-h, -h, h}
    };

    FVector normals[6] = {
        {0, 0, 1}, {0, 0, -1}, {-1, 0, 0}, {1, 0, 0}, {0, 1, 0}, {0, -1, 0}
    };

    for (int32 face = 0; face < 6; ++face)
    {
        for (int32 v = 0; v < 4; ++v)
        {
            MeshData->Vertices.Add(positions[face * 4 + v]);
            MeshData->Normal.Add(normals[face]);
            MeshData->Color.Add(FVector4(1.0f, 1.0f, 1.0f, 1.0f));
        }
        MeshData->UV.Add(FVector2D(0, 1));
        MeshData->UV.Add(FVector2D(1, 1));
        MeshData->UV.Add(FVector2D(1, 0));
        MeshData->UV.Add(FVector2D(0, 0));
    }

    // 인덱스 (각 면 2개 삼각형)
    for (int32 face = 0; face < 6; ++face)
    {
        int32 base = face * 4;
        MeshData->Indices.Add(base + 0);
        MeshData->Indices.Add(base + 1);
        MeshData->Indices.Add(base + 2);
        MeshData->Indices.Add(base + 0);
        MeshData->Indices.Add(base + 2);
        MeshData->Indices.Add(base + 3);
    }

    UStaticMesh* Mesh = NewObject<UStaticMesh>();
    Mesh->Load(MeshData, Device, EVertexLayoutType::PositionColorTexturNormal);
    Add<UStaticMesh>("__PrimitiveBox", Mesh);

    delete MeshData;
    return Mesh;
}

UStaticMesh* UResourceManager::CreateUnitConeMesh(int32 Segments)
{
    FMeshData* MeshData = new FMeshData();

    // 단일 원뿔 (Double-sided): 꼭지점이 원점, 밑면이 +X 방향
    const float Height = 1.0f;
    const float Radius = 1.0f;

    // 원뿔 꼭지점 (원점)
    FVector ApexPos(0.0f, 0.0f, 0.0f);

    // 꼭지점 추가 (인덱스 0)
    MeshData->Vertices.Add(ApexPos);
    MeshData->Normal.Add(FVector(-1.0f, 0.0f, 0.0f));  // 원뿔 축 방향
    MeshData->UV.Add(FVector2D(0.5f, 0.0f));
    MeshData->Color.Add(FVector4(1.0f, 1.0f, 1.0f, 1.0f));

    // 밑면 원의 정점들 (인덱스 1 ~ Segments)
    for (int32 i = 0; i < Segments; ++i)
    {
        float Angle = 2.0f * PRIM_PI * i / Segments;
        float Y = Radius * cosf(Angle);
        float Z = Radius * sinf(Angle);

        FVector BasePoint(Height, Y, Z);

        // 노말 계산 (측면 노말)
        FVector ToBase = (BasePoint - ApexPos).GetSafeNormal();
        FVector Tangent = FVector(0.0f, -sinf(Angle), cosf(Angle));
        FVector Normal = FVector::Cross(ToBase, Tangent).GetSafeNormal();

        MeshData->Vertices.Add(BasePoint);
        MeshData->Normal.Add(Normal);
        MeshData->UV.Add(FVector2D((float)i / Segments, 1.0f));
        MeshData->Color.Add(FVector4(1.0f, 1.0f, 1.0f, 1.0f));
    }

    // 원뿔 측면 삼각형 (앞면 - CCW)
    for (int32 i = 0; i < Segments; ++i)
    {
        int32 Current = i + 1;
        int32 Next = (i + 1) % Segments + 1;

        // 앞면 (CCW)
        MeshData->Indices.Add(0);       // Apex
        MeshData->Indices.Add(Current); // Current base point
        MeshData->Indices.Add(Next);    // Next base point
    }

    // 원뿔 측면 삼각형 (뒷면 - CW, 양면 렌더링용)
    for (int32 i = 0; i < Segments; ++i)
    {
        int32 Current = i + 1;
        int32 Next = (i + 1) % Segments + 1;

        // 뒷면 (CW - 삼각형 순서 반대)
        MeshData->Indices.Add(0);       // Apex
        MeshData->Indices.Add(Next);    // Next base point
        MeshData->Indices.Add(Current); // Current base point
    }

    UStaticMesh* Mesh = NewObject<UStaticMesh>();
    Mesh->Load(MeshData, Device, EVertexLayoutType::PositionColorTexturNormal);
    Add<UStaticMesh>("__PrimitiveCone", Mesh);

    delete MeshData;
    return Mesh;
}

UStaticMesh* UResourceManager::GetOrCreatePrimitiveMesh(const FString& PrimitiveName)
{
    // 내부적으로 사용할 키 이름 생성
    FString InternalName = "__Primitive" + PrimitiveName;

    // 이미 생성된 메시가 있으면 반환
    UStaticMesh* Existing = Get<UStaticMesh>(InternalName);
    if (Existing)
        return Existing;

    // 없으면 생성
    if (PrimitiveName == "Sphere")
        return CreateUnitSphereMesh();
    else if (PrimitiveName == "Capsule")
        return CreateUnitCapsuleMesh();
    else if (PrimitiveName == "Box")
        return CreateUnitBoxMesh();
    else if (PrimitiveName == "Cone")
        return CreateUnitConeMesh();

    return nullptr;
}

UStaticMesh* UResourceManager::GetOrCreateDynamicCapsuleMesh(float CylinderHalfHeight, float Radius, int32 Slices, int32 Stacks)
{
    // 캡슐 메시 동적 생성
    // 0.01 단위로 양자화하여 캐시 키 생성 (정밀도와 캐시 효율성 균형)
    const float QuantizeUnit = 0.01f;
    int32 QuantizedHalfHeight = (int32)(CylinderHalfHeight / QuantizeUnit + 0.5f);
    int32 QuantizedRadius = (int32)(Radius / QuantizeUnit + 0.5f);

    // 캐시 키 생성
    char KeyBuffer[64];
    sprintf_s(KeyBuffer, "__DynamicCapsule_%d_%d", QuantizedHalfHeight, QuantizedRadius);
    FString CacheKey(KeyBuffer);

    // 이미 생성된 메시가 있으면 반환
    UStaticMesh* Existing = Get<UStaticMesh>(CacheKey);
    if (Existing)
        return Existing;

    // 양자화된 값으로 실제 크기 복원
    float ActualHalfHeight = QuantizedHalfHeight * QuantizeUnit;
    float ActualRadius = QuantizedRadius * QuantizeUnit;
    if (ActualRadius < 0.001f) ActualRadius = 0.001f;  // 최소 반지름

    FMeshData* MeshData = new FMeshData();

    // 캡슐 축은 Y축 (엔진 기준)
    float HalfLength = ActualHalfHeight;
    Radius = ActualRadius;

    // --- A. 위쪽 반구 (Top Hemisphere) ---
    for (int32 i = 0; i <= Stacks; ++i)
    {
        float Phi = (PRIM_PI * 0.5f) * ((float)i / Stacks);
        float SinPhi = sinf(Phi);
        float CosPhi = cosf(Phi);
        float YOffset = HalfLength;

        for (int32 j = 0; j <= Slices; ++j)
        {
            float Theta = 2.0f * PRIM_PI * ((float)j / Slices);
            float SinTheta = sinf(Theta);
            float CosTheta = cosf(Theta);

            float x = Radius * SinPhi * CosTheta;
            float z = Radius * SinPhi * SinTheta;
            float y = (Radius * CosPhi) + YOffset;

            FVector normal(SinPhi * CosTheta, CosPhi, SinPhi * SinTheta);

            MeshData->Vertices.Add(FVector(x, y, z));
            MeshData->Normal.Add(normal);
            MeshData->UV.Add(FVector2D((float)j / Slices, (float)i / (Stacks * 2 + 1)));
            MeshData->Color.Add(FVector4(1.0f, 1.0f, 1.0f, 1.0f));
        }
    }

    // --- B. 아래쪽 반구 (Bottom Hemisphere) ---
    for (int32 i = 0; i <= Stacks; ++i)
    {
        float Phi = (PRIM_PI * 0.5f) + (PRIM_PI * 0.5f) * ((float)i / Stacks);
        float SinPhi = sinf(Phi);
        float CosPhi = cosf(Phi);
        float YOffset = -HalfLength;

        for (int32 j = 0; j <= Slices; ++j)
        {
            float Theta = 2.0f * PRIM_PI * ((float)j / Slices);
            float SinTheta = sinf(Theta);
            float CosTheta = cosf(Theta);

            float x = Radius * SinPhi * CosTheta;
            float z = Radius * SinPhi * SinTheta;
            float y = (Radius * CosPhi) + YOffset;

            FVector normal(SinPhi * CosTheta, CosPhi, SinPhi * SinTheta);

            MeshData->Vertices.Add(FVector(x, y, z));
            MeshData->Normal.Add(normal);
            MeshData->UV.Add(FVector2D((float)j / Slices, (float)(i + Stacks + 1) / (Stacks * 2 + 1)));
            MeshData->Color.Add(FVector4(1.0f, 1.0f, 1.0f, 1.0f));
        }
    }

    // --- 인덱스 생성 ---
    int32 RingVertexCount = Slices + 1;
    int32 TotalRings = (Stacks + 1) * 2;

    for (int32 i = 0; i < TotalRings - 1; ++i)
    {
        for (int32 j = 0; j < Slices; ++j)
        {
            int32 Current = i * RingVertexCount + j;
            int32 Next = Current + RingVertexCount;

            MeshData->Indices.Add(Current);
            MeshData->Indices.Add(Next);
            MeshData->Indices.Add(Current + 1);

            MeshData->Indices.Add(Current + 1);
            MeshData->Indices.Add(Next);
            MeshData->Indices.Add(Next + 1);
        }
    }

    UStaticMesh* Mesh = NewObject<UStaticMesh>();
    Mesh->Load(MeshData, Device, EVertexLayoutType::PositionColorTexturNormal);
    Add<UStaticMesh>(CacheKey, Mesh);

    delete MeshData;
    return Mesh;
}

UStaticMesh* UResourceManager::GetOrCreateDynamicArcMesh(float TwistAngle, int32 Segments)
{
    // 각도를 5도 단위로 양자화하여 캐시 키 생성
    const float QuantizeDegrees = 5.0f;
    float AngleDegrees = TwistAngle * 180.0f / PRIM_PI;
    int32 QuantizedAngle = (int32)(AngleDegrees / QuantizeDegrees) * (int32)QuantizeDegrees;
    QuantizedAngle = FMath::Clamp(QuantizedAngle, 0, 180);

    // 캐시 키 생성
    char KeyBuffer[64];
    sprintf_s(KeyBuffer, "__DynamicArc_%d", QuantizedAngle);
    FString CacheKey(KeyBuffer);

    // 이미 생성된 메시가 있으면 반환
    UStaticMesh* Existing = Get<UStaticMesh>(CacheKey);
    if (Existing)
        return Existing;

    // 새 부채꼴 메시 생성
    FMeshData* MeshData = new FMeshData();

    float ActualAngle = (float)QuantizedAngle * PRIM_PI / 180.0f;
    const float Radius = 1.0f;

    // 중심점 (인덱스 0)
    FVector Center(0.0f, 0.0f, 0.0f);
    MeshData->Vertices.Add(Center);
    MeshData->Normal.Add(FVector(1.0f, 0.0f, 0.0f));
    MeshData->UV.Add(FVector2D(0.5f, 0.5f));
    MeshData->Color.Add(FVector4(1.0f, 1.0f, 1.0f, 1.0f));

    // 부채꼴 원호 점들: -ActualAngle ~ +ActualAngle
    for (int32 i = 0; i <= Segments; ++i)
    {
        float t = (float)i / Segments;
        float Angle = -ActualAngle + 2.0f * ActualAngle * t;
        float Y = Radius * cosf(Angle);
        float Z = Radius * sinf(Angle);

        MeshData->Vertices.Add(FVector(0.0f, Y, Z));
        MeshData->Normal.Add(FVector(1.0f, 0.0f, 0.0f));
        MeshData->UV.Add(FVector2D(t, 1.0f));
        MeshData->Color.Add(FVector4(1.0f, 1.0f, 1.0f, 1.0f));
    }

    // 부채꼴 삼각형 인덱스 (양면 렌더링)
    for (int32 i = 0; i < Segments; ++i)
    {
        // 앞면
        MeshData->Indices.Add(0);
        MeshData->Indices.Add(i + 1);
        MeshData->Indices.Add(i + 2);
        // 뒷면
        MeshData->Indices.Add(0);
        MeshData->Indices.Add(i + 2);
        MeshData->Indices.Add(i + 1);
    }

    UStaticMesh* Mesh = NewObject<UStaticMesh>();
    Mesh->Load(MeshData, Device, EVertexLayoutType::PositionColorTexturNormal);
    Add<UStaticMesh>(CacheKey, Mesh);

    delete MeshData;
    return Mesh;
}

UStaticMesh* UResourceManager::GetOrCreateEllipticalConeMesh(float Swing1Angle, float Swing2Angle, int32 Segments)
{
    // 각도를 1도 단위로 양자화하여 캐시 키 생성
    float Angle1Degrees = Swing1Angle * 180.0f / PRIM_PI;
    float Angle2Degrees = Swing2Angle * 180.0f / PRIM_PI;
    int32 QuantizedAngle1 = FMath::Clamp((int32)Angle1Degrees, 0, 180);
    int32 QuantizedAngle2 = FMath::Clamp((int32)Angle2Degrees, 0, 180);

    // 캐시 키 생성
    char KeyBuffer[64];
    sprintf_s(KeyBuffer, "__EllipticalCone_%d_%d", QuantizedAngle1, QuantizedAngle2);
    FString CacheKey(KeyBuffer);

    // 이미 생성된 메시가 있으면 반환
    UStaticMesh* Existing = Get<UStaticMesh>(CacheKey);
    if (Existing)
        return Existing;

    // 새 타원형 원뿔 메시 생성
    FMeshData* MeshData = new FMeshData();

    // 0도(Locked)여도 시각적으로 보일 수 있게 최소값 적용
    const float MinVisualRad = 0.1f * PRIM_PI / 180.0f;  // 0.1도

    float Swing1Rad = (float)QuantizedAngle1 * PRIM_PI / 180.0f;
    float Swing2Rad = (float)QuantizedAngle2 * PRIM_PI / 180.0f;

    float EffectiveSwing1 = FMath::Max(Swing1Rad, MinVisualRad);
    float EffectiveSwing2 = FMath::Max(Swing2Rad, MinVisualRad);

    const float Height = 1.0f;

    // 중심점 (원뿔의 꼭짓점, 인덱스 0)
    MeshData->Vertices.Add(FVector(0.0f, 0.0f, 0.0f));
    MeshData->Normal.Add(FVector(-1.0f, 0.0f, 0.0f));
    MeshData->UV.Add(FVector2D(0.5f, 0.5f));
    MeshData->Color.Add(FVector4(1.0f, 1.0f, 1.0f, 1.0f));

    // 0도 ~ 360도를 돌면서 타원형 한계선을 추적
    for (int32 i = 0; i <= Segments; ++i)
    {
        float Theta = (float)i / (float)Segments * (2.0f * PRIM_PI);

        float DirY = cosf(Theta);
        float DirZ = sinf(Theta);

        // 타원 방정식을 이용해 현재 방향에서의 최대 각도 계산
        float Term1 = (DirY / EffectiveSwing1);
        Term1 *= Term1;

        float Term2 = (DirZ / EffectiveSwing2);
        Term2 *= Term2;

        float MaxAngle = 0.0f;
        float Denominator = Term1 + Term2;

        if (Denominator > 1e-6f)
        {
            MaxAngle = 1.0f / sqrtf(Denominator);
        }

        if (MaxAngle > PRIM_PI) MaxAngle = PRIM_PI;

        // 회전 축 계산
        FVector RotationAxis(0.0f, -DirZ, DirY);
        float AxisLenSq = RotationAxis.Y * RotationAxis.Y + RotationAxis.Z * RotationAxis.Z;

        if (AxisLenSq < 1e-6f)
        {
            RotationAxis = FVector(0.0f, 1.0f, 0.0f);
        }
        else
        {
            float InvLen = 1.0f / sqrtf(AxisLenSq);
            RotationAxis.Y *= InvLen;
            RotationAxis.Z *= InvLen;
        }

        // Angle-Axis로 쿼터니언 생성 후 X축 벡터 회전
        FQuat Q = FQuat::FromAxisAngle(RotationAxis, MaxAngle);
        FVector LocalDir = Q.RotateVector(FVector(1.0f, 0.0f, 0.0f));

        FVector WorldPos = LocalDir * Height;
        FVector Normal = FVector(-LocalDir.X, -LocalDir.Y, -LocalDir.Z);

        MeshData->Vertices.Add(WorldPos);
        MeshData->Normal.Add(Normal);
        MeshData->UV.Add(FVector2D((float)i / Segments, 1.0f));
        MeshData->Color.Add(FVector4(1.0f, 1.0f, 1.0f, 1.0f));
    }

    // 삼각형 인덱스 (Triangle Fan, 양면)
    for (int32 i = 0; i < Segments; ++i)
    {
        // 앞면
        MeshData->Indices.Add(0);
        MeshData->Indices.Add(i + 1);
        MeshData->Indices.Add(i + 2);
        // 뒷면
        MeshData->Indices.Add(0);
        MeshData->Indices.Add(i + 2);
        MeshData->Indices.Add(i + 1);
    }

    UStaticMesh* Mesh = NewObject<UStaticMesh>();
    Mesh->Load(MeshData, Device, EVertexLayoutType::PositionColorTexturNormal);
    Add<UStaticMesh>(CacheKey, Mesh);

    delete MeshData;
    return Mesh;
}
