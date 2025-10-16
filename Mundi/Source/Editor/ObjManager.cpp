#include "pch.h"
#include "ObjManager.h"

#include "ObjectIterator.h"
#include "StaticMesh.h"
#include "Enums.h"
#include "WindowsBinReader.h"
#include "WindowsBinWriter.h"
#include <filesystem>
#include <unordered_set>

namespace fs = std::filesystem;

TMap<FString, FStaticMesh*> FObjManager::ObjStaticMeshMap;

/**
 * .obj 파일을 파싱하여 'mtllib' 라인에 명시된 .mtl 파일의 상대 경로를 찾습니다.
 * @param InObjPath - 검사할 .obj 파일의 경로
 * @return .mtl 파일의 경로를 담은 FString. 찾지 못하면 비어있는 문자열을 반환합니다.
 */
static FString FindMtlFilePath(const FString& InObjPath)
{
	std::ifstream FileIn(InObjPath);
	if (!FileIn)
	{
		return "";
	}

	FString Line;
	while (std::getline(FileIn, Line))
	{
		if (Line.rfind("mtllib ", 0) == 0)
		{
			size_t pos = InObjPath.find_last_of("/\\");
			FString objDir = (pos == FString::npos) ? "" : InObjPath.substr(0, pos + 1);
			return objDir + Line.substr(7);
		}
	}

	return "";
}

/**
 * @brief .obj 파일을 빠르게 스캔하여 참조된 모든 .mtl 파일의 전체 경로 목록을 반환합니다.
 * 이 함수는 전체 3D 데이터를 파싱하지 않고 'mtllib' 지시어만 효율적으로 찾습니다.
 * @param ObjPath 원본 .obj 파일의 경로입니다.
 * @param OutMtlFilePaths[out] 발견된 .mtl 파일들의 전체 경로가 저장될 배열입니다.
 * @return 스캔에 성공하면 true, 파일 열기에 실패하면 false를 반환합니다.
 */
bool GetMtlDependencies(const FString& ObjPath, TArray<FString>& OutMtlFilePaths)
{
	std::ifstream InFile(ObjPath);
	if (!InFile.is_open())
	{
		UE_LOG("Failed to open .obj file for dependency scan: %s", ObjPath.c_str());
		return false;
	}

	fs::path BaseDir = fs::path(ObjPath).parent_path();
	std::string Line;

	while (std::getline(InFile, Line))
	{
		// 라인 앞뒤의 공백을 제거하여 안정성을 높입니다.
		Line.erase(0, Line.find_first_not_of(" \t\r\n"));
		Line.erase(Line.find_last_not_of(" \t\r\n") + 1);

		if (Line.rfind("mtllib ", 0) == 0) // "mtllib "으로 시작하는지 확인
		{
			// "mtllib " 다음의 모든 문자열을 경로로 추출합니다.
			std::string MtlFileName = Line.substr(7);
			if (!MtlFileName.empty())
			{
				fs::path FullPath = fs::weakly_canonical(BaseDir / MtlFileName);
				FString PathStr = FullPath.string();
				std::replace(PathStr.begin(), PathStr.end(), '\\', '/');
				OutMtlFilePaths.AddUnique(PathStr);
			}
		}
	}
	return true;
}

/**
 * @brief 캐시 파일이 원본(.obj) 및 모든 의존성(.mtl) 파일보다 최신인지 검사합니다.
 * @param ObjPath 원본 .obj 파일의 경로입니다.
 * @param BinPath 메쉬 데이터 캐시(.obj.bin) 파일의 경로입니다.
 * @param MatBinPath 머티리얼 데이터 캐시(.mtl.bin) 파일의 경로입니다.
 * @return 캐시를 다시 생성해야 하면 true, 캐시가 유효하면 false를 반환합니다.
 */
