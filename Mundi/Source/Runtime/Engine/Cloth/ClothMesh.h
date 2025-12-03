#pragma once
#include "ClothUtil.h"
#include "objbase.h"
#include "RHIDevice.h"
#include "VertexData.h"

//Cloth 원본
struct FClothMesh
{
public:
    FClothMesh() = default;
    ~FClothMesh();
public:
    Cloth* OriginCloth = nullptr;
    Vector<int32_t>::Type PhaseTypes;
    TArray<physx::PxVec4> OriginParticles;
    TArray<uint32_t> OriginIndices;
    TArray<FVertexDynamic> OriginVertices;

    uint32 Stride = 0;
    uint32 VertexCount = 0;
    uint32 IndexCount = 0;
};


class UClothMeshInstance : public UObject //ResourceBase가 되어야함 일단 기능이없기에 UObject로만 사용
{
    DECLARE_CLASS(UClothMeshInstance, UObject)
public:
    UClothMeshInstance() = default;
    ~UClothMeshInstance() override;
    UClothMeshInstance* GetDuplicated();
    void Init(FClothMesh* InClothMesh);
    void Sync();
    FClothMesh* GetOriginCloth() { return ClothMesh; }
public:
    Cloth* Cloth = nullptr;
    TArray<physx::PxVec4> Particles;
    TArray<uint32_t> Indices;
    TArray<FVertexDynamic> Vertices;

    ID3D11Buffer* VertexBuffer = nullptr;
    ID3D11Buffer* IndexBuffer = nullptr;
private:
    FClothMesh* ClothMesh = nullptr;
};