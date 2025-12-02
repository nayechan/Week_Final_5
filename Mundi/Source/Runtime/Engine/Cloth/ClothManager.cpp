#include "pch.h"
#include "ClothManager.h"

IMPLEMENT_CLASS(UClothManager)

Range<physx::PxVec4> GetRange(TArray<PxVec4>& Arr)
{
    return Range<physx::PxVec4>(
        Arr.data(),
        Arr.data() + Arr.size());
}



void UClothManager::Init(ID3D11Device* InDevice)
{
    //DX11 컨텍스트 설정 
    GraphicsContextManager = new DxContextManagerCallbackImpl(InDevice);
    Factory = NvClothCreateFactoryDX11(GraphicsContextManager);
    Solver = Factory->createSolver();

    // 정점 생성
    ClothWidth = 10;
    ClothHeight = 10;
    for (int y = 0; y < ClothWidth; y++) {
        for (int x = 0; x < ClothHeight; x++) {
            float posX = x * 0.1f;
            float posY = 0.0f;
            float posZ = y * 0.1f;
            float invMass = 1.0f; // 역질량
            Particles.push_back(physx::PxVec4(posX, posY, posZ, invMass));
        }
    }

    // 인덱스 생성
    for (int y = 0; y < ClothHeight - 1; y++) {
        for (int x = 0; x < ClothWidth - 1; x++) {
            int topLeft = y * ClothWidth + x;
            int topRight = topLeft + 1;
            int bottomLeft = (y + 1) * ClothWidth + x;
            int bottomRight = bottomLeft + 1;

            Indices.push_back(topLeft);
            Indices.push_back(bottomLeft);
            Indices.push_back(topRight);

            Indices.push_back(topRight);
            Indices.push_back(bottomLeft);
            Indices.push_back(bottomRight);
        }
    }


    // ClothMeshDesc 설정
    nv::cloth::ClothMeshDesc meshDesc;

    // 정점 데이터
    meshDesc.points.data = Particles.data();
    meshDesc.points.count = static_cast<uint32_t>(Particles.size());
    meshDesc.points.stride = sizeof(physx::PxVec4);

    // 삼각형 인덱스
    meshDesc.triangles.data = Indices.data();
    meshDesc.triangles.count = static_cast<uint32_t>(Indices.size() / 3);
    meshDesc.triangles.stride = sizeof(uint32_t) * 3;

    // invMasses는 particles의 w 값 사용하므로 nullptr
    meshDesc.invMasses.data = nullptr;
    meshDesc.invMasses.count = 0;
    meshDesc.invMasses.stride = 0;

    // Fabric 생성
    Fabric = NvClothCookFabricFromMesh(
        Factory,
        meshDesc,
        physx::PxVec3(0, -1, 0),  // 중력 방향
        nullptr,                   // phase config (nullptr = 자동)
        false                      // use geodesic tether
    );

    // Cloth 생성
    Cloth = Factory->createCloth(GetRange(Particles), *Fabric);
    Solver->addCloth(Cloth);

    // 물리 속성 설정
    Cloth->setGravity(physx::PxVec3(0.0f, -9.81f, 0.0f));
    Cloth->setDamping(physx::PxVec3(0.2f, 0.2f, 0.2f));
    Cloth->setLinearInertia(physx::PxVec3(0.8f, 0.8f, 0.8f));
    Cloth->setFriction(0.5f);


    nv::cloth::MappedRange<physx::PxVec4> particles = Cloth->getCurrentParticles();
    for (int i = 0; i < particles.size(); i++)
    {
        //do something with particles[i]
        //the xyz components are the current positions
        //the w component is the invMass.
    }

}

void UClothManager::Tick(float deltaTime)
{
    Solver->beginSimulation(deltaTime);
    for (int i = 0; i < Solver->getSimulationChunkCount(); i++)
    {
        Solver->simulateChunk(i);
    }
    Solver->endSimulation();
}