bool ShouldRegenerateCache(const FString& ObjPath, const FString& BinPath, const FString& MatBinPath)
{
	// 캐시 파일 중 하나라도 존재하지 않으면 무조건 재생성해야 합니다.
	if (!fs::exists(BinPath) || !fs::exists(MatBinPath))
	{
		return true;
	}

	try
	{
		auto BinTimestamp = fs::last_write_time(BinPath);

		// 1. 원본 .obj 파일의 수정 시간을 캐시 파일과 비교합니다.
		if (fs::last_write_time(ObjPath) > BinTimestamp)
		{
			return true;
		}

		// 2. .obj 파일을 빠르게 스캔하여 의존하는 .mtl 파일 목록을 가져옵니다.
		TArray<FString> MtlDependencies;
		if (!GetMtlDependencies(ObjPath, MtlDependencies))
		{
			return true; // .obj 파일을 읽을 수 없으면 안전을 위해 캐시를 재생성합니다.
		}

		// 3. 각 .mtl 파일의 수정 시간을 캐시 파일과 비교합니다.
		for (const FString& MtlPath : MtlDependencies)
		{
			// .mtl 파일이 존재하지 않거나 캐시보다 최신 버전이면 재생성합니다.
			if (!fs::exists(MtlPath) || fs::last_write_time(MtlPath) > BinTimestamp)
			{
				return true;
			}
		}
	}
	catch (const fs::filesystem_error& e)
	{
		// 파일 시스템 오류(예: 접근 권한 없음) 발생 시 안전하게 캐시를 재생성합니다.
		UE_LOG("Filesystem error during cache validation: %s. Forcing regeneration.", e.what());
		return true;
	}

	// 모든 검사를 통과하면 캐시는 유효합니다.
	return false;
}

