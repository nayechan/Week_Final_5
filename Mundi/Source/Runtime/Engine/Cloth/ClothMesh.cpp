#include "pch.h"
#include "ClothMesh.h"
#include "D3D11RHI.h"
#include "ClothManager.h"

IMPLEMENT_CLASS(UClothMeshInstance)

FClothMesh::~FClothMesh()
{
    NV_CLOTH_DELETE(OriginCloth);
}


void UClothMeshInstance::Init(FClothMesh* InClothMesh)
{
    Fabric* OriginFabric = &InClothMesh->OriginCloth->getFabric();

    // 2. 원본 Cloth의 Particle 데이터 복사
    Range<const physx::PxVec4> OriginParticles = InClothMesh->OriginCloth->getCurrentParticles();
    std::vector<physx::PxVec4> CopiedParticles(OriginParticles.begin(), OriginParticles.end());

    // 3. 새로운 Cloth 생성
   Cloth = UClothManager::Instance->GetFactory()->createCloth(
        nv::cloth::Range<physx::PxVec4>(CopiedParticles.data(), CopiedParticles.data() + CopiedParticles.size()),
        *OriginFabric
    );

    ClothUtil::CopySettings(InClothMesh->OriginCloth, Cloth);

    Particles = InClothMesh->OriginParticles;
    Indices = InClothMesh->OriginIndices; //얘는 원본써도될걸?
    Vertices = InClothMesh->OriginVertices;

    ID3D11Device* Device = UClothManager::Instance->GetDevice();
    D3D11RHI::CreateVerteBuffer(Device, Vertices, &VertexBuffer, D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE);
    D3D11RHI::CreateIndexBuffer(Device, Indices, &IndexBuffer);

    OriginFabric->decRefCount();
    UClothManager::Instance->GetSolver()->addCloth(Cloth);
}

UClothMeshInstance::~UClothMeshInstance()
{
    UClothManager::Instance->GetSolver()->removeCloth(Cloth);
    VertexBuffer->Release();
    IndexBuffer->Release();
    NV_CLOTH_DELETE(Cloth);
}