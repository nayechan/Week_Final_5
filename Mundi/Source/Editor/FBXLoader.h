#pragma once
#include "Object.h"
#include "fbxsdk.h"

class UFbxLoader : public UObject
{
public:

	DECLARE_CLASS(UFbxLoader, UObject)
	static UFbxLoader& GetInstance();
	UFbxLoader();

	static void PreLoad();

	USkeletalMesh* LoadFbxMesh(const FString& FilePath);

	FSkeletalMeshData* LoadFbxMeshAsset(const FString& FilePath);

	/**
	 * FBX 파일에 포함된 모든 애니메이션 스택(AnimStack)의 이름을 가져옴
	 * @param FilePath FBX 파일 경로
	 * @return 애니메이션 스택 이름 배열 (애니메이션이 없으면 빈 배열)
	 */
	TArray<FString> GetAnimationStackNames(const FString& FilePath);

	/**
	 * FBX 파일에 메시가 포함되어 있는지 확인
	 * @param FilePath FBX 파일 경로
	 * @return 메시가 있으면 true, 없으면 false
	 */
	bool HasMeshInFbx(const FString& FilePath);

	/**
	 * FBX 파일에서 스켈레톤만 추출 (메시 없는 애니메이션 전용 FBX용)
	 * @param FilePath FBX 파일 경로
	 * @return 추출된 스켈레톤 (실패 시 nullptr)
	 */
	FSkeleton* LoadSkeletonOnly(const FString& FilePath);

	/**
	 * 이미 로드된 스켈레톤 중에서 호환되는 스켈레톤 찾기
	 * @param SourceSkeleton 매칭할 소스 스켈레톤
	 * @return 호환되는 스켈레톤 (없으면 nullptr)
	 */
	const FSkeleton* FindCompatibleSkeleton(const FSkeleton* SourceSkeleton);

	/**
	 * FBX 파일에서 애니메이션 로드
	 * @param FilePath FBX 파일 경로
	 * @param TargetSkeleton 대상 스켈레톤 (본 인덱스 매칭용)
	 * @param AnimStackName 로드할 애니메이션 스택 이름 (빈 문자열이면 첫 번째 스택 사용)
	 * @return 로드된 애니메이션 시퀀스
	 */
	class UAnimSequence* LoadFbxAnimation(const FString& FilePath, const struct FSkeleton* TargetSkeleton, const FString& AnimStackName = "");

protected:
	~UFbxLoader() override;
private:
	UFbxLoader(const UFbxLoader&) = delete;
	UFbxLoader& operator=(const UFbxLoader&) = delete;


	void LoadMeshFromNode(FbxNode* InNode, FSkeletalMeshData& MeshData, TMap<int32, TArray<uint32>>& MaterialGroupIndexList, TMap<FbxNode*, int32>& BoneToIndex, TMap<FbxSurfaceMaterial*, int32>& MaterialToIndex);

	void LoadSkeletonFromNode(FbxNode* InNode, FSkeletalMeshData& MeshData, int32 ParentNodeIndex, TMap<FbxNode*, int32>& BoneToIndex);

	void LoadMeshFromAttribute(FbxNodeAttribute* InAttribute, FSkeletalMeshData& MeshData);

	void LoadMesh(FbxMesh* InMesh, FSkeletalMeshData& MeshData, TMap<int32, TArray<uint32>>& MaterialGroupIndexList, TMap<FbxNode*, int32>& BoneToIndex, TArray<int32> MaterialSlotToIndex, int32 DefaultMaterialIndex = 0);

	void ParseMaterial(FbxSurfaceMaterial* Material, FMaterialInfo& MaterialInfo);

	FString ParseTexturePath(FbxProperty& Property);

	FbxString GetAttributeTypeName(FbxNodeAttribute* InAttribute);

	void EnsureSingleRootBone(FSkeletalMeshData& MeshData);

	/**
	 * FBX Scene에 메시가 있는지 확인
	 */
	bool HasMeshInScene(FbxNode* Node);

	/**
	 * 스켈레톤만 추출하는 헬퍼 (Scene에서 직접 추출)
	 */
	void LoadSkeletonFromScene(FbxNode* Node, FSkeleton& OutSkeleton, int32 ParentIndex, TMap<FbxNode*, int32>& BoneToIndex);

	/**
	 * 파일 시스템에 안전한 이름으로 변환 (특수문자 제거)
	 * @param FileName 원본 파일명
	 * @return Windows 파일 시스템에 사용 가능한 파일명
	 */
	static FString SanitizeFileName(const FString& FileName);
	
	// bin파일 저장용
	TArray<FMaterialInfo> MaterialInfos;
	FbxManager* SdkManager = nullptr;

};