void FObjManager::Preload()
{
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
			FString PathStr = Path.string();
			std::replace(PathStr.begin(), PathStr.end(), '\\', '/');

			// 이미 처리된 파일인지 확인
			if (ProcessedFiles.find(PathStr) == ProcessedFiles.end())
			{
				ProcessedFiles.insert(PathStr);
				LoadObjStaticMesh(PathStr);
				++LoadedCount;
			}
		}
		else if (Extension == ".dds" || Extension == ".jpg" || Extension == ".png")
		{
			UResourceManager::GetInstance().Load<UTexture>(Path.string()); // 데칼 텍스쳐를 ui에서 고를 수 있게 하기 위해 임시로 만듬.
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
	FString NormalizedPathStr = PathFileName;
	std::replace(NormalizedPathStr.begin(), NormalizedPathStr.end(), '\\', '/');

	// 1. 메모리 캐시 확인: 이미 로드된 에셋이 있으면 즉시 반환합니다.
	if (FStaticMesh** It = ObjStaticMeshMap.Find(NormalizedPathStr))
	{
		return *It;
	}

	std::filesystem::path Path(NormalizedPathStr);

	// 2. 파일 경로 설정
	// 확장자를 소문자로 변환하여 대소문자 구분 없이 ".obj"를 처리합니다.
	std::string Extension = Path.extension().string();
	std::transform(Extension.begin(), Extension.end(), Extension.begin(),
		[](unsigned char c) { return static_cast<char>(std::tolower(c)); });

	if (Extension != ".obj")
	{
		UE_LOG("this file is not obj!: %s", NormalizedPathStr.c_str());
		return nullptr; // NewFStaticMesh가 생성되지 않았으므로 delete 불필요
	}

	// 파일명 생성 규칙 변경 (예: cube.obj -> cube.obj.bin, cube.mtl -> cube.obj.mat.bin)
	const FString BinPathFileName = NormalizedPathStr + ".bin";
	const FString MatBinPathFileName = NormalizedPathStr + ".mat.bin";

	// 3. 캐시 유효성 검사 및 데이터 준비
	FStaticMesh* NewFStaticMesh = new FStaticMesh();
	TArray<FObjMaterialInfo> MaterialInfos;

	bool bRegenerate = ShouldRegenerateCache(NormalizedPathStr, BinPathFileName, MatBinPathFileName);

	if (bRegenerate)
	{
		UE_LOG("Cache for '%s' is stale or missing. Regenerating...", NormalizedPathStr.c_str());

		// .obj 및 .mtl 파일 파싱
		FObjInfo RawObjInfo;
		if (!FObjImporter::LoadObjModel(NormalizedPathStr, &RawObjInfo, MaterialInfos, true))
		{
			UE_LOG("Failed to load and parse .obj model: %s", NormalizedPathStr.c_str());
			delete NewFStaticMesh;
			return nullptr;
		}

		// FStaticMesh 데이터로 변환
		FObjImporter::ConvertToStaticMesh(RawObjInfo, MaterialInfos, NewFStaticMesh);

		// 새로운 캐시 파일(.bin) 저장
		FWindowsBinWriter Writer(BinPathFileName);
		Writer << *NewFStaticMesh;
		Writer.Close();

		FWindowsBinWriter MatWriter(MatBinPathFileName);
		Serialization::WriteArray<FObjMaterialInfo>(MatWriter, MaterialInfos);
		MatWriter.Close();
	}
	else
	{
		UE_LOG("Loading '%s' from cache.", NormalizedPathStr.c_str());

		// 캐시에서 FStaticMesh 데이터 로드
		FWindowsBinReader Reader(BinPathFileName);
		if (!Reader.IsOpen())
		{
			UE_LOG("Failed to open bin file for reading: %s", BinPathFileName.c_str());
			delete NewFStaticMesh;
			return nullptr;
		}
		Reader << *NewFStaticMesh;
		Reader.Close();

		// 캐시에서 Material 데이터 로드
		FWindowsBinReader MatReader(MatBinPathFileName);
		if (!MatReader.IsOpen())
		{
			UE_LOG("Failed to open material bin file for reading: %s", MatBinPathFileName.c_str());
			delete NewFStaticMesh;
			return nullptr;
		}
		Serialization::ReadArray<FObjMaterialInfo>(MatReader, MaterialInfos);
		MatReader.Close();
	}

	// 4. 머티리얼 및 텍스처 경로 처리 (공통 로직)

	// 텍스처 파일의 상대 경로를 .obj 파일 기준의 절대 경로로 변환
	// [수정된 부분] FString을 fs::path로 변환 후 parent_path() 호출
	fs::path BaseDir = fs::path(NormalizedPathStr).parent_path();
	for (auto& MI : MaterialInfos)
	{
		auto FixPath = [&](FString& TexturePath)
			{
				if (TexturePath.empty()) return;

				fs::path TexPath(TexturePath);
				if (!TexPath.is_absolute())
				{
					fs::path AbsolutePath = fs::weakly_canonical(BaseDir / TexPath);
					TexturePath = AbsolutePath.string();
					std::replace(TexturePath.begin(), TexturePath.end(), '\\', '/');
				}
			};
		FixPath(MI.DiffuseTextureFileName);
		FixPath(MI.TransparencyTextureFileName);
		FixPath(MI.AmbientTextureFileName);
		FixPath(MI.SpecularTextureFileName);
		FixPath(MI.SpecularExponentTextureFileName);
		FixPath(MI.EmissiveTextureFileName);
	}

	// 리소스 매니저에 머티리얼 등록
	for (const FObjMaterialInfo& InMaterialInfo : MaterialInfos)
	{
		if (!UResourceManager::GetInstance().Get<UMaterial>(InMaterialInfo.MaterialName))
		{
			UMaterial* Material = NewObject<UMaterial>();
			Material->SetMaterialInfo(InMaterialInfo);
			UResourceManager::GetInstance().Add<UMaterial>(InMaterialInfo.MaterialName, Material);
		}
	}

	// 5. 메모리 캐시에 등록하고 반환
	ObjStaticMeshMap.Add(NormalizedPathStr, NewFStaticMesh);
	return NewFStaticMesh;
}

// 여기서 BVH 정보 담아주기 작업을 해야 함 
UStaticMesh* FObjManager::LoadObjStaticMesh(const FString& PathFileName)
{
	// 0) 경로
	FString NormalizedPathStr = PathFileName;
	std::replace(NormalizedPathStr.begin(), NormalizedPathStr.end(), '\\', '/');

	// 1) 이미 로드된 UStaticMesh가 있는지 전체 검색 (정규화된 경로로 비교)
	for (TObjectIterator<UStaticMesh> It; It; ++It)
	{
		UStaticMesh* StaticMesh = *It;

		if (StaticMesh->GetFilePath() == NormalizedPathStr)
		{
			return StaticMesh;
		}
	}

	// 2) 없으면 새로 로드 (정규화된 경로 사용)
	UStaticMesh* StaticMesh = UResourceManager::GetInstance().Load<UStaticMesh>(NormalizedPathStr, EVertexLayoutType::PositionColorTexturNormal);

	UE_LOG("UStaticMesh(filename: \'%s\') is successfully crated!", NormalizedPathStr.c_str());
	return StaticMesh;
}
