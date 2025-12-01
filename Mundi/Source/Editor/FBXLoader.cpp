#include "pch.h"
#include "ObjectFactory.h"
#include "FbxLoader.h"
#include "fbxsdk/fileio/fbxiosettings.h"
#include "fbxsdk/scene/geometry/fbxcluster.h"
#include "ObjectIterator.h"
#include "WindowsBinReader.h"
#include "WindowsBinWriter.h"
#include "PathUtils.h"
#include "AnimSequence.h"
#include "AnimDataModel.h"
#include "ResourceManager.h"
#include <filesystem>
#include <functional>

IMPLEMENT_CLASS(UFbxLoader)

// 노드가 스켈레톤 속성을 포함하는지 확인
static bool NodeContainsSkeleton(FbxNode* InNode)
{
	if (!InNode)
	{
		return false;
	}

	int InNodeCount = InNode->GetNodeAttributeCount();
	for (int i = 0; i < InNodeCount; ++i)
	{
		if (FbxNodeAttribute* Attribute = InNode->GetNodeAttributeByIndex(i))
		{
			if (Attribute->GetAttributeType() == FbxNodeAttribute::eSkeleton)
			{
				return true;
			}
		}
	}
	return false;
}

// 노드의 직계 자식 중에만 스켈레톤 노드가 있는지 확인 (비재귀적)
static bool NodeContainsSkeletonInImmediateChildren(FbxNode* InNode)
{
	if (!InNode)
	{
		return false;
	}

	// 직계 자식만 확인 (재귀 없음)
	int InNodeCount = InNode->GetChildCount();
	for (int i = 0; i < InNodeCount; ++i)
	{
		FbxNode* ChildNode = InNode->GetChild(i);

		// 자식 자체가 스켈레톤이면 true 반환
		int ChildAttributeCount = ChildNode->GetNodeAttributeCount();
		for (int j = 0; j < ChildAttributeCount; ++j)
		{
			if (FbxNodeAttribute* Attribute = ChildNode->GetNodeAttributeByIndex(j))
			{
				if (Attribute->GetAttributeType() == FbxNodeAttribute::eSkeleton)
				{
					return true;
				}
			}
		}
	}

	return false;
}

// 직계 자식 중에 스켈레톤이 있는 첫 번째 비스켈레톤 노드를 재귀적으로 찾음
// Blender의 Armature Object Node나 다른 컨테이너 노드를 찾는 데 사용
static FbxNode* FindNonSkeletonParentRecursive(FbxNode* Node)
{
	if (!Node)
	{
		return nullptr;
	}

	// 이 노드가 스켈레톤이 아니면서 직계 자식 중에 스켈레톤이 있는지 확인
	if (!NodeContainsSkeleton(Node) && NodeContainsSkeletonInImmediateChildren(Node))
	{
		return Node;
	}

	// 자식 노드들을 재귀적으로 탐색
	int ChildCount = Node->GetChildCount();
	for (int i = 0; i < ChildCount; ++i)
	{
		FbxNode* FoundNode = FindNonSkeletonParentRecursive(Node->GetChild(i));
		if (FoundNode)
		{
			return FoundNode;
		}
	}

	return nullptr;
}

UFbxLoader::UFbxLoader()
{
	// 메모리 관리, FbxManager 소멸시 Fbx 관련 오브젝트 모두 소멸
	SdkManager = FbxManager::Create();

}

