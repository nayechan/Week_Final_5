#include "pch.h"
#include "PhysicalMaterialLoader.h"
#include "PhysicalMaterial.h"
#include "ResourceManager.h"
#include <filesystem>
#include <unordered_set>

namespace fs = std::filesystem;

void FPhysicalMaterialLoader::Preload(const FString& RelativePath)
{
    FString FullPathStr = GDataDir + "/" + RelativePath;
    const fs::path DataDir(UTF8ToWide(FullPathStr));

    if (!fs::exists(DataDir) || !fs::is_directory(DataDir))
    {
        UE_LOG("[Physics] PhysicalMaterial Directory not found: %s", FullPathStr.c_str());
        return;
    }

    size_t LoadedCount = 0;
    std::unordered_set<FString> ProcessedFiles;

    for (const auto& Entry : fs::recursive_directory_iterator(DataDir))
    {
        if (!Entry.is_regular_file())
            continue;

        const fs::path& FilePath = Entry.path();
        FString Extension = WideToUTF8(FilePath.extension().wstring());
        
        // 소문자 변환
        std::transform(Extension.begin(), Extension.end(), Extension.begin(), 
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

        // .json 파일만 대상 (물리 재질은 json으로 저장됨)
        if (Extension == ".phxmtl")
        {
            FString PathStr = NormalizePath(WideToUTF8(FilePath.wstring()));

            if (ProcessedFiles.find(PathStr) == ProcessedFiles.end())
            {
                ProcessedFiles.insert(PathStr);

                UPhysicalMaterial* PhysMat = UResourceManager::GetInstance().Load<UPhysicalMaterial>(PathStr);
                if (PhysMat)
                {
                    LoadedCount++;
                }
            }
        }
    }

    UE_LOG("[Physics] Preloaded %zu Physical Materials from %s", LoadedCount, FullPathStr.c_str());
}