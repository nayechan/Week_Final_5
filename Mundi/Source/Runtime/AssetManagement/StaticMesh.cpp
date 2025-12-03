#include "pch.h"
#include "StaticMesh.h"
#include "StaticMeshComponent.h"
#include "ObjManager.h"
#include "ResourceManager.h"
#include "FBXLoader.h"
#include "BodySetup.h"
#include "ConvexElem.h"
#include "PhysicsCacheData.h"
#include "WindowsBinReader.h"
#include "WindowsBinWriter.h"
#include "PathUtils.h"
#include "JsonSerializer.h"
#include <filesystem>
#include <fstream>

IMPLEMENT_CLASS(UStaticMesh)

namespace
{
    // FSkeletalMeshData를 FStaticMesh로 변환 (본 정보 제거)
    FStaticMesh* ConvertSkeletalToStaticMesh(const FSkeletalMeshData& SkeletalData)
    {
        FStaticMesh* StaticMesh = new FStaticMesh();

        // 정점 데이터 변환 (FSkinnedVertex -> FNormalVertex)
        StaticMesh->Vertices.reserve(SkeletalData.Vertices.size());
        for (const auto& SkinnedVtx : SkeletalData.Vertices)
        {
            FNormalVertex NormalVtx;
            NormalVtx.pos = SkinnedVtx.Position;
            NormalVtx.normal = SkinnedVtx.Normal;
            NormalVtx.tex = SkinnedVtx.UV;
            NormalVtx.Tangent = SkinnedVtx.Tangent;
            NormalVtx.color = SkinnedVtx.Color;
            StaticMesh->Vertices.push_back(NormalVtx);
        }

        // 인덱스 복사
        StaticMesh->Indices = SkeletalData.Indices;

        // 그룹 정보 복사
        StaticMesh->GroupInfos = SkeletalData.GroupInfos;
        StaticMesh->bHasMaterial = SkeletalData.bHasMaterial;

        // 캐시 경로 복사
        StaticMesh->CacheFilePath = SkeletalData.CacheFilePath;

        return StaticMesh;
    }
}

UStaticMesh::~UStaticMesh()
{
    ReleaseResources();
}

void UStaticMesh::Load(const FString& InFilePath, ID3D11Device* InDevice, EVertexLayoutType InVertexType)
{
    assert(InDevice);

    UE_LOG("[UStaticMesh::Load] 시작: %s", InFilePath.c_str());

    // FilePath를 먼저 설정 (물리 데이터 로드/저장에 필요)
    FilePath = InFilePath;

    SetVertexType(InVertexType);

    // 파일 확장자 확인
    std::filesystem::path FilePath(UTF8ToWide(InFilePath));
    FString Extension = WideToUTF8(FilePath.extension().wstring());
    std::transform(Extension.begin(), Extension.end(), Extension.begin(), ::tolower);

    if (Extension == ".fbx")
    {
        // FBX 파일 로드
        FSkeletalMeshData* SkeletalData = UFbxLoader::GetInstance().LoadFbxMeshAsset(InFilePath);

        if (!SkeletalData || SkeletalData->Vertices.empty() || SkeletalData->Indices.empty())
        {
            UE_LOG("ERROR: Failed to load FBX mesh from '%s'", InFilePath.c_str());
            delete SkeletalData;  // 메모리 누수 방지
            return;
        }

        // SkeletalMeshData를 StaticMesh로 변환
        StaticMeshAsset = ConvertSkeletalToStaticMesh(*SkeletalData);
        StaticMeshAsset->PathFileName = InFilePath;

        // FBX 메시를 ObjManager 캐시에 등록 (메모리 관리)
        FObjManager::RegisterStaticMeshAsset(InFilePath, StaticMeshAsset);
        delete SkeletalData;
    }
    else
    {
        // OBJ 파일 로드 (기존 방식)
        StaticMeshAsset = FObjManager::LoadObjStaticMeshAsset(InFilePath);
    }

    // 빈 버텍스, 인덱스로 버퍼 생성 방지
    if (StaticMeshAsset && 0 < StaticMeshAsset->Vertices.size() && 0 < StaticMeshAsset->Indices.size())
    {
        CacheFilePath = StaticMeshAsset->CacheFilePath;
        CreateVertexBuffer(StaticMeshAsset, InDevice, InVertexType);
        CreateIndexBuffer(StaticMeshAsset, InDevice);
        CreateLocalBound(StaticMeshAsset);
        VertexCount = static_cast<uint32>(StaticMeshAsset->Vertices.size());
        IndexCount = static_cast<uint32>(StaticMeshAsset->Indices.size());

        // Physics 메타데이터 로드 시도 (companion .physics.json 파일이 있으면)
        LoadPhysicsMetadata();

        // BodySetup이 없거나 비어있으면 메시 정점으로부터 기본 Convex 생성
        CreateDefaultConvexIfNeeded();
    }
}