void UFbxLoader::PreLoad()
{
	UFbxLoader& FbxLoader = GetInstance();

	const fs::path DataDir(UTF8ToWide(GDataDir));

	if (!fs::exists(DataDir) || !fs::is_directory(DataDir))
	{
		UE_LOG("UFbxLoader::Preload: Data directory not found: %s", WideToUTF8(DataDir.wstring()).c_str());
		return;
	}

	size_t LoadedCount = 0;
	std::unordered_set<FString> ProcessedFiles; // 중복 로딩 방지

	for (const auto& Entry : fs::recursive_directory_iterator(DataDir))
	{
		if (!Entry.is_regular_file())
			continue;

		const fs::path& Path = Entry.path();
		FString Extension = WideToUTF8(Path.extension().wstring());
		std::transform(Extension.begin(), Extension.end(), Extension.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

		if (Extension == ".fbx")
		{
			FString PathStr = NormalizePath(WideToUTF8(Path.wstring()));

			// 이미 처리된 파일인지 확인
			if (ProcessedFiles.find(PathStr) == ProcessedFiles.end())
			{
				ProcessedFiles.insert(PathStr);

				// 1. FBX 메시 로드
				USkeletalMesh* SkeletalMesh = FbxLoader.LoadFbxMesh(PathStr);

				// 2. 애니메이션 로드 (메시가 성공적으로 로드되고 스켈레톤이 있는 경우)
				if (SkeletalMesh)
				{
					const FSkeleton* Skeleton = SkeletalMesh->GetSkeleton();
					if (Skeleton && !Skeleton->Bones.IsEmpty())
					{
						// 3. FBX 파일에서 모든 애니메이션 스택 이름 가져오기
						TArray<FString> AnimStackNames = FbxLoader.GetAnimationStackNames(PathStr);

						// 4. 각 애니메이션 스택 로드
						for (const FString& AnimStackName : AnimStackNames)
						{
							UAnimSequence* AnimSequence = FbxLoader.LoadFbxAnimation(PathStr, Skeleton, AnimStackName);
							if (AnimSequence)
							{
								UE_LOG("UFbxLoader::PreLoad: Loaded animation '%s' from '%s'",
									AnimStackName.c_str(), PathStr.c_str());
							}
						}

						if (!AnimStackNames.IsEmpty())
						{
							UE_LOG("UFbxLoader::PreLoad: Total %d animations loaded from '%s'",
								AnimStackNames.Num(), PathStr.c_str());
						}
					}
				}

				++LoadedCount;
			}
		}
		else if (Extension == ".dds" || Extension == ".jpg" || Extension == ".png")
		{
			UResourceManager::GetInstance().Load<UTexture>(WideToUTF8(Path.wstring())); // 데칼 텍스쳐를 ui에서 고를 수 있게 하기 위해 임시로 만듬.
		}
	}

	RESOURCE.SetSkeletalMeshs();
	RESOURCE.SetAnimations();

	UE_LOG("UFbxLoader::Preload: Loaded %zu .fbx files from %s", LoadedCount, WideToUTF8(DataDir.wstring()).c_str());
}


UFbxLoader::~UFbxLoader()
{
	SdkManager->Destroy();
}

UFbxLoader& UFbxLoader::GetInstance()
{
	static UFbxLoader* FbxLoader = nullptr;
	if (!FbxLoader)
	{
		FbxLoader = ObjectFactory::NewObject<UFbxLoader>();
	}
	return *FbxLoader;
}

USkeletalMesh* UFbxLoader::LoadFbxMesh(const FString& FilePath)
{
	// 0) 경로
	FString NormalizedPathStr = NormalizePath(FilePath);

	// 1) 이미 로드된 UStaticMesh가 있는지 전체 검색 (정규화된 경로로 비교)
	for (TObjectIterator<USkeletalMesh> It; It; ++It)
	{
		USkeletalMesh* SkeletalMesh = *It;

		if (SkeletalMesh->GetFilePath() == NormalizedPathStr)
		{
			return SkeletalMesh;
		}
	}

	// 2) 없으면 새로 로드 (정규화된 경로 사용)
	USkeletalMesh* SkeletalMesh = UResourceManager::GetInstance().Load<USkeletalMesh>(NormalizedPathStr);

	UE_LOG("USkeletalMesh(filename: \'%s\') is successfully crated!", NormalizedPathStr.c_str());
	return SkeletalMesh;
}

FSkeletalMeshData* UFbxLoader::LoadFbxMeshAsset(const FString& FilePath)
{
	MaterialInfos.clear();
	FString NormalizedPath = NormalizePath(FilePath);
	FSkeletalMeshData* MeshData = nullptr;
#ifdef USE_OBJ_CACHE
	// 1. 캐시 파일 경로 설정
	FString CachePathStr = ConvertDataPathToCachePath(NormalizedPath);
	const FString BinPathFileName = CachePathStr + ".bin";

	// 캐시를 저장할 디렉토리가 없으면 생성
	std::filesystem::path CacheFileDirPath(UTF8ToWide(BinPathFileName));
	if (CacheFileDirPath.has_parent_path())
	{
		std::filesystem::create_directories(CacheFileDirPath.parent_path());
	}

	bool bLoadedFromCache = false;
	

	// 2. 캐시 유효성 검사
	bool bShouldRegenerate = true;
	if (std::filesystem::exists(UTF8ToWide(BinPathFileName)))
	{
		try
		{
			auto binTime = std::filesystem::last_write_time(UTF8ToWide(BinPathFileName));
			auto fbxTime = std::filesystem::last_write_time(UTF8ToWide(NormalizedPath));

			// FBX 파일이 캐시보다 오래되었으면 캐시 사용
			if (fbxTime <= binTime)
			{
				bShouldRegenerate = false;
			}
		}
		catch (const std::filesystem::filesystem_error& e)
		{
			UE_LOG("Filesystem error during cache validation: %s. Forcing regeneration.", e.what());
			bShouldRegenerate = true;
		}
	}

	// 3. 캐시에서 로드 시도
	if (!bShouldRegenerate)
	{
		UE_LOG("Attempting to load FBX '%s' from cache.", NormalizedPath.c_str());
		try
		{
			MeshData = new FSkeletalMeshData();
			MeshData->PathFileName = NormalizedPath;

			// FBX 파일의 이름을 스켈레톤 이름으로 사용
			std::filesystem::path p(UTF8ToWide(NormalizedPath));
			MeshData->Skeleton.Name = WideToUTF8(p.stem().wstring());

			FWindowsBinReader Reader(BinPathFileName);
			if (!Reader.IsOpen())
			{
				throw std::runtime_error("Failed to open bin file for reading.");
			}
			Reader << *MeshData;
			Reader.Close();

			for (int Index = 0; Index < MeshData->GroupInfos.Num(); Index++)
			{
				if (MeshData->GroupInfos[Index].InitialMaterialName.empty())
					continue;
				const FString& MaterialName = MeshData->GroupInfos[Index].InitialMaterialName;
				const FString& MaterialFilePath = ConvertDataPathToCachePath(MaterialName + ".mat.bin");
				FWindowsBinReader MatReader(MaterialFilePath);
				if (!MatReader.IsOpen())
				{
					throw std::runtime_error("Failed to open material bin file for reading.");
				}
				// for bin Load
				FMaterialInfo MaterialInfo{};
				Serialization::ReadAsset<FMaterialInfo>(MatReader, &MaterialInfo);

				UMaterial* NewMaterial = NewObject<UMaterial>();

				UMaterial* Default = UResourceManager::GetInstance().GetDefaultMaterial();
				NewMaterial->SetMaterialInfo(MaterialInfo);
				NewMaterial->SetShader(Default->GetShader());
				NewMaterial->SetShaderMacros(Default->GetShaderMacros());
				UResourceManager::GetInstance().Add<UMaterial>(MaterialInfo.MaterialName, NewMaterial);
			}

			MeshData->CacheFilePath = BinPathFileName;
			bLoadedFromCache = true;

			UE_LOG("Successfully loaded FBX '%s' from cache.", NormalizedPath.c_str());
			return MeshData;
		}
		catch (const std::exception& e)
		{
			UE_LOG("Error loading FBX from cache: %s. Cache might be corrupt or incompatible.", e.what());
			UE_LOG("Deleting corrupt cache and forcing regeneration for '%s'.", NormalizedPath.c_str());

			std::filesystem::remove(UTF8ToWide(BinPathFileName));
			if (MeshData)
			{
				delete MeshData;
				MeshData = nullptr;
			}
			bLoadedFromCache = false;
		}
	}

	// 4. 캐시 로드 실패 시 FBX 파싱
	UE_LOG("Regenerating cache for FBX '%s'...", NormalizedPath.c_str());
#endif // USE_OBJ_CACHE

	// 임포트 옵션 세팅 ( 에니메이션 임포트 여부, 머티리얼 임포트 여부 등등 IO에 대한 세팅 )
	// IOSROOT = IOSetting 객체의 기본 이름( Fbx매니저가 이름으로 관리하고 디버깅 시 매니저 내부의 객체를 이름으로 식별 가능)
	// 하지만 매니저 자체의 기본 설정이 있기 때문에 아직 안씀
	//FbxIOSettings* IoSetting = FbxIOSettings::Create(SdkManager, IOSROOT);
	//SdkManager->SetIOSettings(IoSetting);

	// 로드할때마다 임포터 생성
	FbxImporter* Importer = FbxImporter::Create(SdkManager, "");

	// 원하는 IO 세팅과 Fbx파일로 Importer initialize
	if (!Importer->Initialize(NormalizedPath.c_str(), -1, SdkManager->GetIOSettings()))
	{
		UE_LOG("Call to FbxImporter::Initialize() Falied\n");
		UE_LOG("[FbxImporter::Initialize()] Error Reports: %s\n\n", Importer->GetStatus().GetErrorString());
		return nullptr;
	}

	// 임포터가 씬에 데이터를 채워줄 것임. 이름은 IoSetting과 마찬가지로 매니저가 이름으로 관리하고 Export될 때 표시할 씬 이름.
	FbxScene* Scene = FbxScene::Create(SdkManager, "My Scene");

	// 임포트해서 Fbx를 씬에 넣어줌
	Importer->Import(Scene);

	// 임포트 후 제거
	Importer->Destroy();

	FbxAxisSystem UnrealImportAxis(FbxAxisSystem::eZAxis, FbxAxisSystem::eParityEven, FbxAxisSystem::eLeftHanded);
	FbxAxisSystem SourceSetup = Scene->GetGlobalSettings().GetAxisSystem();

	FbxSystemUnit::m.ConvertScene(Scene);

	if (SourceSetup != UnrealImportAxis)
	{
		UE_LOG("Fbx 축 변환 완료!\n");
		UnrealImportAxis.DeepConvertScene(Scene);
	}

	// 매시의 폴리곤이 삼각형이 아닐 수가 있음, Converter가 모두 삼각화해줌.
	FbxGeometryConverter IGeometryConverter(SdkManager);
	if (IGeometryConverter.Triangulate(Scene, true))
	{
		UE_LOG("Fbx 씬 삼각화 완료\n");
	}
	else
	{
		UE_LOG("Fbx 씬 삼각화가 실패했습니다, 매시가 깨질 수 있습니다\n");
	}

	MeshData = new FSkeletalMeshData();
	MeshData->PathFileName = NormalizedPath;

	// FBX 파일의 이름을 스켈레톤 이름으로 사용
	std::filesystem::path p(UTF8ToWide(NormalizedPath));
	MeshData->Skeleton.Name = WideToUTF8(p.stem().wstring());

	// Fbx파일에 씬은 하나만 존재하고 씬에 매쉬, 라이트, 카메라 등등의 element들이 트리 구조로 저장되어 있음.
	// 씬 Export 시에 루트 노드 말고 Child 노드만 저장됨. 노드 하나가 여러 Element를 저장할 수 있고 Element는 FbxNodeAttribute 클래스로 정의되어 있음.
	// 루트 노드 얻어옴
	FbxNode* RootNode = Scene->GetRootNode();

	// 뼈의 인덱스를 부여, 기본적으로 Fbx는 정점이 아니라 뼈 중심으로 데이터가 저장되어 있음(뼈가 몇번 ControlPoint에 영향을 주는지)
	// 정점이 몇번 뼈의 영향을 받는지 표현하려면 직접 뼈 인덱스를 만들어야함.
	TMap<FbxNode*, int32> BoneToIndex;

	// 머티리얼도 인덱스가 따로 없어서(있긴 한데 순서가 뒤죽박죽이고 모든 노드를 순회하기 전까지 머티리얼 개수를 몰라서) 직접 만들어줘야함
	// FbxSurfaceMaterial : 진짜 머티리얼 , FbxGeometryElementMaterial : 인덱싱용, 폴리곤이 어떤 머티리얼 슬롯을 쓰는지 알려줌
	TMap<FbxSurfaceMaterial*, int32> MaterialToIndex;

	// 머티리얼마다 정점 Index를 할당해 줄 것임. 머티리얼 정렬을 해놔야 렌더링 비용이 적게 드니까. 
	// 최종적으로 이 맵의 Array들을 합쳐서 MeshData.Indices에 저장하고 GroupInfos를 채워줄 것임.
	TMap<int32, TArray<uint32>> MaterialGroupIndexList;

	// 머티리얼이 어떤 노드는 있는데 어떤 노드는 없을 수 있음, 이때 머티리얼이 없는 노드의 Index를 저장해놓고 나중에 머티리얼이 있는 노드의 index를
	// 그룹별 정렬하게 되면 인덱스가 꼬임. 그래서 머티리얼이 없는 경우 그냥 0번 그룹을 쓰도록 하고 머티리얼 인덱스 0번을 nullptr에 할당함
	MaterialGroupIndexList.Add(0, TArray<uint32>());
	MaterialToIndex.Add(nullptr, 0);
	MeshData->GroupInfos.Add(FGroupInfo());
	if (RootNode)
	{
		// 2번의 패스로 나눠서 처음엔 뼈의 인덱스를 결정하고 2번째 패스에서 뼈가 영향을 미치는 정점들을 구하고 정점마다 뼈 인덱스를 할당해 줄 것임(동시에 TPose 역행렬도 구함)
		for (int Index = 0; Index < RootNode->GetChildCount(); Index++)
		{
			LoadSkeletonFromNode(RootNode->GetChild(Index), *MeshData, -1, BoneToIndex);
		}
		for (int Index = 0; Index < RootNode->GetChildCount(); Index++)
		{
			LoadMeshFromNode(RootNode->GetChild(Index), *MeshData, MaterialGroupIndexList, BoneToIndex, MaterialToIndex);
		}
		
		// 여러 루트 본이 있으면 가상 루트 생성
		EnsureSingleRootBone(*MeshData);
	}

	// 머티리얼이 있는 경우 플래그 설정
	if (MeshData->GroupInfos.Num() > 1)
	{
		MeshData->bHasMaterial = true;
	}
	// 있든없든 항상 기본 머티리얼 포함 머티리얼 그룹별로 인덱스 저장하므로 Append 해줘야함
	uint32 Count = 0;
	for (auto& Element : MaterialGroupIndexList)
	{
		int32 MatrialIndex = Element.first;

		const TArray<uint32>& IndexList = Element.second;

		// 최종 인덱스 배열에 리스트 추가
		MeshData->Indices.Append(IndexList);

		// 그룹info에 Startindex와 Count 추가
		MeshData->GroupInfos[MatrialIndex].StartIndex = Count;
		MeshData->GroupInfos[MatrialIndex].IndexCount = IndexList.Num();
		Count += IndexList.Num();
	}

#ifdef USE_OBJ_CACHE
	// 5. 캐시 저장
	try
	{
		FWindowsBinWriter Writer(BinPathFileName);
		Writer << *MeshData;
		Writer.Close();

		for (FMaterialInfo& MaterialInfo : MaterialInfos)
		{
			
			const FString MaterialFilePath = ConvertDataPathToCachePath(MaterialInfo.MaterialName + ".mat.bin");
			FWindowsBinWriter MatWriter(MaterialFilePath);
			Serialization::WriteAsset<FMaterialInfo>(MatWriter, &MaterialInfo);
			MatWriter.Close();
		}

		MeshData->CacheFilePath = BinPathFileName;

		UE_LOG("Cache regeneration complete for FBX '%s'.", NormalizedPath.c_str());
	}
	catch (const std::exception& e)
	{
		UE_LOG("Failed to save FBX cache: %s", e.what());
	}
#endif // USE_OBJ_CACHE

	// FbxScene 리소스 해제
	if (Scene)
	{
		Scene->Destroy();
		Scene = nullptr;
	}

	return MeshData;
}


void UFbxLoader::LoadMeshFromNode(FbxNode* InNode,
	FSkeletalMeshData& MeshData,
	TMap<int32, TArray<uint32>>& MaterialGroupIndexList,
	TMap<FbxNode*, int32>& BoneToIndex, 
	TMap<FbxSurfaceMaterial*, int32>& MaterialToIndex)
{
	// 최적화, 메시 로드 전에 미리 머티리얼로부터 인덱스를 해시맵을 이용해서 얻고 그걸 TArray에 저장하면 됨. 
	// 노드의 머티리얼 리스트는 슬롯으로 참조함(내가 정한 MaterialIndex는 슬롯과 다름), 슬롯에 대응하는 머티리얼 인덱스를 캐싱하는 것
	// 그럼 폴리곤 순회하면서 해싱할 필요가 없음
	TArray<int32> MaterialSlotToIndex;
	// Attribute 참조 함수
	for (int Index = 0; Index < InNode->GetNodeAttributeCount(); Index++)
	{
		FbxNodeAttribute* Attribute = InNode->GetNodeAttributeByIndex(Index);
		if (!Attribute)
		{
			continue;
		}
		
		if (Attribute->GetAttributeType() == FbxNodeAttribute::eMesh)
		{
			FbxMesh* Mesh = (FbxMesh*)Attribute;
			
			// 0번이 기본 머티리얼이고 1번 이상은 블렌딩 머티리얼이라고 함. 근데 엄청 고급 기능이라서 일반적인 로더는 0번만 쓴다고 함.
			if (Mesh->GetElementMaterialCount() > 0)
			{
				// 머티리얼 슬롯 인덱싱 해주는 클래스 (ex. materialElement->GetIndexArray() : 폴리곤마다 어떤 머티리얼 슬롯을 쓰는지 Array로 표현)
				FbxGeometryElementMaterial* MaterialElement = Mesh->GetElementMaterial(0);
				// 머티리얼이 폴리곤 단위로 매핑함 -> 모든 폴리곤이 같은 머티리얼을 쓰지 않음(같은 머티리얼을 쓰는 경우 = eAllSame)
				// MaterialCount랑은 전혀 다른 동작임(슬롯이 2개 이상 있어도 매핑 모드가 eAllSame이라서 머티리얼을 하나만 쓰는 경우가 있음)
				if (MaterialElement->GetMappingMode() == FbxGeometryElement::eByPolygon)
				{
					for (int MaterialSlot = 0; MaterialSlot < InNode->GetMaterialCount(); MaterialSlot++)
					{
						int MaterialIndex = 0;
						FbxSurfaceMaterial* Material = InNode->GetMaterial(MaterialSlot);
						if (MaterialToIndex.Contains(Material))
						{
							MaterialIndex = MaterialToIndex[Material];
						}
						else
						{
							FMaterialInfo MaterialInfo{};
							ParseMaterial(Material, MaterialInfo);
							// 새로운 머티리얼, 머티리얼 인덱스 설정
							MaterialIndex = MaterialToIndex.Num();
							MaterialToIndex.Add(Material, MaterialIndex);
							MeshData.GroupInfos.Add(FGroupInfo());
							MeshData.GroupInfos[MaterialIndex].InitialMaterialName = MaterialInfo.MaterialName;
						}
						// MaterialSlot에 대응하는 전역 MaterialIndex 저장
						MaterialSlotToIndex.Add(MaterialIndex);
					}
				}
				// 노드가 하나의 머티리얼만 쓰는 경우
				else if (MaterialElement->GetMappingMode() == FbxGeometryElement::eAllSame)
				{
					int MaterialIndex = 0;
					int MaterialSlot = MaterialElement->GetIndexArray().GetAt(0);
					FbxSurfaceMaterial* Material = InNode->GetMaterial(MaterialSlot);
					if (MaterialToIndex.Contains(Material))
					{
						MaterialIndex = MaterialToIndex[Material];
					}
					else
					{
						FMaterialInfo MaterialInfo{};
						ParseMaterial(Material, MaterialInfo);
						// 새로운 머티리얼, 머티리얼 인덱스 설정
						MaterialIndex = MaterialToIndex.Num();

						MaterialToIndex.Add(Material, MaterialIndex);
						MeshData.GroupInfos.Add(FGroupInfo());
						MeshData.GroupInfos[MaterialIndex].InitialMaterialName = MaterialInfo.MaterialName;
					}
					// MaterialSlotToIndex에 추가할 필요 없음(머티리얼 하나일때 해싱 패스하고 Material Index로 바로 그룹핑 할 거라서 안 씀)
					LoadMesh(Mesh, MeshData, MaterialGroupIndexList, BoneToIndex, MaterialSlotToIndex, MaterialIndex);
					continue;
				}
			}
			
			LoadMesh(Mesh, MeshData, MaterialGroupIndexList, BoneToIndex, MaterialSlotToIndex);
		}
	}

	for (int Index = 0; Index < InNode->GetChildCount(); Index++)
	{
		LoadMeshFromNode(InNode->GetChild(Index), MeshData, MaterialGroupIndexList, BoneToIndex, MaterialToIndex);
	}
}

// Skeleton은 계층구조까지 표현해야하므로 깊이 우선 탐색, ParentNodeIndex 명시.
void UFbxLoader::LoadSkeletonFromNode(FbxNode* InNode, FSkeletalMeshData& MeshData, int32 ParentNodeIndex, TMap<FbxNode*, int32>& BoneToIndex)
{
	int32 BoneIndex = ParentNodeIndex;
	for (int Index = 0; Index < InNode->GetNodeAttributeCount(); Index++)
	{
		
		FbxNodeAttribute* Attribute = InNode->GetNodeAttributeByIndex(Index);
		if (!Attribute)
		{
			continue;
		}

		if (Attribute->GetAttributeType() == FbxNodeAttribute::eSkeleton)
		{
			FBone BoneInfo{};

			BoneInfo.Name = FString(InNode->GetName());
			
			BoneInfo.ParentIndex = ParentNodeIndex;

			// 뼈 리스트에 추가
			MeshData.Skeleton.Bones.Add(BoneInfo);
			
			// 뼈 인덱스 우리가 정해줌(방금 추가한 마지막 원소)
			BoneIndex = MeshData.Skeleton.Bones.Num() - 1;
			
			// 뼈 이름으로 인덱스 서치 가능하게 함.
			MeshData.Skeleton.BoneNameToIndex.Add(BoneInfo.Name, BoneIndex);

			// 매시 로드할때 써야되서 맵에 인덱스 저장
			BoneToIndex.Add(InNode, BoneIndex);
			// 뼈가 노드 하나에 여러개 있는 경우는 없음. 말이 안되는 상황임.
			break;
		}
	}
	for (int Index = 0; Index < InNode->GetChildCount(); Index++)
	{
		// 깊이 우선 탐색 부모 인덱스 설정(InNOde가 eSkeleton이 아니면 기존 부모 인덱스가 넘어감(BoneIndex = ParentNodeIndex)
		LoadSkeletonFromNode(InNode->GetChild(Index), MeshData, BoneIndex, BoneToIndex);
	}
}

// 예시 코드
void UFbxLoader::LoadMeshFromAttribute(FbxNodeAttribute* InAttribute, FSkeletalMeshData& MeshData)
{
}

void UFbxLoader::LoadMesh(FbxMesh* InMesh, FSkeletalMeshData& MeshData, TMap<int32, TArray<uint32>>& MaterialGroupIndexList, TMap<FbxNode*, int32>& BoneToIndex, TArray<int32> MaterialSlotToIndex, int32 DefaultMaterialIndex)
{
	// 위에서 뼈 인덱스를 구했으므로 일단 ControlPoint에 대응되는 뼈 인덱스와 가중치부터 할당할 것임(이후 MeshData를 채우면서 ControlPoint를 순회할 것이므로)
	struct IndexWeight
	{
		uint32 BoneIndex;
		float BoneWeight;
	};
	// ControlPoint에 대응하는 뼈 인덱스, 가중치를 저장하는 맵
	// ControlPoint에 대응하는 뼈가 여러개일 수 있으므로 TArray로 저장
	TMap<int32, TArray<IndexWeight>> ControlPointToBoneWeight;
	// 메시 로컬 좌표계를 Fbx Scene World 좌표계로 바꿔주는 행렬
	FbxAMatrix FbxSceneWorld{};
	// 역전치(노말용)
	FbxAMatrix FbxSceneWorldInverseTranspose{};

	// Deformer: 매시의 모양을 변형시키는 모든 기능, ex) skin, blendShape(모프 타겟, 두 표정 미리 만들고 블랜딩해서 서서히 변화시킴)
	// 99.9퍼센트는 스킨이 하나만 있고 완전 복잡한 얼굴 표정을 표현하기 위해서 2개 이상을 쓰기도 하는데 0번만 쓰도록 해도 문제 없음(AAA급 게임에서 2개 이상을 처리함)
	// 2개 이상의 스킨이 들어가면 뼈 인덱스가 16개까지도 늘어남. 
	if (InMesh->GetDeformerCount(FbxDeformer::eSkin) > 0)
	{
		// 클러스터: 뼈라고 봐도 됨(뼈 정보와(Bind Pose 행렬) 그 뼈가 영향을 주는 정점, 가중치 저장)
		for (int Index = 0; Index < ((FbxSkin*)InMesh->GetDeformer(0, FbxDeformer::eSkin))->GetClusterCount(); Index++)
		{
			FbxCluster* Cluster = ((FbxSkin*)InMesh->GetDeformer(0, FbxDeformer::eSkin))->GetCluster(Index);

			if (Index == 0)
			{
				// 클러스터를 담고 있는 Node의(Mesh) Fbx Scene World Transform, 한 번만 구해도 되고 
				// 정점을 Fbx Scene World 좌표계로 저장하기 위해 사용(아티스트 의도를 그대로 반영 가능, 서브메시를 단일메시로 처리 가능)
				// 모든 SkeletalMesh는 Scene World 원점을 기준으로 제작되어야함
				Cluster->GetTransformMatrix(FbxSceneWorld);
				FbxSceneWorldInverseTranspose = FbxSceneWorld.Inverse().Transpose();
			}
			int IndexCount = Cluster->GetControlPointIndicesCount();
			// 클러스터가 영향을 주는 ControlPointIndex를 구함.
			int* Indices = Cluster->GetControlPointIndices();
			double* Weights = Cluster->GetControlPointWeights();
            // Bind Pose, Inverse Bind Pose 저장.
            // Fbx 스킨 규약:
            //  - TransformLinkMatrix: 본의 글로벌 바인드 행렬
            //  - TransformMatrix:     메시의 글로벌 바인드 행렬
            // 엔진 로컬(메시 기준) 바인드 행렬 = Inv(TransformMatrix) * TransformLinkMatrix
            FbxAMatrix BoneBindGlobal;
            Cluster->GetTransformLinkMatrix(BoneBindGlobal);
            FbxAMatrix BoneBindGlobalInv = BoneBindGlobal.Inverse();
            // FbxMatrix는 128바이트, FMatrix는 64바이트라서 memcpy쓰면 안 됨. 원소 단위로 복사합니다.
            for (int Row = 0; Row < 4; Row++)
            {
                for (int Col = 0; Col < 4; Col++)
                {
                    MeshData.Skeleton.Bones[BoneToIndex[Cluster->GetLink()]].BindPose.M[Row][Col] = static_cast<float>(BoneBindGlobal[Row][Col]);
                    MeshData.Skeleton.Bones[BoneToIndex[Cluster->GetLink()]].InverseBindPose.M[Row][Col] = static_cast<float>(BoneBindGlobalInv[Row][Col]);
                }
            }


			for (int ControlPointIndex = 0; ControlPointIndex < IndexCount; ControlPointIndex++)
			{
				// GetLink -> 아까 저장한 노드 To Index맵의 노드 (Cluster에 대응되는 뼈 인덱스를 ControlPointIndex에 대응시키는 과정)
				// ControlPointIndex = 클러스터가 저장하는 ControlPointIndex 배열에 대한 Index
				TArray<IndexWeight>& IndexWeightArray = ControlPointToBoneWeight[Indices[ControlPointIndex]];
				IndexWeightArray.Add(IndexWeight(BoneToIndex[Cluster->GetLink()], Weights[ControlPointIndex]));
			}
		}
	}

	bool bIsUniformScale = false;
	const FbxVector4& ScaleOfSceneWorld = FbxSceneWorld.GetS();
	// 비균등 스케일일 경우 그람슈미트 이용해서 탄젠트 재계산
	bIsUniformScale = ((FMath::Abs(ScaleOfSceneWorld[0] - ScaleOfSceneWorld[1]) < 0.001f) &&
		(FMath::Abs(ScaleOfSceneWorld[0] - ScaleOfSceneWorld[2]) < 0.001f));


	// 로드는 TriangleList를 가정하고 할 것임. 
	// TriangleStrip은 한번 만들면 편집이 사실상 불가능함, Fbx같은 호환성이 중요한 모델링 포멧이 유연성 부족한 모델을 저장할 이유도 없고
	// 엔진 최적화 측면에서도 GPU의 Vertex Cache가 Strip과 비슷한 성능을 내면서도 직관적이고 유연해서 잘 쓰지도 않기 때문에 그냥 안 씀.
	int PolygonCount = InMesh->GetPolygonCount();

	// ControlPoints는 정점의 위치 정보를 배열로 저장함, Vertex마다 ControlIndex로 참조함.
	FbxVector4* ControlPoints = InMesh->GetControlPoints();


	// Vertex 위치가 같아도 서로 다른 Normal, Tangent, UV좌표를 가질 수 있음, Fbx는 하나의 인덱스 배열에서 이들을 서로 다른 인덱스로 관리하길 강제하지 않고 
	// Vertex 위치는 ControlPoint로 관리하고 그 외의 정보들은 선택적으로 분리해서 관리하도록 함. 그래서 ControlPoint를 Index로 쓸 수도 없어서 따로 만들어야 하고, 
	// 위치정보 외의 정보를 참조할때는 매핑 방식별로 분기해서 저장해야함. 만약 매핑 방식이 eByPolygonVertex(꼭짓점 기준)인 경우 폴리곤의 꼭짓점을 순회하는 순서
	// 그대로 참조하면 됨, 그래서 VertexId를 꼭짓점 순회하는 순서대로 증가시키면서 매핑할 것임.
	int VertexId = 0;

	// 위의 이유로 ControlPoint를 인덱스 버퍼로 쓸 수가 없어서 Vertex마다 대응되는 Index Map을 따로 만들어서 계산할 것임.
	TMap<FSkinnedVertex, uint32> IndexMap;


	for (int PolygonIndex = 0; PolygonIndex < PolygonCount; PolygonIndex++)
	{
		// 최종적으로 사용할 머티리얼 인덱스를 구함, MaterialIndex 기본값이 0이므로 없는 경우 처리됨, 머티리얼이 하나일때 materialIndex가 1 이상이므로 처리됨.
		// 머티리얼이 여러개일때만 처리해주면 됌.
		
		// 머티리얼이 여러개인 경우(머티리얼이 하나 이상 있는데 materialIndex가 0이면 여러개, 하나일때는 MaterialIndex를 설정해주니까)
		// 이때는 해싱을 해줘야함
		int32 MaterialIndex = DefaultMaterialIndex;
		if (DefaultMaterialIndex == 0 && InMesh->GetElementMaterialCount() > 0)
		{
			FbxGeometryElementMaterial* Material = InMesh->GetElementMaterial(0);
			int MaterialSlot = Material->GetIndexArray().GetAt(PolygonIndex);
			MaterialIndex = MaterialSlotToIndex[MaterialSlot];
		}

		// 하나의 Polygon 내에서의 VertexIndex, PolygonSize가 다를 수 있지만 위에서 삼각화를 해줬기 때문에 무조건 3임
		for (int VertexIndex = 0; VertexIndex < InMesh->GetPolygonSize(PolygonIndex); VertexIndex++)
		{
			FSkinnedVertex SkinnedVertex{};
			// 폴리곤 인덱스와 폴리곤 내에서의 vertexIndex로 ControlPointIndex 얻어냄
			int ControlPointIndex = InMesh->GetPolygonVertex(PolygonIndex, VertexIndex);

			const FbxVector4& Position = FbxSceneWorld.MultT(ControlPoints[ControlPointIndex]);
			//const FbxVector4& Position = ControlPoints[ControlPointIndex];
			SkinnedVertex.Position = FVector(Position.mData[0], Position.mData[1], Position.mData[2]);


			if (ControlPointToBoneWeight.Contains(ControlPointIndex))
			{
				double TotalWeights = 0.0;


				const TArray<IndexWeight>& WeightArray = ControlPointToBoneWeight[ControlPointIndex];
				for (int BoneIndex = 0; BoneIndex < WeightArray.Num() && BoneIndex < 4; BoneIndex++)
				{
					// Total weight 구하기(정규화)
					TotalWeights += ControlPointToBoneWeight[ControlPointIndex][BoneIndex].BoneWeight;
				}
				// 5개 이상이 있어도 4개만 처리할 것임.
				for (int BoneIndex = 0; BoneIndex < WeightArray.Num() && BoneIndex < 4; BoneIndex++)
				{
					// ControlPoint에 대응하는 뼈 인덱스, 가중치를 모두 저장
					SkinnedVertex.BoneIndices[BoneIndex] = ControlPointToBoneWeight[ControlPointIndex][BoneIndex].BoneIndex;
					SkinnedVertex.BoneWeights[BoneIndex] = ControlPointToBoneWeight[ControlPointIndex][BoneIndex].BoneWeight/TotalWeights;
				}
			}


			// 함수명과 다르게 매시가 가진 버텍스 컬러 레이어 개수를 리턴함.( 0번 : Diffuse, 1번 : 블랜딩 마스크 , 기타..)
			// 엔진에서는 항상 0번만 사용하거나 Count가 0임. 그래서 하나라도 있으면 그냥 0번 쓰게 함.
			// 왜 이렇게 지어졌나? -> Fbx가 3D 모델링 관점에서 만들어졌기 때문, 모델링 툴에서는 여러 개의 컬러 레이어를 하나에 매시에 만들 수 있음.
			// 컬러 뿐만 아니라 UV Normal Tangent 모두 다 레이어로 저장하고 모두 다 0번만 쓰면 됨.
			if (InMesh->GetElementVertexColorCount() > 0)
			{
				// 왜 FbxLayerElement를 안 쓰지? -> 구버전 API
				FbxGeometryElementVertexColor* VertexColors = InMesh->GetElementVertexColor(0);
				int MappingIndex;
				// 확장성을 고려하여 switch를 씀, ControlPoint와 PolygonVertex말고 다른 모드들도 있음.
				switch (VertexColors->GetMappingMode())
				{
				case FbxGeometryElement::eByPolygon: //다른 모드 예시
				case FbxGeometryElement::eAllSame:
				case FbxGeometryElement::eNone:
				default:
					break;
					// 가장 단순한 경우, 그냥 ControlPoint(Vertex의 위치)마다 하나의 컬러값을 저장.
				case FbxGeometryElement::eByControlPoint:
					MappingIndex = ControlPointIndex;
					break;
					// 꼭짓점마다 컬러가 저장된 경우(같은 위치여도 다른 컬러 저장 가능), 위와 같지만 꼭짓점마다 할당되는 VertexId를 씀.
				case FbxGeometryElement::eByPolygonVertex:
					MappingIndex = VertexId;
					break;
				}

				// 매핑 방식에 더해서, 실제로 그 ControlPoint에서 어떻게 참조할 것인지가 다를 수 있음.(데이터 압축때문에 필요, IndexBuffer를 쓰는 것과 비슷함)
				switch (VertexColors->GetReferenceMode())
				{
					// 인덱스 자체가 데이터 배열의 인덱스인 경우(중복이 생길 수 있음)
				case FbxGeometryElement::eDirect:
				{
					// 바로 참조 가능.
					const FbxColor& Color = VertexColors->GetDirectArray().GetAt(MappingIndex);
					SkinnedVertex.Color = FVector4(Color.mRed, Color.mGreen, Color.mBlue, Color.mAlpha);
				}
				break;
				//인덱스 배열로 간접참조해야함
				case FbxGeometryElement::eIndexToDirect:
				{
					int Id = VertexColors->GetIndexArray().GetAt(MappingIndex);
					const FbxColor& Color = VertexColors->GetDirectArray().GetAt(Id);
					SkinnedVertex.Color = FVector4(Color.mRed, Color.mGreen, Color.mBlue, Color.mAlpha);
				}
				break;
				//외의 경우는 일단 배제
				default:
					break;
				}
			}

			if (InMesh->GetElementNormalCount() > 0)
			{
				FbxGeometryElementNormal* Normals = InMesh->GetElementNormal(0);

				// 각진 모서리 표현력 때문에 99퍼센트의 모델은 eByPolygonVertex를 쓴다고 함.
				// 근데 구 같이 각진 모서리가 아예 없는 경우, 부드러운 셰이딩 모델을 익스포트해서 eControlPoint로 저장될 수도 있음
				int MappingIndex;

				switch (Normals->GetMappingMode())
				{
				case FbxGeometryElement::eByControlPoint:
					MappingIndex = ControlPointIndex;
					break;
				case FbxGeometryElement::eByPolygonVertex:
					MappingIndex = VertexId;
					break;
				default:
					break;
				}

				switch (Normals->GetReferenceMode())
				{
				case FbxGeometryElement::eDirect:
				{
					const FbxVector4& Normal = Normals->GetDirectArray().GetAt(MappingIndex);
					FbxVector4 NormalWorld = FbxSceneWorldInverseTranspose.MultT(FbxVector4(Normal.mData[0], Normal.mData[1], Normal.mData[2], 0.0f));
					SkinnedVertex.Normal = FVector(NormalWorld.mData[0], NormalWorld.mData[1], NormalWorld.mData[2]);
				}
				break;
				case FbxGeometryElement::eIndexToDirect:
				{
					int Id = Normals->GetIndexArray().GetAt(MappingIndex);
					const FbxVector4& Normal = Normals->GetDirectArray().GetAt(Id);
					FbxVector4 NormalWorld = FbxSceneWorldInverseTranspose.MultT(FbxVector4(Normal.mData[0], Normal.mData[1], Normal.mData[2], 0.0f));
					SkinnedVertex.Normal = FVector(NormalWorld.mData[0], NormalWorld.mData[1], NormalWorld.mData[2]);
				}
				break;
				default:
					break;
				}
			}

			if (InMesh->GetElementTangentCount() > 0)
			{
				FbxGeometryElementTangent* Tangents = InMesh->GetElementTangent(0);

				// 왜 Color에서 계산한 Mapping Index를 안 쓰지? -> 컬러, 탄젠트, 노말, UV 모두 다 다른 매핑 방식을 사용 가능함.
				int MappingIndex;

				switch (Tangents->GetMappingMode())
				{
				case FbxGeometryElement::eByControlPoint:
					MappingIndex = ControlPointIndex;
					break;
				case FbxGeometryElement::eByPolygonVertex:
					MappingIndex = VertexId;
					break;
				default:
					break;
				}

				switch (Tangents->GetReferenceMode())
				{
				case FbxGeometryElement::eDirect:
				{
					const FbxVector4& Tangent = Tangents->GetDirectArray().GetAt(MappingIndex);
					FbxVector4 TangentWorld = FbxSceneWorld.MultT(FbxVector4(Tangent.mData[0], Tangent.mData[1], Tangent.mData[2], 0.0f));
					SkinnedVertex.Tangent = FVector4(TangentWorld.mData[0], TangentWorld.mData[1], TangentWorld.mData[2], Tangent.mData[3]);
				}
				break;
				case FbxGeometryElement::eIndexToDirect:
				{
					int Id = Tangents->GetIndexArray().GetAt(MappingIndex);
					const FbxVector4& Tangent = Tangents->GetDirectArray().GetAt(Id);
					FbxVector4 TangentWorld = FbxSceneWorld.MultT(FbxVector4(Tangent.mData[0], Tangent.mData[1], Tangent.mData[2], 0.0f));
					SkinnedVertex.Tangent = FVector4(TangentWorld.mData[0], TangentWorld.mData[1], TangentWorld.mData[2], Tangent.mData[3]);
				}
				break;
				default:
					break;
				}

				// 유니폼 스케일이 아니므로 그람슈미트, 노말이 필요하므로 노말 이후에 탄젠트 계산해야함
				if (!bIsUniformScale)
				{
					FVector Tangent = FVector(SkinnedVertex.Tangent.X, SkinnedVertex.Tangent.Y, SkinnedVertex.Tangent.Z);
					float Handedness = SkinnedVertex.Tangent.W;
					const FVector& Normal = SkinnedVertex.Normal;

					float TangentToNormalDir = FVector::Dot(Tangent, Normal);

					Tangent = Tangent - Normal * TangentToNormalDir;
					Tangent.Normalize();
					SkinnedVertex.Tangent = FVector4(Tangent.X, Tangent.Y, Tangent.Z, Handedness);
				}

			}

			// UV는 매핑 방식이 위와 다름(eByPolygonVertex에서 VertexId를 안 쓰고 TextureUvIndex를 씀, 참조방식도 위와 다름.)
			// 이유 : 3D 모델의 부드러운 면에 2D 텍스처 매핑을 위해 제봉선(가상의)을 만드는 경우가 생김, 그때 하나의 VertexId가 그 제봉선을 경계로
			//		  서로 다른 uv 좌표를 가져야 할 때가 생김. 그냥 VertexId를 더 나누면 안되나? => 아티스트가 싫어하고 직관적이지도 않음, 실제로 
			//		  물리적으로 폴리곤이 찢어진 게 아닌데 텍스처를 입히겠다고 Vertex를 새로 만들고 폴리곤을 찢어야 함.
			//		  그래서 UV는 인덱싱을 나머지와 다르게함
			if (InMesh->GetElementUVCount() > 0)
			{
				FbxGeometryElementUV* UVs = InMesh->GetElementUV(0);

				switch (UVs->GetMappingMode())
				{
				case FbxGeometryElement::eByControlPoint:
					switch (UVs->GetReferenceMode())
					{
					case FbxGeometryElement::eDirect:
					{
						const FbxVector2& UV = UVs->GetDirectArray().GetAt(ControlPointIndex);
						SkinnedVertex.UV = FVector2D(UV.mData[0], 1 - UV.mData[1]);
					}
					break;
					case FbxGeometryElement::eIndexToDirect:
					{
						int Id = UVs->GetIndexArray().GetAt(ControlPointIndex);
						const FbxVector2& UV = UVs->GetDirectArray().GetAt(Id);
						SkinnedVertex.UV = FVector2D(UV.mData[0], 1 - UV.mData[1]);
					}
					break;
					default:
						break;
					}
					break;
				case FbxGeometryElement::eByPolygonVertex:
				{
					int TextureUvIndex = InMesh->GetTextureUVIndex(PolygonIndex, VertexIndex);
					switch (UVs->GetReferenceMode())
					{
					case FbxGeometryElement::eDirect:
					case FbxGeometryElement::eIndexToDirect:
					{
						const FbxVector2& UV = UVs->GetDirectArray().GetAt(TextureUvIndex);
						SkinnedVertex.UV = FVector2D(UV.mData[0], 1 - UV.mData[1]);
					}
					break;
					default:
						break;
					}
				}
				break;
				default:
					break;
				}
			}

			// 실제 인덱스 버퍼에서 사용할 인덱스
			uint32 IndexOfVertex;
			// 기존의 Vertex맵에 있으면 그 인덱스를 사용
			if (IndexMap.Contains(SkinnedVertex))
			{
				IndexOfVertex = IndexMap[SkinnedVertex];
			}
			else
			{
				// 없으면 Vertex 리스트에 추가하고 마지막 원소 인덱스를 사용
				MeshData.Vertices.Add(SkinnedVertex);
				IndexOfVertex = MeshData.Vertices.Num() - 1;

				// 인덱스 맵에 추가
				IndexMap.Add(SkinnedVertex, IndexOfVertex);
			}
			// 대응하는 머티리얼 인덱스 리스트에 추가
			TArray<uint32>& GroupIndexList = MaterialGroupIndexList[MaterialIndex];
			GroupIndexList.Add(IndexOfVertex);

			// Vertex 하나 저장했고 Vertex마다 Id를 사용하므로 +1
			VertexId++;
		} // for PolygonSize
	} // for PolygonCount

	

	// FBX에 정점의 탄젠트 벡터가 존재하지 않을 시
	if (InMesh->GetElementTangentCount() == 0)
	{
        // 1. 계산된 탄젠트와 바이탄젠트(Bitangent)를 누적할 임시 저장소를 만듭니다.
        // MeshData.Vertices에 이미 중복 제거된 유일한 정점들이 들어있습니다.
        TArray<FVector> TempTangents(MeshData.Vertices.Num());
        TArray<FVector> TempBitangents(MeshData.Vertices.Num());

        // 2. 모든 머티리얼 그룹의 인덱스 리스트를 순회합니다.
        for (auto& Elem : MaterialGroupIndexList)
        {
            TArray<uint32>& GroupIndexList = Elem.second;

            // 인덱스 리스트를 3개씩(트라이앵글 단위로) 순회합니다.
            for (int32 i = 0; i < GroupIndexList.Num(); i += 3)
            {
                uint32 i0 = GroupIndexList[i];
                uint32 i1 = GroupIndexList[i + 1];
                uint32 i2 = GroupIndexList[i + 2];

                // 트라이앵글을 구성하는 3개의 정점 데이터를 가져옵니다.
                // 이 정점들은 MeshData.Vertices에 있는 *유일한* 정점입니다.
                const FSkinnedVertex& v0 = MeshData.Vertices[i0];
                const FSkinnedVertex& v1 = MeshData.Vertices[i1];
                const FSkinnedVertex& v2 = MeshData.Vertices[i2];

                // 위치(P)와 UV(W)를 가져옵니다.
                const FVector& P0 = v0.Position;
                const FVector& P1 = v1.Position;
                const FVector& P2 = v2.Position;

                const FVector2D& W0 = v0.UV;
                const FVector2D& W1 = v1.UV;
                const FVector2D& W2 = v2.UV;

                // 트라이앵글의 엣지(Edge)와 델타(Delta) UV를 계산합니다.
                FVector Edge1 = P1 - P0;
                FVector Edge2 = P2 - P0;
                FVector2D DeltaUV1 = W1 - W0;
                FVector2D DeltaUV2 = W2 - W0;

                // Lengyel's MikkTSpace/Schwarze Formula (분모)
                float r = 1.0f / (DeltaUV1.X * DeltaUV2.Y - DeltaUV1.Y * DeltaUV2.X);
                
                // r이 무한대(inf)나 NaN이 아닌지 확인 (UV가 겹치는 경우)
                if (isinf(r) || isnan(r))
                {
                    r = 0.0f; // 이 트라이앵글은 계산에서 제외
                }

                // (정규화되지 않은) 탄젠트(T)와 바이탄젠트(B) 계산
                FVector T = (Edge1 * DeltaUV2.Y - Edge2 * DeltaUV1.Y) * r;
                FVector B = (Edge2 * DeltaUV1.X - Edge1 * DeltaUV2.X) * r;

                // 3개의 정점에 T와 B를 (정규화 없이) 누적합니다.
                // 이렇게 하면 동일한 정점을 공유하는 모든 트라이앵글의 T/B가 합산됩니다.
                TempTangents[i0] += T;
                TempTangents[i1] += T;
                TempTangents[i2] += T;

                TempBitangents[i0] += B;
                TempBitangents[i1] += B;
                TempBitangents[i2] += B;
            }
        }

        // 3. 모든 정점을 순회하며 누적된 T/B를 직교화(Gram-Schmidt)하고 저장합니다.
        for (int32 i = 0; i < MeshData.Vertices.Num(); ++i)
        {
            FSkinnedVertex& V = MeshData.Vertices[i]; // 실제 정점 데이터에 접근
            const FVector& N = V.Normal;
            const FVector& T_accum = TempTangents[i];
            const FVector& B_accum = TempBitangents[i];

            if (T_accum.IsZero() || N.IsZero())
            {
                // T 또는 N이 0이면 계산 불가. 유효한 기본값 설정
                FVector T_fallback = FVector(1.0f, 0.0f, 0.0f);
                if (FMath::Abs(FVector::Dot(N, T_fallback)) > 0.99f) // N이 X축과 거의 평행하면
                {
                    T_fallback = FVector(0.0f, 1.0f, 0.0f); // Y축을 T로 사용
                }
                V.Tangent = FVector4(T_fallback.X, T_fallback.Y, T_fallback.Z, 1.0f);
                continue;
            }

            // Gram-Schmidt 직교화: T = T - (T dot N) * N
            // (T를 N에 투영한 성분을 T에서 빼서, N과 수직인 벡터를 만듭니다)
        	FVector Tangent = (T_accum - N * (FVector::Dot(T_accum, N))).GetSafeNormal();

            // Handedness (W 컴포넌트) 계산:
            // 외적으로 구한 B(N x T)와 누적된 B(B_accum)의 방향을 비교합니다.
            float Handedness = (FVector::Dot((FVector::Cross(Tangent, N)), B_accum) > 0.0f ) ? 1.0f : -1.0f;

            // 최종 탄젠트(T)와 Handedness(W)를 저장합니다.
            V.Tangent = FVector4(Tangent.X, Tangent.Y, Tangent.Z, Handedness);
        }
    }
}


// 머티리얼 파싱해서 FMaterialInfo에 매핑
void UFbxLoader::ParseMaterial(FbxSurfaceMaterial* Material, FMaterialInfo& MaterialInfo)
{

	UMaterial* NewMaterial = NewObject<UMaterial>();

	// FbxPropertyT : 타입에 대해 애니메이션과 연결 지원(키프레임마다 타입 변경 등)
	FbxPropertyT<FbxDouble3> Double3Prop;
	FbxPropertyT<FbxDouble> DoubleProp;

	MaterialInfo.MaterialName = Material->GetName();
	// PBR 제외하고 Phong, Lambert 머티리얼만 처리함. 
	if (Material->GetClassId().Is(FbxSurfacePhong::ClassId))
	{
		FbxSurfacePhong* SurfacePhong = (FbxSurfacePhong*)Material;

		Double3Prop = SurfacePhong->Diffuse;
		MaterialInfo.DiffuseColor = FVector(Double3Prop.Get()[0], Double3Prop.Get()[1], Double3Prop.Get()[2]);

		Double3Prop = SurfacePhong->Ambient;
		MaterialInfo.AmbientColor = FVector(Double3Prop.Get()[0], Double3Prop.Get()[1], Double3Prop.Get()[2]);

		// SurfacePhong->Reflection : 환경 반사, 퐁 모델에선 필요없음
		Double3Prop = SurfacePhong->Specular;
		DoubleProp = SurfacePhong->SpecularFactor;
		MaterialInfo.SpecularColor = FVector(Double3Prop.Get()[0], Double3Prop.Get()[1], Double3Prop.Get()[2]) * DoubleProp.Get();

		// HDR 안 써서 의미 없음
	/*	Double3Prop = SurfacePhong->Emissive;
		MaterialInfo.EmissiveColor = FVector(Double3Prop.Get()[0], Double3Prop.Get()[1], Double3Prop.Get()[2]);*/

		DoubleProp = SurfacePhong->Shininess;
		MaterialInfo.SpecularExponent = DoubleProp.Get();

		DoubleProp = SurfacePhong->TransparencyFactor;
		MaterialInfo.Transparency = DoubleProp.Get();
	}
	else if (Material->GetClassId().Is(FbxSurfaceLambert::ClassId))
	{
		FbxSurfaceLambert* SurfacePhong = (FbxSurfaceLambert*)Material;

		Double3Prop = SurfacePhong->Diffuse;
		MaterialInfo.DiffuseColor = FVector(Double3Prop.Get()[0], Double3Prop.Get()[1], Double3Prop.Get()[2]);

		Double3Prop = SurfacePhong->Ambient;
		MaterialInfo.AmbientColor = FVector(Double3Prop.Get()[0], Double3Prop.Get()[1], Double3Prop.Get()[2]);

		DoubleProp = SurfacePhong->TransparencyFactor;
		MaterialInfo.Transparency = DoubleProp.Get();
	}


	FbxProperty Property;

	Property = Material->FindProperty(FbxSurfaceMaterial::sDiffuse);
	MaterialInfo.DiffuseTextureFileName = ParseTexturePath(Property);

	Property = Material->FindProperty(FbxSurfaceMaterial::sNormalMap);
	MaterialInfo.NormalTextureFileName = ParseTexturePath(Property);

	Property = Material->FindProperty(FbxSurfaceMaterial::sAmbient);
	MaterialInfo.AmbientTextureFileName = ParseTexturePath(Property);

	Property = Material->FindProperty(FbxSurfaceMaterial::sSpecular);
	MaterialInfo.SpecularTextureFileName = ParseTexturePath(Property);

	Property = Material->FindProperty(FbxSurfaceMaterial::sEmissive);
	MaterialInfo.EmissiveTextureFileName = ParseTexturePath(Property);

	Property = Material->FindProperty(FbxSurfaceMaterial::sTransparencyFactor);
	MaterialInfo.TransparencyTextureFileName = ParseTexturePath(Property);

	Property = Material->FindProperty(FbxSurfaceMaterial::sShininess);
	MaterialInfo.SpecularExponentTextureFileName = ParseTexturePath(Property);
	
	UMaterial* Default = UResourceManager::GetInstance().GetDefaultMaterial();
	NewMaterial->SetMaterialInfo(MaterialInfo);
	NewMaterial->SetShader(Default->GetShader());
	NewMaterial->SetShaderMacros(Default->GetShaderMacros());
	//NewMaterial->ResolveTextures();  // 텍스처 경로를 실제 텍스처 포인터로 변환 : 일단 적용 안함

	MaterialInfos.Add(MaterialInfo);
	UResourceManager::GetInstance().Add<UMaterial>(MaterialInfo.MaterialName, NewMaterial);
}

FString UFbxLoader::ParseTexturePath(FbxProperty& Property)
{
	if (Property.IsValid())
	{
		if (Property.GetSrcObjectCount<FbxFileTexture>() > 0)
		{
			FbxFileTexture* Texture = Property.GetSrcObject<FbxFileTexture>(0);
			if (Texture)
			{
				return FString(Texture->GetFileName());
			}
		}
	}
	return FString();
}

FbxString UFbxLoader::GetAttributeTypeName(FbxNodeAttribute* InAttribute)
{
	return "test";
}

void UFbxLoader::EnsureSingleRootBone(FSkeletalMeshData& MeshData)
{
	if (MeshData.Skeleton.Bones.IsEmpty())
		return;
    
	// 루트 본 개수 세기
	TArray<int32> RootBoneIndices;
	for (int32 i = 0; i < MeshData.Skeleton.Bones.size(); ++i)
	{
		if (MeshData.Skeleton.Bones[i].ParentIndex == -1)
		{
			RootBoneIndices.Add(i);
		}
	}
    
	// 루트 본이 2개 이상이면 가상 루트 생성
	if (RootBoneIndices.Num() > 1)
	{
		// 가상 루트 본 생성
		FBone VirtualRoot;
		VirtualRoot.Name = "VirtualRoot";
		VirtualRoot.ParentIndex = -1;

		// 항등 행렬로 초기화 (원점에 위치, 회전/스케일 없음)
		VirtualRoot.BindPose = FMatrix::Identity();
		VirtualRoot.InverseBindPose = FMatrix::Identity();

		// 가상 루트를 배열 맨 앞에 삽입
		MeshData.Skeleton.Bones.Insert(VirtualRoot, 0);

		// 기존 본들의 인덱스가 모두 +1 씩 밀림
		// 모든 본의 ParentIndex 업데이트
		for (int32 i = 1; i < MeshData.Skeleton.Bones.size(); ++i)
		{
			if (MeshData.Skeleton.Bones[i].ParentIndex >= 0)
			{
				MeshData.Skeleton.Bones[i].ParentIndex += 1;
			}
			else // 원래 루트 본들
			{
				MeshData.Skeleton.Bones[i].ParentIndex = 0; // 가상 루트를 부모로 설정
			}
		}

		// Vertex의 BoneIndex도 모두 +1 해줘야 함
		for (auto& Vertex : MeshData.Vertices)
		{
			for (int32 i = 0; i < 4; ++i)
			{
				Vertex.BoneIndices[i] += 1;
			}
		}

		UE_LOG("UFbxLoader: Created virtual root bone. Found %d root bones.", RootBoneIndices.Num());
	}
}

FString UFbxLoader::SanitizeFileName(const FString& FileName)
{
	FString Sanitized = FileName;

	// Windows 파일 시스템에서 사용할 수 없는 문자들을 언더스코어로 대체
	// < > : " / \ | ? *
	const char InvalidChars[] = {'<', '>', ':', '"', '/', '\\', '|', '?', '*'};

	for (char InvalidChar : InvalidChars)
	{
		std::replace(Sanitized.begin(), Sanitized.end(), InvalidChar, '_');
	}

	return Sanitized;
}

TArray<FString> UFbxLoader::GetAnimationStackNames(const FString& FilePath)
{
	TArray<FString> AnimStackNames;

	// 1. 파일 경로 정규화
	FString NormalizedPath = NormalizePath(FilePath);

	// 2. FbxImporter 생성 및 초기화
	FbxImporter* Importer = FbxImporter::Create(SdkManager, "");
	if (!Importer->Initialize(NormalizedPath.c_str(), -1, SdkManager->GetIOSettings()))
	{
		UE_LOG("UFbxLoader::GetAnimationStackNames: Failed to initialize FbxImporter for '%s'", NormalizedPath.c_str());
		Importer->Destroy();
		return AnimStackNames; // 빈 배열 반환
	}

	// 3. FbxScene 생성 및 Import
	FbxScene* Scene = FbxScene::Create(SdkManager, "TempScene");
	if (!Importer->Import(Scene))
	{
		UE_LOG("UFbxLoader::GetAnimationStackNames: Failed to import scene from '%s'", NormalizedPath.c_str());
		Importer->Destroy();
		Scene->Destroy();
		return AnimStackNames; // 빈 배열 반환
	}
	Importer->Destroy();

	// 4. AnimStack 개수 확인
	int AnimStackCount = Scene->GetSrcObjectCount<FbxAnimStack>();
	if (AnimStackCount == 0)
	{
		UE_LOG("UFbxLoader::GetAnimationStackNames: No animation stacks found in '%s'", NormalizedPath.c_str());
		Scene->Destroy();
		return AnimStackNames; // 빈 배열 반환
	}

	// 5. 각 AnimStack의 이름을 배열에 추가
	for (int i = 0; i < AnimStackCount; ++i)
	{
		FbxAnimStack* AnimStack = Scene->GetSrcObject<FbxAnimStack>(i);
		if (AnimStack)
		{
			FbxString AnimStackName = AnimStack->GetName();
			AnimStackNames.Add(FString(AnimStackName.Buffer()));
			UE_LOG("UFbxLoader::GetAnimationStackNames: Found AnimStack[%d]: '%s'", i, AnimStackName.Buffer());
		}
	}

	// 6. Scene 정리
	Scene->Destroy();

	UE_LOG("UFbxLoader::GetAnimationStackNames: Total %d animation stacks found in '%s'", AnimStackNames.Num(), NormalizedPath.c_str());
	return AnimStackNames;
}

UAnimSequence* UFbxLoader::LoadFbxAnimation(const FString& FilePath, const struct FSkeleton* TargetSkeleton, const FString& AnimStackName)
{
	// 1. 파일 경로 정규화
	FString NormalizedPath = NormalizePath(FilePath);
	UE_LOG("UFbxLoader::LoadFbxAnimation: Loading animation from '%s' (AnimStack: '%s')",
		NormalizedPath.c_str(), AnimStackName.empty() ? "default" : AnimStackName.c_str());

	// 2. 리소스 키 생성: {파일명}_{AnimStack명} 또는 {파일명} (AnimStackName이 비어있을 경우)
	FString ResourceKey = NormalizedPath;
	FString SanitizedAnimStackName;
	if (!AnimStackName.empty())
	{
		// AnimStack 이름에서 파일 시스템에 안전하지 않은 문자 제거
		SanitizedAnimStackName = SanitizeFileName(AnimStackName);

		// 파일명에서 확장자 제거
		size_t LastDot = NormalizedPath.find_last_of('.');
		FString BaseName = (LastDot != FString::npos) ? NormalizedPath.substr(0, LastDot) : NormalizedPath;
		ResourceKey = BaseName + "_" + SanitizedAnimStackName;

		UE_LOG("UFbxLoader::LoadFbxAnimation: Sanitized AnimStack name '%s' -> '%s'",
			AnimStackName.c_str(), SanitizedAnimStackName.c_str());
	}

	// 3. 리소스 매니저 캐시 확인
	UAnimSequence* CachedAnim = UResourceManager::GetInstance().Get<UAnimSequence>(ResourceKey);
	if (CachedAnim)
	{
		UE_LOG("UFbxLoader::LoadFbxAnimation: Animation already cached (key: '%s'), returning existing resource", ResourceKey.c_str());
		return CachedAnim;
	}

	// 3. 스켈레톤 유효성 검사
	if (!TargetSkeleton || TargetSkeleton->Bones.IsEmpty())
	{
		UE_LOG("UFbxLoader::LoadFbxAnimation: Invalid target skeleton");
		return nullptr;
	}

	// 4. 바이너리 캐시 파일 처리
	UAnimDataModel* DataModel = nullptr;
#ifdef USE_OBJ_CACHE
	// 4-1. 캐시 파일 경로 설정 (AnimStack 이름 포함)
	FString CachePathStr = ConvertDataPathToCachePath(NormalizedPath);
	FString AnimCacheFileName = CachePathStr;
	if (!AnimStackName.empty())
	{
		// 확장자 제거 후 Sanitized AnimStack 이름 추가
		size_t LastDot = AnimCacheFileName.find_last_of('.');
		if (LastDot != FString::npos)
		{
			AnimCacheFileName = AnimCacheFileName.substr(0, LastDot);
		}
		AnimCacheFileName += "_" + SanitizedAnimStackName;
	}
	AnimCacheFileName += ".anim.bin";

	// 캐시를 저장할 디렉토리가 없으면 생성
	std::filesystem::path CacheFileDirPath(UTF8ToWide(AnimCacheFileName));
	if (CacheFileDirPath.has_parent_path())
	{
		std::filesystem::create_directories(CacheFileDirPath.parent_path());
	}

	bool bLoadedFromCache = false;

	// 4-2. 캐시 유효성 검사
	bool bShouldRegenerate = true;
	if (std::filesystem::exists(UTF8ToWide(AnimCacheFileName)))
	{
		try
		{
			auto binTime = std::filesystem::last_write_time(UTF8ToWide(AnimCacheFileName));
			auto fbxTime = std::filesystem::last_write_time(UTF8ToWide(NormalizedPath));

			// FBX 파일이 캐시보다 오래되었으면 캐시 사용
			if (fbxTime <= binTime)
			{
				bShouldRegenerate = false;
			}
		}
		catch (const std::filesystem::filesystem_error& e)
		{
			UE_LOG("UFbxLoader::LoadFbxAnimation: Filesystem error during cache validation: %s. Forcing regeneration.", e.what());
			bShouldRegenerate = true;
		}
	}

	// 4-3. 캐시에서 로드 시도
	if (!bShouldRegenerate)
	{
		UE_LOG("UFbxLoader::LoadFbxAnimation: Attempting to load animation from cache '%s'", AnimCacheFileName.c_str());
		try
		{
			DataModel = NewObject<UAnimDataModel>();

			FWindowsBinReader Reader(AnimCacheFileName);
			if (!Reader.IsOpen())
			{
				throw std::runtime_error("Failed to open animation cache file for reading.");
			}
			Reader << *DataModel;
			Reader.Close();

			// 캐시 로드 성공
			bLoadedFromCache = true;
			UE_LOG("UFbxLoader::LoadFbxAnimation: Successfully loaded animation from cache (%.3f sec, %d bones, %d keys)",
				DataModel->SequenceLength, DataModel->BoneAnimationTracks.Num(), DataModel->NumberOfKeys);

			// UAnimSequence 생성 및 설정
			UAnimSequence* AnimSequence = NewObject<UAnimSequence>();
			AnimSequence->SetFilePath(NormalizedPath);
			AnimSequence->SetSkeletonName(TargetSkeleton->Name);
			AnimSequence->SetAnimDataModel(DataModel);

			// Compute and store skeleton signature for strict compatibility checking
			uint64 Signature = ComputeSkeletonSignature(*TargetSkeleton);
			AnimSequence->SetSkeletonSignature(Signature);
			AnimSequence->SetSkeletonBoneCount(static_cast<int32>(TargetSkeleton->Bones.size()));

			UE_LOG("UFbxLoader::LoadFbxAnimation: Skeleton '%s' signature = 0x%016llX (%d bones)",
				TargetSkeleton->Name.c_str(), Signature, static_cast<int32>(TargetSkeleton->Bones.size()));

			// 리소스 매니저에 등록 (ResourceKey 사용)
			UResourceManager::GetInstance().Add<UAnimSequence>(ResourceKey, AnimSequence);

			return AnimSequence;
		}
		catch (const std::exception& e)
		{
			UE_LOG("UFbxLoader::LoadFbxAnimation: Error loading animation from cache: %s. Cache might be corrupt or incompatible.", e.what());
			UE_LOG("UFbxLoader::LoadFbxAnimation: Deleting corrupt cache and forcing regeneration.");

			std::filesystem::remove(UTF8ToWide(AnimCacheFileName));

			// 캐시 로드 실패 시 생성된 DataModel 정리
			if (DataModel)
			{
				ObjectFactory::DeleteObject(DataModel);
				DataModel = nullptr;
			}
			bLoadedFromCache = false;
		}
	}

	// 4-4. 캐시 로드 실패 시 FBX 파싱
	if (!bLoadedFromCache)
	{
		UE_LOG("UFbxLoader::LoadFbxAnimation: Regenerating animation cache from FBX...");
	}
#endif // USE_OBJ_CACHE

	// 5. FbxImporter 생성 및 초기화
	FbxImporter* Importer = FbxImporter::Create(SdkManager, "");
	if (!Importer->Initialize(NormalizedPath.c_str(), -1, SdkManager->GetIOSettings()))
	{
		UE_LOG("UFbxLoader::LoadFbxAnimation: Failed to initialize FbxImporter");
		UE_LOG("Error: %s", Importer->GetStatus().GetErrorString());
		return nullptr;
	}

	// 5. FbxScene 생성 및 Import
	FbxScene* Scene = FbxScene::Create(SdkManager, "Animation Scene");
	Importer->Import(Scene);
	Importer->Destroy();

	FbxAxisSystem UnrealImportAxis(FbxAxisSystem::eZAxis, FbxAxisSystem::eParityEven, FbxAxisSystem::eLeftHanded);
	FbxAxisSystem SourceSetup = Scene->GetGlobalSettings().GetAxisSystem();

	FbxSystemUnit::m.ConvertScene(Scene);

	if (SourceSetup != UnrealImportAxis)
	{
		UE_LOG("Fbx 축 변환 완료!\n");
		UnrealImportAxis.DeepConvertScene(Scene);
	}

	// 5-1. Blender Armature Transform 추출
	// 첫 번째 비스켈레톤 노드를 찾아서 보정용 트랜스폼 정보들을 추출
	FbxAMatrix ArmatureTransform;
	ArmatureTransform.SetIdentity();
	FbxNode* RootNode = Scene->GetRootNode();

	if (RootNode)
	{
		// 루트부터 재귀적으로 탐색
		FbxNode* NonSkeletonParentNode = FindNonSkeletonParentRecursive(RootNode);

		if (NonSkeletonParentNode)
		{
			// 비스켈레톤 부모 노드를 찾아서 트랜스폼 추출
			ArmatureTransform = NonSkeletonParentNode->EvaluateLocalTransform();

			FbxVector4 ArmatureScale = ArmatureTransform.GetS();
			FString NodeName = NonSkeletonParentNode->GetName();
			UE_LOG("UFbxLoader::LoadFbxAnimation: Found Armature '%s' with Scale: (%.2f, %.2f, %.2f)",
				NodeName.c_str(), ArmatureScale[0], ArmatureScale[1], ArmatureScale[2]);
		}
		else
		{
			UE_LOG("UFbxLoader::LoadFbxAnimation: No non-skeleton container found, skeleton directly under root (Mixamo-style FBX)");
		}
	}

	// 6. AnimStack 가져오기 (이름으로 찾거나 첫 번째 스택 사용)
	int AnimStackCount = Scene->GetSrcObjectCount<FbxAnimStack>();
	if (AnimStackCount == 0)
	{
		UE_LOG("UFbxLoader::LoadFbxAnimation: No animation stacks found in FBX file");
		Scene->Destroy();
		return nullptr;
	}

	FbxAnimStack* AnimStack = nullptr;
	FbxString FbxAnimStackName;

	// AnimStackName이 지정되었으면 해당 이름의 스택 찾기
	if (!AnimStackName.empty())
	{
		UE_LOG("UFbxLoader::LoadFbxAnimation: Searching for AnimStack '%s' among %d stacks", AnimStackName.c_str(), AnimStackCount);

		for (int i = 0; i < AnimStackCount; ++i)
		{
			FbxAnimStack* TempStack = Scene->GetSrcObject<FbxAnimStack>(i);
			if (TempStack)
			{
				FbxString TempStackName = TempStack->GetName();
				UE_LOG("UFbxLoader::LoadFbxAnimation: Checking stack[%d]: '%s'", i, TempStackName.Buffer());

				// FbxString 비교: Buffer()로 변환하여 strcmp 사용
				if (strcmp(TempStackName.Buffer(), AnimStackName.c_str()) == 0)
				{
					AnimStack = TempStack;
					FbxAnimStackName = TempStackName;
					UE_LOG("UFbxLoader::LoadFbxAnimation: Found matching AnimStack at index %d", i);
					break;
				}
			}
		}

		if (!AnimStack)
		{
			UE_LOG("UFbxLoader::LoadFbxAnimation: ERROR - AnimStack '%s' not found in FBX file", AnimStackName.c_str());
			UE_LOG("UFbxLoader::LoadFbxAnimation: Available stacks:");
			for (int i = 0; i < AnimStackCount; ++i)
			{
				FbxAnimStack* TempStack = Scene->GetSrcObject<FbxAnimStack>(i);
				if (TempStack)
				{
					UE_LOG("  [%d] '%s'", i, TempStack->GetName());
				}
			}
			Scene->Destroy();
			return nullptr;
		}
	}
	else
	{
		// AnimStackName이 비어있으면 첫 번째 스택 사용
		AnimStack = Scene->GetSrcObject<FbxAnimStack>(0);
		FbxAnimStackName = AnimStack->GetName();
	}

	UE_LOG("UFbxLoader::LoadFbxAnimation: Using animation stack '%s'", FbxAnimStackName.Buffer());

	// 7. 애니메이션 시간 범위 가져오기
	FbxTakeInfo* TakeInfo = Scene->GetTakeInfo(FbxAnimStackName);
	FbxTimeSpan TimeSpan;
	if (TakeInfo)
	{
		TimeSpan = TakeInfo->mLocalTimeSpan;
	}
	else
	{
		TimeSpan = AnimStack->GetLocalTimeSpan();
	}

	FbxTime StartTime = TimeSpan.GetStart();
	FbxTime EndTime = TimeSpan.GetStop();
	FbxTime Duration = EndTime - StartTime;

	// FbxTime을 초(second) 단위로 변환
	float SequenceLength = static_cast<float>(Duration.GetSecondDouble());

	// FBX의 프레임레이트 가져오기
	FbxTime::EMode TimeMode = Scene->GetGlobalSettings().GetTimeMode();
	float FrameRate = static_cast<float>(FbxTime::GetFrameRate(TimeMode));

	UE_LOG("UFbxLoader::LoadFbxAnimation: Duration = %.3f seconds, FrameRate = %.1f fps", SequenceLength, FrameRate);

	// 8. UAnimDataModel 생성 (캐시 로드 실패 시만 생성)
	if (!DataModel)
	{
		DataModel = NewObject<UAnimDataModel>();
	}
	DataModel->SequenceLength = SequenceLength;
	DataModel->FrameRate = FrameRate;
	DataModel->NumberOfFrames = static_cast<int32>(SequenceLength * FrameRate);

	// 9. AnimStack 활성화
	Scene->SetCurrentAnimationStack(AnimStack);

	// 10. 첫 번째 AnimLayer 가져오기 (일반적으로 레이어는 하나만 사용)
	int LayerCount = AnimStack->GetMemberCount<FbxAnimLayer>();
	UE_LOG("UFbxLoader::LoadFbxAnimation: Found %d animation layer(s)", LayerCount);

	FbxAnimLayer* AnimLayer = AnimStack->GetMember<FbxAnimLayer>(0);
	if (!AnimLayer)
	{
		UE_LOG("UFbxLoader::LoadFbxAnimation: No animation layer found");
		Scene->Destroy();
		return nullptr;
	}
	UE_LOG("UFbxLoader::LoadFbxAnimation: Using animation layer '%s'", AnimLayer->GetName());

	// 11. FBX 씬의 루트 노드 재확인
	RootNode = Scene->GetRootNode();
	if (!RootNode)
	{
		UE_LOG("UFbxLoader::LoadFbxAnimation: No root node found");
		Scene->Destroy();
		return nullptr;
	}

	// 12. 본 이름으로 FbxNode 찾기 위한 맵 생성
	TMap<FString, FbxNode*> BoneNameToNode;
	std::function<void(FbxNode*)> CollectBoneNodes = [&](FbxNode* Node)
	{
		if (Node->GetNodeAttribute() && Node->GetNodeAttribute()->GetAttributeType() == FbxNodeAttribute::eSkeleton)
		{
			FString BoneName = Node->GetName();
			BoneNameToNode[BoneName] = Node;
		}

		for (int i = 0; i < Node->GetChildCount(); ++i)
		{
			CollectBoneNodes(Node->GetChild(i));
		}
	};
	CollectBoneNodes(RootNode);

	UE_LOG("UFbxLoader::LoadFbxAnimation: Collected %d bone nodes from FBX", BoneNameToNode.size());

	// 13. CurveData 초기화 (본 개수만큼 Transform 커브 배열 생성)
	DataModel->CurveData.BoneTransformCurves.SetNum(TargetSkeleton->Bones.Num());

	// 14. 애니메이션 데이터 추출
	// 수정: Offset 문제 방지를 위해 항상 Evaluation-Based 방식 사용
	// EvaluateLocalTransform으로 절대 로컬 변환을 가져와서 커브 유무와 관계없이 모든 본 처리
	UE_LOG("UFbxLoader::LoadFbxAnimation: Using EVALUATION-BASED extraction (EvaluateLocalTransform for all bones)");

	// 15. 각 본에 대해 애니메이션 데이터 추출
	int32 TotalKeys = 0;
	for (int32 BoneIndex = 0; BoneIndex < TargetSkeleton->Bones.Num(); ++BoneIndex)
	{
		const FBone& Bone = TargetSkeleton->Bones[BoneIndex];

		// 스켈레톤의 본 이름으로 FbxNode 찾기
		if (!BoneNameToNode.Contains(Bone.Name))
		{
			UE_LOG("UFbxLoader::LoadFbxAnimation: Bone '%s' not found in FBX", Bone.Name.c_str());
			continue;
		}

		FbxNode* BoneNode = BoneNameToNode[Bone.Name];
		bool bIsRootBone = (Bone.ParentIndex == -1);

		// FTransformAnimCurve 참조 가져오기
		FTransformAnimCurve& TransformCurve = DataModel->CurveData.BoneTransformCurves[BoneIndex];

		// EvaluateLocalTransform으로 절대 로컬 변환 가져오기
		{
			// 프레임 간격 계산
			FbxTime FrameTime;
			FrameTime.SetSecondDouble(1.0 / static_cast<double>(FrameRate));

			// 부모 노드 가져오기
			FbxNode* ParentNode = BoneNode->GetParent();

			// 각 프레임에서 Transform을 평가하여 키프레임 생성
			int32 FrameCount = DataModel->NumberOfFrames;
			for (int32 FrameIndex = 0; FrameIndex <= FrameCount; ++FrameIndex)
			{
				// 현재 프레임의 시간 계산
				FbxTime CurrentTime = StartTime + (FrameTime * FrameIndex);
				float Time = static_cast<float>(CurrentTime.GetSecondDouble() - StartTime.GetSecondDouble());

				// 노드의 로컬 Transform을 평가 (부모의 영향을 받지 않는 순수 로컬 Transform)
				FbxAMatrix LocalTransform = BoneNode->EvaluateLocalTransform(CurrentTime);

				// 루트 본에 Armature Transform 적용
				// Armature가 없으면 항등 행렬이므로 영향 없음
				if (bIsRootBone)
				{
					LocalTransform = ArmatureTransform * LocalTransform;
				}

				// Translation 추출
				FbxVector4 FbxTranslation = LocalTransform.GetT();
				FVector Position(
					static_cast<float>(FbxTranslation[0]),
					static_cast<float>(FbxTranslation[1]),
					static_cast<float>(FbxTranslation[2])
				);

				// Rotation 추출 (Quaternion으로)
				FbxQuaternion FbxRotation = LocalTransform.GetQ();
				FQuat Rotation(
					static_cast<float>(FbxRotation[0]),
					static_cast<float>(FbxRotation[1]),
					static_cast<float>(FbxRotation[2]),
					static_cast<float>(FbxRotation[3])
				);

				// Scale 추출
				FbxVector4 FbxScale = LocalTransform.GetS();
				FVector Scale(
					static_cast<float>(FbxScale[0]),
					static_cast<float>(FbxScale[1]),
					static_cast<float>(FbxScale[2])
				);

				// 키프레임 추가
				TransformCurve.PositionCurve.AddKey(Time, Position);
				TransformCurve.RotationCurve.AddKey(Time, Rotation);
				TransformCurve.ScaleCurve.AddKey(Time, Scale);
			}

			TotalKeys += (FrameCount + 1) * 3; // Position, Rotation, Scale 각각
		}

		// BoneAnimationTrack 생성 및 추가 (CurveData를 FRawAnimSequenceTrack으로 변환)
		FBoneAnimationTrack Track;
		Track.BoneIndex = BoneIndex;
		Track.BoneName = Bone.Name;

		// CurveData에서 추출한 키프레임을 InternalTrack으로 복사
		for (int32 i = 0; i < TransformCurve.PositionCurve.Times.Num(); ++i)
		{
			Track.InternalTrack.PositionKeys.Add(TransformCurve.PositionCurve.Values[i]);
		}

		for (int32 i = 0; i < TransformCurve.RotationCurve.Times.Num(); ++i)
		{
			Track.InternalTrack.RotationKeys.Add(TransformCurve.RotationCurve.Values[i]);
		}

		for (int32 i = 0; i < TransformCurve.ScaleCurve.Times.Num(); ++i)
		{
			Track.InternalTrack.ScaleKeys.Add(TransformCurve.ScaleCurve.Values[i]);
		}

		// BoneAnimationTracks에 추가
		DataModel->BoneAnimationTracks.Add(Track);
	}

	DataModel->NumberOfKeys = TotalKeys;

	// 16. UAnimSequence 생성 및 설정
	UAnimSequence* AnimSequence = NewObject<UAnimSequence>();
	AnimSequence->SetFilePath(NormalizedPath);
	AnimSequence->SetSkeletonName(TargetSkeleton->Name);
	AnimSequence->SetAnimDataModel(DataModel);

	// Compute and store skeleton signature for strict compatibility checking
	uint64 Signature = ComputeSkeletonSignature(*TargetSkeleton);
	AnimSequence->SetSkeletonSignature(Signature);
	AnimSequence->SetSkeletonBoneCount(static_cast<int32>(TargetSkeleton->Bones.size()));

	// 17. 씬 정리
	Scene->Destroy();

	UE_LOG("UFbxLoader::LoadFbxAnimation: Successfully loaded animation (%.3f sec, %d bones, %d total keys)",
		SequenceLength, TargetSkeleton->Bones.Num(), TotalKeys);

	// 18. 리소스 매니저에 등록 (ResourceKey 사용)
	bool bRegistered = UResourceManager::GetInstance().Add<UAnimSequence>(ResourceKey, AnimSequence);
	if (bRegistered)
	{
		UE_LOG("UFbxLoader::LoadFbxAnimation: Animation registered in ResourceManager (key: '%s')", ResourceKey.c_str());
	}
	else
	{
		UE_LOG("UFbxLoader::LoadFbxAnimation: Warning - Animation was already registered (key: '%s')", ResourceKey.c_str());
	}

#ifdef USE_OBJ_CACHE
	// 19. 바이너리 캐시 저장 (FBX 파싱으로 생성한 경우만)
	if (!bLoadedFromCache)
	{
		try
		{
			UE_LOG("UFbxLoader::LoadFbxAnimation: Saving animation to cache '%s'", AnimCacheFileName.c_str());

			FWindowsBinWriter Writer(AnimCacheFileName);
			Writer << *DataModel;
			Writer.Close();

			UE_LOG("UFbxLoader::LoadFbxAnimation: Successfully saved animation cache");
		}
		catch (const std::exception& e)
		{
			UE_LOG("UFbxLoader::LoadFbxAnimation: Error saving animation cache: %s", e.what());
			// 캐시 저장 실패는 치명적이지 않으므로 계속 진행
		}
	}
#endif // USE_OBJ_CACHE

	if (TargetSkeleton)
	{
		AnimSequence->SetSkeletonName(TargetSkeleton->Name);

		// Compute and store skeleton signature for strict compatibility checking
		uint64 Signature = ComputeSkeletonSignature(*TargetSkeleton);
		AnimSequence->SetSkeletonSignature(Signature);
		AnimSequence->SetSkeletonBoneCount(static_cast<int32>(TargetSkeleton->Bones.size()));
	}

	return AnimSequence;
}

