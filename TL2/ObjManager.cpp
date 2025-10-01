#include "pch.h"
#include "ObjManager.h"

#include "ObjectIterator.h"
#include "StaticMesh.h"
#include "Enums.h"
#include "WindowsBinReader.h"
#include "WindowsBinWriter.h"
#include <filesystem>
#include <unordered_set>

TMap<FString, FStaticMesh*> FObjManager::ObjStaticMeshMap;

void FObjManager::Preload()
{
    namespace fs = std::filesystem;
    const fs::path DataDir("Data");

    if (!fs::exists(DataDir) || !fs::is_directory(DataDir))
    {
        UE_LOG("FObjManager::Preload: Data directory not found: %s", DataDir.string().c_str());
        return;
    }

    size_t LoadedCount = 0;
    std::unordered_set<FString> ProcessedFiles; // 중복 로딩 방지

    for (const auto& Entry : fs::recursive_directory_iterator(DataDir))
    {
        if (!Entry.is_regular_file())
            continue;

        const fs::path& Path = Entry.path();
        std::string Extension = Path.extension().string();
        std::transform(Extension.begin(), Extension.end(), Extension.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

        if (Extension == ".obj")
        {
            // 경로 정규화 - 절대경로로 변환하고 슬래시로 통일
            std::filesystem::path NormalizedPath = std::filesystem::absolute(Path);
            FString PathStr = NormalizedPath.string();
            std::replace(PathStr.begin(), PathStr.end(), '\\', '/');

            // 이미 처리된 파일인지 확인
            if (ProcessedFiles.find(PathStr) == ProcessedFiles.end())
            {
                ProcessedFiles.insert(PathStr);
                LoadObjStaticMesh(PathStr);
                ++LoadedCount;
            }
        }
    }

    // 4) 모든 StaticMeshs 가져오기
    RESOURCE.SetStaticMeshs();

    UE_LOG("FObjManager::Preload: Loaded %zu .obj files from %s", LoadedCount, DataDir.string().c_str());
}

void FObjManager::Clear()
{
    for (auto& Pair : ObjStaticMeshMap)
    {
        delete Pair.second;
    }

    ObjStaticMeshMap.Empty();
}

FStaticMesh* FObjManager::LoadObjStaticMeshAsset(const FString& PathFileName)
{
    // 1) 경로 정규화 - 절대경로로 변환하고 백슬래시를 슬래시로 통일
    std::filesystem::path NormalizedPath = std::filesystem::absolute(PathFileName);
    FString NormalizedPathStr = NormalizedPath.string();
    std::replace(NormalizedPathStr.begin(), NormalizedPathStr.end(), '\\', '/');

    // 2) 캐시 히트 시 즉시 반환 (정규화된 경로로 검색)
    if (FStaticMesh** It = ObjStaticMeshMap.Find(NormalizedPathStr))
    {
        return *It;
    }

    // 3) 캐시 미스: 새로 생성
    FStaticMesh* NewFStaticMesh = new FStaticMesh();

    //FWideString WPathFileName(PathFileName.begin(), PathFileName.end()); // 단순 ascii라고 가정

    // 4) 해당 파일명 bin이 존재하는 지 확인
    // 존재하면 bin을 가져와서 FStaticMesh에 할당
    // 존재하지 않으면, 아래 과정 진행 후, bin으로 저장
    std::filesystem::path Path(NormalizedPathStr);
    if ((Path.extension() != ".obj") && (Path.extension() != ".OBJ"))
    {
        UE_LOG("this file is not obj!: %s", NormalizedPathStr.c_str());
        return nullptr;
    }

    TArray<FObjMaterialInfo> MaterialInfos;

    std::filesystem::path WithoutExtensionPath = Path;
    WithoutExtensionPath.replace_extension("");
    const FString StemPath = WithoutExtensionPath.string(); // 확장자를 제외한 경로
    const FString BinPathFileName = StemPath + ".bin";
    if (std::filesystem::exists(BinPathFileName))
    {
        // obj 정보 bin으로 가져오기
        FWindowsBinReader Reader(BinPathFileName);
        Reader << *NewFStaticMesh;
        Reader.Close();

        // MaterialInfo도 bin으로 가져오기
        FString MatBinPathFileName = StemPath + "Mat.bin";
        if (!std::filesystem::exists(MatBinPathFileName))
        {
            UE_LOG("\'%s\' does not exists!", MatBinPathFileName);
            assert(std::filesystem::exists(StemPath + "Mat.bin") && "material bin file dont exists!");

            // 존재하지 않으므로 obj(mtl 파싱 위해선 obj도 파싱 필요) 및 Mtl 파싱
            FObjInfo RawObjInfo;
            FObjImporter::LoadObjModel(NormalizedPathStr, &RawObjInfo, MaterialInfos, true, true);

            // MaterialInfos를 관련 파일명으로 bin 저장
            FWindowsBinWriter MatWriter(StemPath + "Mat.bin");
            Serialization::WriteArray<FObjMaterialInfo>(MatWriter, MaterialInfos);
            MatWriter.Close();
        }
        else
        {
            UE_LOG("bin file \'%s\', \'%s\' load completed", BinPathFileName, MatBinPathFileName);
            FWindowsBinReader MatReader(StemPath + "Mat.bin");
            Serialization::ReadArray<FObjMaterialInfo>(MatReader, MaterialInfos);
            MatReader.Close();

            // 상대경로를 OBJ 디렉터리 기준 절대경로로 보정
            std::filesystem::path baseDir = std::filesystem::path(StemPath).parent_path();
            for (auto& mi : MaterialInfos)
            {
                auto fix = [&](std::string& s){
                    if (s.empty()) return;
                    std::filesystem::path p = std::filesystem::path(s);
                    if (!p.is_absolute())
                    {
                        std::filesystem::path abs = baseDir / p;
                        std::string norm = abs.string();
                        std::replace(norm.begin(), norm.end(), '\\', '/');
                        s = norm;
                    }
                };
                fix(mi.DiffuseTextureFileName);
                fix(mi.TransparencyTextureFileName);
                fix(mi.AmbientTextureFileName);
                fix(mi.SpecularTextureFileName);
                fix(mi.SpecularExponentTextureFileName);
                fix(mi.EmissiveTextureFileName);
            }
        }  
    }
    else
    {
        // obj 및 Mtl 파싱
        FObjInfo RawObjInfo;
        //FObjImporter::LoadObjModel(WPathFileName, &RawObjInfo, false, true); // test로 오른손 좌표계 false
        FObjImporter::LoadObjModel(NormalizedPathStr, &RawObjInfo, MaterialInfos, true, true);
        FObjImporter::ConvertToStaticMesh(RawObjInfo, MaterialInfos, NewFStaticMesh);

        // obj 정보 bin에 저장
        FWindowsBinWriter Writer(BinPathFileName);
        Writer << *NewFStaticMesh;
        Writer.Close();

        // MaterialInfos도 관련 파일명으로 bin 저장
        FWindowsBinWriter MatWriter(StemPath + "Mat.bin");
        Serialization::WriteArray<FObjMaterialInfo>(MatWriter, MaterialInfos);
        MatWriter.Close();
    }

    // 리소스 매니저에 Material 리소스 맵핑 (중복 방지)
    for (const FObjMaterialInfo& InMaterialInfo : MaterialInfos)
    {
        // 이미 존재하는 머티리얼인지 확인
        if (!UResourceManager::GetInstance().Get<UMaterial>(InMaterialInfo.MaterialName))
        {
            UMaterial* Material = NewObject<UMaterial>();
            Material->SetMaterialInfo(InMaterialInfo);

            UResourceManager::GetInstance().Add<UMaterial>(InMaterialInfo.MaterialName, Material);
        }
    }

    // 5) 맵에 추가 (정규화된 경로로 저장)
    ObjStaticMeshMap.Add(NormalizedPathStr, NewFStaticMesh);

    // 6) 반환 경로 보장
    return NewFStaticMesh;
}

// 여기서 BVH 정보 담아주기 작업을 해야 함 
UStaticMesh* FObjManager::LoadObjStaticMesh(const FString& PathFileName)
{
    // 0) 경로 정규화
    std::filesystem::path NormalizedPath = std::filesystem::absolute(PathFileName);
    FString NormalizedPathStr = NormalizedPath.string();
    std::replace(NormalizedPathStr.begin(), NormalizedPathStr.end(), '\\', '/');

	// 1) 이미 로드된 UStaticMesh가 있는지 전체 검색 (정규화된 경로로 비교)
    for (TObjectIterator<UStaticMesh> It; It; ++It)
    {
        UStaticMesh* StaticMesh = *It;
        std::filesystem::path ExistingPath = std::filesystem::absolute(StaticMesh->GetFilePath());
        FString ExistingNormalizedStr = ExistingPath.string();
        std::replace(ExistingNormalizedStr.begin(), ExistingNormalizedStr.end(), '\\', '/');

        if (ExistingNormalizedStr == NormalizedPathStr)
        {
            return StaticMesh;
        }
    }

	// 2) 없으면 새로 로드 (정규화된 경로 사용)
    UStaticMesh* StaticMesh = UResourceManager::GetInstance().Load<UStaticMesh>(NormalizedPathStr, EVertexLayoutType::PositionColorTexturNormal);

    UE_LOG("UStaticMesh(filename: \'%s\') is successfully crated!", NormalizedPathStr.c_str());
	return StaticMesh;
}