void UStaticMesh::Load(FMeshData* InData, ID3D11Device* InDevice, EVertexLayoutType InVertexType)
{
    SetVertexType(InVertexType);

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
    CreateLocalBound(InData);

    VertexCount = static_cast<uint32>(InData->Vertices.size());
    IndexCount = static_cast<uint32>(InData->Indices.size());
}

void UStaticMesh::SetVertexType(EVertexLayoutType InVertexType)
{
    VertexType = InVertexType;

    uint32 Stride = 0;
    switch (InVertexType)
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
        assert(false && "Unknown vertex type!");
    }

    VertexStride = Stride;
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

void UStaticMesh::CreateLocalBound(const FMeshData* InMeshData)
{
    TArray<FVector> Verts = InMeshData->Vertices;
    FVector Min = Verts[0];
    FVector Max = Verts[0];
    for (FVector Vertex : Verts)
    {
        Min = Min.ComponentMin(Vertex);
        Max = Max.ComponentMax(Vertex);
    }
    LocalBound = FAABB(Min, Max);
}

void UStaticMesh::CreateLocalBound(const FStaticMesh* InStaticMesh)
{
    TArray<FNormalVertex> Verts = InStaticMesh->Vertices;
    FVector Min = Verts[0].pos;
    FVector Max = Verts[0].pos;
    for (FNormalVertex Vertex : Verts)
    {
        FVector Pos = Vertex.pos;
        Min = Min.ComponentMin(Pos);
        Max = Max.ComponentMax(Pos);
    }
    LocalBound = FAABB(Min, Max);
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

UBodySetup* UStaticMesh::GetBodySetup()
{
    // Lazy 초기화: BodySetup이 없으면 기본 Convex 생성
    if (!BodySetup)
    {
        CreateDefaultConvexIfNeeded();
    }
    return BodySetup;
}

void UStaticMesh::CreateBodySetupIfNeeded()
{
    if (!BodySetup)
    {
        BodySetup = NewObject<UBodySetup>();
    }
}

FString UStaticMesh::GetPhysicsCachePath() const
{
    // Convex CookedData 캐시 경로
    // 예: Data/cube.obj -> DerivedDataCache/cube.obj.physics.bin
    if (FilePath.empty())
    {
        return "";
    }
    return ConvertDataPathToCachePath(FilePath) + ".physics.bin";
}

FString UStaticMesh::GetPhysicsMetadataPath() const
{
    // Physics 메타데이터 경로 (Data/Metadata 폴더에 미러링)
    // 예: Data/Model/Cube.obj -> Data/Metadata/Model/Cube.obj.metadata
    if (FilePath.empty())
    {
        return "";
    }
    return ConvertDataPathToMetadataPath(FilePath);
}

bool UStaticMesh::LoadPhysicsCache()
{
    FString PhysicsPath = GetPhysicsCachePath();
    if (PhysicsPath.empty())
    {
        return false;
    }

    // 파일 존재 여부 확인
    if (!std::filesystem::exists(PhysicsPath))
    {
        return false;
    }

    // BodySetup이 없거나 Convex가 없으면 로드할 필요 없음
    if (!BodySetup || BodySetup->AggGeom.ConvexElems.IsEmpty())
    {
        return false;
    }

    // Binary 로드
    FWindowsBinReader Reader(PhysicsPath);
    if (!Reader.IsOpen())
    {
        UE_LOG("[UStaticMesh] 물리 데이터 파일 열기 실패: %s", PhysicsPath.c_str());
        return false;
    }

    try
    {
        // Intermediate structure로 로드
        FPhysicsCacheData CacheData;
        Reader << CacheData;
        Reader.Close();

        // 버전 체크
        if (CacheData.Version != 1)
        {
            UE_LOG("[UStaticMesh] 지원하지 않는 물리 데이터 버전: %u", CacheData.Version);
            return false;
        }

        // 개수가 다르면 캐시 무효화 (메타데이터와 불일치)
        if (CacheData.ConvexElems.size() != BodySetup->AggGeom.ConvexElems.Num())
        {
            UE_LOG("[UStaticMesh] Convex 캐시 개수 불일치: 캐시=%zu, 메타데이터=%d",
                   CacheData.ConvexElems.size(), BodySetup->AggGeom.ConvexElems.Num());
            return false;
        }

        // 기존 ConvexElems에 CookedData 채워넣기
        for (size_t i = 0; i < CacheData.ConvexElems.size(); ++i)
        {
            FKConvexElem& Convex = BodySetup->AggGeom.ConvexElems[i];
            FConvexCacheData& Cache = CacheData.ConvexElems[i];

            Convex.CookedData = std::move(Cache.CookedData);

            // CookedData로부터 ConvexMesh 생성 (re-cooking 없이)
            if (Convex.CookedData.Num() > 0)
            {
                if (!Convex.CreateConvexMeshFromCookedData())
                {
                    UE_LOG("[UStaticMesh] CookedData로부터 ConvexMesh 생성 실패 (index=%zu)", i);
                    return false;
                }
            }
        }

        UE_LOG("[UStaticMesh] Convex CookedData 캐시 로드 성공: %s (Convex %zu개)",
               PhysicsPath.c_str(), CacheData.ConvexElems.size());
        return true;
    }
    catch (const std::exception& e)
    {
        UE_LOG("[UStaticMesh] 물리 데이터 로드 예외: %s", e.what());
        return false;
    }
}

bool UStaticMesh::SavePhysicsCache()
{
    FString PhysicsPath = GetPhysicsCachePath();
    if (PhysicsPath.empty())
    {
        UE_LOG("[UStaticMesh] 물리 데이터 저장 실패: 파일 경로 없음");
        return false;
    }

    // BodySetup이 없으면 저장할 것이 없음
    if (!BodySetup)
    {
        return false;
    }

    // Convex 요소가 없으면 저장하지 않음
    if (BodySetup->AggGeom.ConvexElems.IsEmpty())
    {
        // 기존 파일이 있으면 삭제
        if (std::filesystem::exists(PhysicsPath))
        {
            std::filesystem::remove(PhysicsPath);
        }
        return true;
    }

    // 캐시 디렉토리 생성
    std::filesystem::path CacheFilePath(UTF8ToWide(PhysicsPath));
    if (CacheFilePath.has_parent_path())
    {
        std::filesystem::create_directories(CacheFilePath.parent_path());
    }

    // Intermediate structure로 변환
    FPhysicsCacheData CacheData;
    CacheData.Version = 1;
    CacheData.ConvexElems.reserve(BodySetup->AggGeom.ConvexElems.Num());

    for (const FKConvexElem& Convex : BodySetup->AggGeom.ConvexElems)
    {
        FConvexCacheData Cache;
        Cache.CookedData = Convex.CookedData;  // Copy (원본 유지 필요)
        Cache.Transform = Convex.ElemTransform;
        CacheData.ConvexElems.push_back(std::move(Cache));
    }

    // Binary 저장
    try
    {
        FWindowsBinWriter Writer(PhysicsPath);
        Writer << CacheData;
        Writer.Close();

        UE_LOG("[UStaticMesh] 물리 데이터 저장 성공: %s (Convex %zu개)",
               PhysicsPath.c_str(), CacheData.ConvexElems.size());
        return true;
    }
    catch (const std::exception& e)
    {
        UE_LOG("[UStaticMesh] 물리 데이터 저장 예외: %s", e.what());
        return false;
    }
}

bool UStaticMesh::LoadPhysicsMetadata()
{
    FString MetadataPath = GetPhysicsMetadataPath();
    if (MetadataPath.empty())
    {
        return false;
    }

    if (!std::filesystem::exists(MetadataPath))
    {
        return false;
    }

    try
    {
        JSON Root;
        FWideString WidePath = UTF8ToWide(MetadataPath);
        if (!FJsonSerializer::LoadJsonFromFile(Root, WidePath))
        {
            UE_LOG("[UStaticMesh] Physics 메타데이터 파일 열기 실패: %s", MetadataPath.c_str());
            return false;
        }

        CreateBodySetupIfNeeded();
        BodySetup->Serialize(true, Root);  // 로드

        UE_LOG("[UStaticMesh] Physics 메타데이터 로드 성공: %s", MetadataPath.c_str());

        // Convex가 있으면 처리
        if (!BodySetup->AggGeom.ConvexElems.IsEmpty())
        {
            // Source == Mesh인 Convex에 메시 정점 설정
            for (FKConvexElem& Convex : BodySetup->AggGeom.ConvexElems)
            {
                if (Convex.Source == EConvexSource::Mesh && StaticMeshAsset)
                {
                    // 메시 정점 추출
                    Convex.VertexData.Empty();
                    Convex.VertexData.Reserve(StaticMeshAsset->Vertices.size());
                    for (const FNormalVertex& Vtx : StaticMeshAsset->Vertices)
                    {
                        Convex.VertexData.Add(Vtx.pos);
                    }
                    Convex.UpdateBounds();
                }
            }

            // 캐시에서 CookedData 로드 시도
            if (LoadPhysicsCache())
            {
                UE_LOG("[UStaticMesh] Convex CookedData 캐시 로드 성공");
            }
            else
            {
                // 캐시가 없으면 VertexData에서 재쿠킹
                UE_LOG("[UStaticMesh] Convex CookedData 캐시 없음, 재쿠킹 시작...");
                for (FKConvexElem& Convex : BodySetup->AggGeom.ConvexElems)
                {
                    if (Convex.VertexData.Num() >= 4 && !Convex.ConvexMesh)
                    {
                        Convex.CreateConvexMesh();
                    }
                }
                // 쿠킹 후 캐시 저장
                SavePhysicsCache();
            }
        }

        return true;
    }
    catch (const std::exception& e)
    {
        UE_LOG("[UStaticMesh] Physics 메타데이터 로드 예외: %s", e.what());
        return false;
    }
}

bool UStaticMesh::SavePhysicsMetadata()
{
    FString MetadataPath = GetPhysicsMetadataPath();
    if (MetadataPath.empty())
    {
        UE_LOG("[UStaticMesh] Physics 메타데이터 저장 실패: 파일 경로 없음");
        return false;
    }

    if (!BodySetup)
    {
        UE_LOG("[UStaticMesh] Physics 메타데이터 저장 실패: BodySetup 없음");
        return false;
    }

    // Shape가 하나도 없으면 기존 파일들 삭제
    if (BodySetup->AggGeom.GetElementCount() == 0)
    {
        if (std::filesystem::exists(MetadataPath))
        {
            std::filesystem::remove(MetadataPath);
        }
        // Convex 캐시도 삭제
        FString CachePath = GetPhysicsCachePath();
        if (!CachePath.empty() && std::filesystem::exists(CachePath))
        {
            std::filesystem::remove(CachePath);
        }
        return true;
    }

    // 메타데이터 디렉토리 생성
    std::filesystem::path MetaFilePath(UTF8ToWide(MetadataPath));
    if (MetaFilePath.has_parent_path())
    {
        std::filesystem::create_directories(MetaFilePath.parent_path());
    }

    try
    {
        JSON Root = JSON::Make(JSON::Class::Object);
        BodySetup->Serialize(false, Root);  // 저장

        FWideString WidePath = UTF8ToWide(MetadataPath);
        if (!FJsonSerializer::SaveJsonToFile(Root, WidePath))
        {
            UE_LOG("[UStaticMesh] Physics 메타데이터 파일 쓰기 실패: %s", MetadataPath.c_str());
            return false;
        }

        UE_LOG("[UStaticMesh] Physics 메타데이터 저장 성공: %s", MetadataPath.c_str());

        // Convex가 있으면 CookedData 캐시도 저장
        if (!BodySetup->AggGeom.ConvexElems.IsEmpty())
        {
            SavePhysicsCache();
        }

        return true;
    }
    catch (const std::exception& e)
    {
        UE_LOG("[UStaticMesh] Physics 메타데이터 저장 예외: %s", e.what());
        return false;
    }
}

void UStaticMesh::CreateDefaultConvexIfNeeded()
{
    // 메시 데이터가 없으면 스킵
    if (!StaticMeshAsset || StaticMeshAsset->Vertices.empty())
    {
        return;
    }

    // BodySetup 생성
    CreateBodySetupIfNeeded();

    // 이미 충돌체가 있으면 스킵
    if (BodySetup->AggGeom.GetElementCount() > 0)
    {
        return;
    }

    // 캐시에서 CookedData 로드 시도 (메시에서 자동 생성된 Convex)
    FString CachePath = GetPhysicsCachePath();
    if (!CachePath.empty() && std::filesystem::exists(CachePath))
    {
        FWindowsBinReader Reader(CachePath);
        if (Reader.IsOpen())
        {
            try
            {
                FPhysicsCacheData CacheData;
                Reader << CacheData;
                Reader.Close();

                if (CacheData.Version == 1 && !CacheData.ConvexElems.empty())
                {
                    for (auto& Cache : CacheData.ConvexElems)
                    {
                        FKConvexElem Convex;
                        Convex.Source = EConvexSource::Mesh;  // 기본 Convex는 항상 Mesh 소스
                        Convex.CookedData = std::move(Cache.CookedData);
                        Convex.ElemTransform = Cache.Transform;

                        if (Convex.CookedData.Num() > 0 && Convex.CreateConvexMeshFromCookedData())
                        {
                            BodySetup->AggGeom.ConvexElems.Add(std::move(Convex));
                        }
                    }

                    if (BodySetup->AggGeom.ConvexElems.Num() > 0)
                    {
                        UE_LOG("[UStaticMesh] 캐시에서 기본 Convex 로드 성공: %s", FilePath.c_str());
                        // 메타데이터가 없으면 생성
                        FString MetadataPath = GetPhysicsMetadataPath();
                        if (!MetadataPath.empty() && !std::filesystem::exists(MetadataPath))
                        {
                            SavePhysicsMetadata();
                        }
                        return;
                    }
                }
            }
            catch (...) {}
        }
    }

    // 캐시 없으면 메시 정점에서 생성
    TArray<FVector> MeshVertices;
    MeshVertices.Reserve(StaticMeshAsset->Vertices.size());
    for (const FNormalVertex& Vtx : StaticMeshAsset->Vertices)
    {
        MeshVertices.Add(Vtx.pos);
    }

    // 최소 4개 정점이 필요 (Convex Hull 생성 조건)
    if (MeshVertices.Num() < 4)
    {
        UE_LOG("[UStaticMesh] Convex 생성 스킵: 정점이 4개 미만 (%d개)", MeshVertices.Num());
        return;
    }

    // Convex 생성
    FKConvexElem NewConvex;
    NewConvex.SetVertices(MeshVertices);

    // PhysX Convex Mesh 쿠킹
    if (NewConvex.CreateConvexMesh())
    {
        BodySetup->AggGeom.ConvexElems.Add(NewConvex);
        UE_LOG("[UStaticMesh] 기본 Convex 생성 성공: %s (정점 %d개)",
               FilePath.c_str(), MeshVertices.Num());

        // 메타데이터 + CookedData 캐시 저장
        SavePhysicsMetadata();
    }
    else
    {
        UE_LOG("[UStaticMesh] 기본 Convex 생성 실패: %s", FilePath.c_str());
    }
}

void UStaticMesh::ResetPhysicsToDefault()
{
    // 메시 데이터가 없으면 스킵
    if (!StaticMeshAsset || StaticMeshAsset->Vertices.empty())
    {
        UE_LOG("[UStaticMesh] ResetPhysicsToDefault 실패: 메시 데이터 없음");
        return;
    }

    // BodySetup 생성
    CreateBodySetupIfNeeded();

    // 기존 Shape 모두 제거
    BodySetup->AggGeom.SphereElems.Empty();
    BodySetup->AggGeom.BoxElems.Empty();
    BodySetup->AggGeom.SphylElems.Empty();
    BodySetup->AggGeom.ConvexElems.Empty();

    // 캐시에서 기본 Convex 로드 시도
    FString CachePath = GetPhysicsCachePath();
    if (!CachePath.empty() && std::filesystem::exists(CachePath))
    {
        FWindowsBinReader Reader(CachePath);
        if (Reader.IsOpen())
        {
            try
            {
                FPhysicsCacheData CacheData;
                Reader << CacheData;
                Reader.Close();

                if (CacheData.Version == 1 && !CacheData.ConvexElems.empty())
                {
                    for (auto& Cache : CacheData.ConvexElems)
                    {
                        FKConvexElem Convex;
                        Convex.Source = EConvexSource::Mesh;
                        Convex.CookedData = std::move(Cache.CookedData);
                        Convex.ElemTransform = Cache.Transform;

                        if (Convex.CookedData.Num() > 0 && Convex.CreateConvexMeshFromCookedData())
                        {
                            BodySetup->AggGeom.ConvexElems.Add(std::move(Convex));
                        }
                    }

                    if (BodySetup->AggGeom.ConvexElems.Num() > 0)
                    {
                        UE_LOG("[UStaticMesh] ResetPhysicsToDefault: 캐시에서 기본 Convex 복원 성공");
                        SavePhysicsMetadata();
                        return;
                    }
                }
            }
            catch (...)
            {
                UE_LOG("[UStaticMesh] ResetPhysicsToDefault: 캐시 로드 예외");
            }
        }
    }

    // 캐시 없으면 메시 정점에서 재생성
    TArray<FVector> MeshVertices;
    MeshVertices.Reserve(StaticMeshAsset->Vertices.size());
    for (const FNormalVertex& Vtx : StaticMeshAsset->Vertices)
    {
        MeshVertices.Add(Vtx.pos);
    }

    if (MeshVertices.Num() < 4)
    {
        UE_LOG("[UStaticMesh] ResetPhysicsToDefault: 정점이 4개 미만 (%d개)", MeshVertices.Num());
        return;
    }

    FKConvexElem NewConvex;
    NewConvex.Source = EConvexSource::Mesh;
    NewConvex.SetVertices(MeshVertices);

    if (NewConvex.CreateConvexMesh())
    {
        BodySetup->AggGeom.ConvexElems.Add(NewConvex);
        UE_LOG("[UStaticMesh] ResetPhysicsToDefault: 메시에서 기본 Convex 재생성 성공");
        SavePhysicsMetadata();
    }
    else
    {
        UE_LOG("[UStaticMesh] ResetPhysicsToDefault: Convex 생성 실패");
    }
}
