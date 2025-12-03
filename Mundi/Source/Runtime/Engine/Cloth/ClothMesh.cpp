#include "pch.h"
#include "ClothMesh.h"
#include "D3D11RHI.h"
#include "ClothManager.h"

IMPLEMENT_CLASS(UClothMeshInstance)

FClothMesh::~FClothMesh()
{
    NV_CLOTH_DELETE(OriginCloth);
}


UClothMeshInstance::~UClothMeshInstance()
{
    VertexBuffer->Release();
    IndexBuffer->Release();
    NV_CLOTH_DELETE(Cloth);
}


void UClothMeshInstance::Init(FClothMesh* InClothMesh)
{
    ClothMesh = InClothMesh;
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
    
    //공유Fabric을 쓰는 Cloth 생성 시 레프카운트 증가하지않음
    //Cloth 삭제 시 레프카운트가 감소하므로 decRefCount 하지말것
    //OriginFabric->decRefCount();
}

UClothMeshInstance* UClothMeshInstance::GetDuplicated()
{
    //Fabric 은 원본 복사 (사실 원본쓰던 OtherInstance쓰던 같음)
    UClothMeshInstance* DuplicatedInstance = NewObject<UClothMeshInstance>();
    nv::cloth::Cloth* OriginCloth = ClothMesh->OriginCloth;
    Fabric& OriginFabric = OriginCloth->getFabric();

    //OtherInstance에서 Particle정보 변할 수 있으니 OtherInstance복사
    Range<const physx::PxVec4> OriginParticles = Cloth->getCurrentParticles();
    std::vector<physx::PxVec4> CopiedParticles(OriginParticles.begin(), OriginParticles.end());

    // 3. 새로운 Cloth 생성
    DuplicatedInstance->Cloth = UClothManager::Instance->GetFactory()->createCloth(
        nv::cloth::Range<physx::PxVec4>(CopiedParticles.data(), CopiedParticles.data() + CopiedParticles.size()),
        OriginFabric
    );

    ClothUtil::CopySettings(OriginCloth, DuplicatedInstance->Cloth);
    DuplicatedInstance->Particles = Particles;
    DuplicatedInstance->Indices = Indices; //얘는 원본써도될걸?
    DuplicatedInstance->Vertices = Vertices;

    ID3D11Device* Device = UClothManager::Instance->GetDevice();
    D3D11RHI::CreateVerteBuffer(Device, DuplicatedInstance->Vertices, &DuplicatedInstance->VertexBuffer, D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE);
    D3D11RHI::CreateIndexBuffer(Device, DuplicatedInstance->Indices, &DuplicatedInstance->IndexBuffer);

    //공유Fabric을 쓰는 Cloth 생성 시 레프카운트 증가하지않음
    //Cloth 삭제 시 레프카운트가 감소하므로 decRefCount 하지말것
    //OriginFabric.decRefCount();
    return DuplicatedInstance;
}



void UClothMeshInstance::Sync()
{
    //물리 계산된 파티클 정보를 실제 Vertices 랑 VertexBuffer 에 넣음
    MappedRange<physx::PxVec4> Particles = Cloth->getCurrentParticles();
    for (int i = 0; i < Particles.size(); i++)
    {
        PxVec3 PxPos = Particles[i].getXYZ();
        FVector Pos = FVector(PxPos.x, PxPos.y, PxPos.z);
        Vertices[i].Position = Pos;
        //do something with particles[i]
        //the xyz components are the current positions
        //the w component is the invMass.
    }
    ID3D11DeviceContext* Context = UClothManager::Instance->GetContext();

    D3D11RHI::VertexBufferUpdate(Context, VertexBuffer, Vertices);
    //destructor of particles should be called before mCloth is destroyed.
}