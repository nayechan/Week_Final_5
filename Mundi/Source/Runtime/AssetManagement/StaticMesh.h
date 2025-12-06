#pragma once
#include "ResourceBase.h"
#include "MeshBVH.h"
#include <d3d11.h>

class UStaticMeshComponent;
class FMeshBVH;
class UBodySetup;
class UStaticMesh : public UResourceBase
{
public:
    DECLARE_CLASS(UStaticMesh, UResourceBase)

    UStaticMesh() = default;
    virtual ~UStaticMesh() override;

    void Load(const FString& InFilePath, ID3D11Device* InDevice, EVertexLayoutType InVertexType = EVertexLayoutType::PositionColorTexturNormal);
    void Load(FMeshData* InData, ID3D11Device* InDevice, EVertexLayoutType InVertexType = EVertexLayoutType::PositionColorTexturNormal);

    ID3D11Buffer* GetVertexBuffer() const { return VertexBuffer; }
    ID3D11Buffer* GetIndexBuffer() const { return IndexBuffer; }
    uint32 GetVertexCount() const { return VertexCount; }
    uint32 GetIndexCount() const { return IndexCount; }
    void SetVertexType(EVertexLayoutType InVertexLayoutType);
    EVertexLayoutType GetVertexType() const { return VertexType; }
    void SetIndexCount(uint32 Cnt) { IndexCount = Cnt; }
    uint32 GetVertexStride() const { return VertexStride; };

    // 외부에서 직접 버퍼 설정 (스카이박스 등 절차적 메시용)
    void SetBuffers(ID3D11Buffer* InVertexBuffer, ID3D11Buffer* InIndexBuffer,
                    uint32 InVertexCount, uint32 InIndexCount, uint32 InVertexStride)
    {
        VertexBuffer = InVertexBuffer;
        IndexBuffer = InIndexBuffer;
        VertexCount = InVertexCount;
        IndexCount = InIndexCount;
        VertexStride = InVertexStride;
    }

	const FString& GetAssetPathFileName() const { return StaticMeshAsset ? StaticMeshAsset->PathFileName : FilePath; }
    void SetStaticMeshAsset(FStaticMesh* InStaticMesh) { StaticMeshAsset = InStaticMesh; }
	FStaticMesh* GetStaticMeshAsset() const { return StaticMeshAsset; }

    const TArray<FGroupInfo>& GetMeshGroupInfo() const { return StaticMeshAsset->GroupInfos; }
    bool HasMaterial() const { return StaticMeshAsset->bHasMaterial; }

    uint64 GetMeshGroupCount() const { return StaticMeshAsset->GroupInfos.size(); }
    
    FAABB GetLocalBound() const {return LocalBound; }

    const FString& GetCacheFilePath() const { return CacheFilePath; }

    // ====================================================================
    // Physics Body Setup
    // ====================================================================

    /** Physics collision setup을 반환합니다 */
    UBodySetup* GetBodySetup();

    /** BodySetup이 없으면 생성합니다 */
    void CreateBodySetupIfNeeded();

    /** BodySetup을 설정합니다 */
    void SetBodySetup(UBodySetup* InBodySetup);

    /** Convex CookedData 캐시 파일 경로 (DerivedDataCache/xxx.physics.bin) */
    FString GetPhysicsCachePath() const;

    /** Physics 메타데이터 파일 경로 (원본 옆에 xxx.physics.json) */
    FString GetPhysicsMetadataPath() const;

    /** Convex CookedData 캐시 로드 */
    bool LoadPhysicsCache();

    /** Convex CookedData 캐시 저장 */
    bool SavePhysicsCache();

    /** Physics 메타데이터 로드 (Sphere/Box/Capsule/Convex 정보) */
    bool LoadPhysicsMetadata();

    /** Physics 메타데이터 저장 (Sphere/Box/Capsule/Convex 정보) */
    bool SavePhysicsMetadata();

    /** BodySetup이 비어있으면 메시 정점으로부터 기본 Convex 생성 */
    void CreateDefaultConvexIfNeeded();

    /** BodySetup이 비어있으면 메시로부터 기본 TriangleMesh 생성 */
    void CreateDefaultTriangleMeshIfNeeded();

    /** CollisionComplexity에 따라 충돌체 재생성 (수동 편집 존중) */
    void RegenerateCollision();

    /** Physics를 기본값(캐시의 Convex)으로 리셋 */
    void ResetPhysicsToDefault();

private:
    void CreateVertexBuffer(FMeshData* InMeshData, ID3D11Device* InDevice, EVertexLayoutType InVertexType);
	void CreateVertexBuffer(FStaticMesh* InStaticMesh, ID3D11Device* InDevice, EVertexLayoutType InVertexType);
    void CreateIndexBuffer(FMeshData* InMeshData, ID3D11Device* InDevice);
	void CreateIndexBuffer(FStaticMesh* InStaticMesh, ID3D11Device* InDevice);
    void CreateLocalBound(const FMeshData* InMeshData);
    void CreateLocalBound(const FStaticMesh* InStaticMesh);
    void ReleaseResources();

    FString CacheFilePath;  // 캐시된 소스 경로 (예: DerivedDataCache/cube.obj.bin)

    // GPU 리소스
    ID3D11Buffer* VertexBuffer = nullptr;
    ID3D11Buffer* IndexBuffer = nullptr;
    uint32 VertexCount = 0;     // 정점 개수
    uint32 IndexCount = 0;     // 버텍스 점의 개수 
    uint32 VertexStride = 0;
    EVertexLayoutType VertexType = EVertexLayoutType::PositionColorTexturNormal;  // Stride를 계산하기 위한 버텍스 타입

	// CPU 리소스
    FStaticMesh* StaticMeshAsset = nullptr;

    // 메시 단위 BVH (ResourceManager에서 캐싱, 소유)
    // 초기화되지 않는 멤버변수 (참조도 ResourceManager에서만 이루어짐) 
    // FMeshBVH* MeshBVH = nullptr;

    // 로컬 AABB. (스태틱메시 액터 전체 경계 계산에 사용. StaticMeshAsset 로드할 때마다 갱신)
    FAABB LocalBound;

    // Physics body setup (충돌 형상 정의)
    UBodySetup* BodySetup = nullptr;
};

