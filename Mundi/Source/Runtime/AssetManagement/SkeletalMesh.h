#pragma once
#include "ResourceBase.h"
#include "USkeletalMesh.generated.h"

class UPhysicsAsset;

UCLASS()
class USkeletalMesh : public UResourceBase
{
public:
    GENERATED_REFLECTION_BODY()

    USkeletalMesh();
    virtual ~USkeletalMesh() override;
    
    void Load(const FString& InFilePath, ID3D11Device* InDevice);
    
    const FSkeletalMeshData* GetSkeletalMeshData() const { return Data; }
    const FString& GetPathFileName() const { if (Data) return Data->PathFileName; return FString(); }
    const FSkeleton* GetSkeleton() const { return Data ? &Data->Skeleton : nullptr; }
    uint32 GetBoneCount() const { return Data ? Data->Skeleton.Bones.Num() : 0; }
    
    // ID3D11Buffer* GetVertexBuffer() const { return VertexBuffer; } // W10 CPU Skinning이라 Component가 VB 소유
    ID3D11Buffer* GetIndexBuffer() const { return IndexBuffer; }

    uint32 GetVertexCount() const { return VertexCount; }
    uint32 GetIndexCount() const { return IndexCount; }

    uint32 GetVertexStride() const { return VertexStride; }
    
    const TArray<FGroupInfo>& GetMeshGroupInfo() const { static TArray<FGroupInfo> EmptyGroup; return Data ? Data->GroupInfos : EmptyGroup; }
    bool HasMaterial() const { return Data ? Data->bHasMaterial : false; }

    uint64 GetMeshGroupCount() const { return Data ? Data->GroupInfos.size() : 0; }

    // --- 물리 에셋 ---
    UPhysicsAsset* GetPhysicsAsset() const { return PhysicsAsset; }
    void SetPhysicsAsset(UPhysicsAsset* InPhysicsAsset) { PhysicsAsset = InPhysicsAsset; }

    void CreateVertexBuffer(ID3D11Buffer** InVertexBuffer);
    void UpdateVertexBuffer(const TArray<FNormalVertex>& SkinnedVertices, ID3D11Buffer* InVertexBuffer);

    // GPU 스키닝용 버텍스 버퍼 생성 (FSkinnedVertex 그대로 사용)
    void CreateGPUSkinnedVertexBuffer(ID3D11Buffer** InVertexBuffer);

    // 물리 에셋 (랙돌용 본별 충돌 Shape + Constraint 정의)
    UPROPERTY(EditAnywhere, Category = "Physics")
    UPhysicsAsset* PhysicsAsset = nullptr;
    
private:
    void CreateIndexBuffer(FSkeletalMeshData* InSkeletalMesh, ID3D11Device* InDevice);
    void ReleaseResources();
    
private:
    // GPU 리소스
    // ID3D11Buffer* VertexBuffer = nullptr; // W10 CPU Skinning이라 Component가 VB 소유
    ID3D11Buffer* IndexBuffer = nullptr;
    uint32 VertexCount = 0;     // 정점 개수
    uint32 IndexCount = 0;     // 버텍스 점의 개수 
    uint32 VertexStride = 0;
    
    // CPU 리소스
    FSkeletalMeshData* Data = nullptr;
};