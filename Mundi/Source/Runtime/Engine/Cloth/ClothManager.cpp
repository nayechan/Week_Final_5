#include "pch.h"
#include "ClothManager.h"
#include "ClothMesh.h"

UClothManager* UClothManager::Instance = nullptr;

UClothManager::UClothManager()
{
    Instance = this;
}
UClothManager::~UClothManager()
{
    Release();
}

void UClothManager::InitClothManager(ID3D11Device* InDevice, ID3D11DeviceContext* InContext)
{
    Device = InDevice;
    Context = InContext;
    //GraphicsContextManager = new DxContextManagerCallbackImpl(InDevice, InContext);
    // 1. 콜백 객체 생성
    if (!Allocator)
    {
        Allocator = new NvClothAllocator();
    }

    if (!ErrorCallback)
    {
        ErrorCallback = new NvClothErrorCallback();
    }

    if (!AssertHandler)
    {
        AssertHandler = new NvClothAssertHandler();
    }

    // 2. NvCloth 초기화
    nv::cloth::InitializeNvCloth(
        Allocator,
        ErrorCallback,
        AssertHandler,
        nullptr  // profiler (optional)
    );
    //Factory = NvClothCreateFactoryDX11(GraphicsContextManager);
    Factory = NvClothCreateFactoryCPU();
    Solver = Factory->createSolver();

    TestCloth = new FClothMesh();

    for (int z= 0; z<= 30; z++)
    {
        for (int x = 0; x <= 30; x++)
        {
            FVertexDynamic Vert;
            Vert.Position = FVector(x / 3.0f, 0, -z / 3.0f);
            Vert.Normal = FVector(0, 1, 0);
            Vert.UV = FVector2D(x / 30.0f, z/ 30.0f);
            TestCloth->OriginVertices.push_back(Vert);
            float InvMass = (x == 0 || x == 15 || x == 30) && z == 0 ? 0 : 1.0f;
            
            TestCloth->OriginParticles.push_back(physx::PxVec4(Vert.Position.X, Vert.Position.Y, Vert.Position.Z, InvMass));
        }
    }

    for (int y = 0; y < 30; y++)
    {
        for (int x = 0; x < 30; x++)
        {
            int CurIdx = x + y * 31;
            TestCloth->OriginIndices.push_back(CurIdx);
            TestCloth->OriginIndices.push_back(CurIdx + 32);
            TestCloth->OriginIndices.push_back(CurIdx + 31);
            TestCloth->OriginIndices.push_back(CurIdx);
            TestCloth->OriginIndices.push_back(CurIdx + 1);
            TestCloth->OriginIndices.push_back(CurIdx + 32);
        }
    }

    nv::cloth::ClothMeshDesc meshDesc;
    //Fill meshDesc with data
    meshDesc.setToDefault();
    meshDesc.points.data = TestCloth->OriginVertices.data();
    meshDesc.points.stride = sizeof(FVertexDynamic);
    meshDesc.points.count = TestCloth->OriginVertices.size();

    meshDesc.triangles.data = TestCloth->OriginIndices.data();
    meshDesc.triangles.stride = sizeof(uint32) * 3;
    meshDesc.triangles.count = TestCloth->OriginIndices.size() / 3;

    // InvMasses 설정 (중요!)
    meshDesc.invMasses.data = TestCloth->OriginParticles.data();
    meshDesc.invMasses.stride = sizeof(physx::PxVec4);
    meshDesc.invMasses.count = TestCloth->OriginParticles.size();
    //etc. for quads, triangles and invMasses

    physx::PxVec3 gravity(0.0f, 0.0f, -9.8f);
    Fabric* Fabric = NvClothCookFabricFromMesh(UClothManager::Instance->GetFactory(), meshDesc, gravity, &TestCloth->PhaseTypes);
    TestCloth->OriginCloth = UClothManager::Instance->GetFactory()->createCloth(GetRange(TestCloth->OriginParticles), *Fabric);

    // 2. Stretch 제한 (Tether Constraints)
    TestCloth->OriginCloth->setTetherConstraintStiffness(0.5f); // 덜 강하게 원위치로 복귀
    TestCloth->OriginCloth->setTetherConstraintScale(1.5f);

    // ===== Stiffness 설정 =====
    // PhaseConfig 직접 생성
    int numPhases = Fabric->getNumPhases();
    std::vector<nv::cloth::PhaseConfig> phaseConfigs(numPhases);

    for (int i = 0; i < numPhases; i++)
    {
        phaseConfigs[i].mPhaseIndex = i;
        phaseConfigs[i].mStiffness = 0.1f;           // 0.0 ~ 1.0
        phaseConfigs[i].mStiffnessMultiplier = 1.0f;
        phaseConfigs[i].mCompressionLimit = 1.0f;
        phaseConfigs[i].mStretchLimit = 5.0f;        // 늘어남 제한
    }

    nv::cloth::Range<nv::cloth::PhaseConfig> range(phaseConfigs.data(), phaseConfigs.data() + numPhases);
    TestCloth->OriginCloth->setPhaseConfig(range);


    // Cloth 속성 설정 (자연스러운 움직임)
    TestCloth->OriginCloth->setGravity(gravity);
    TestCloth->OriginCloth->setDamping(physx::PxVec3(0.3f, 0.3f, 0.3f));  // 감쇠
    TestCloth->OriginCloth->setFriction(0.5f);  // 마찰
    TestCloth->OriginCloth->setLinearDrag(physx::PxVec3(0.2f, 0.2f, 0.2f));  // 공기 저항

    Fabric->decRefCount();
}

void UClothManager::Release()
{
    delete TestCloth;

    // Solver 정리
    if (Solver)
    {
        NV_CLOTH_DELETE(Solver);
        Solver = nullptr;
    }

    // Factory 정리
    if (Factory)
    {
        NvClothDestroyFactory(Factory);
        Factory = nullptr;
    }

    // 콜백 정리
    delete Allocator;
    delete ErrorCallback;
    delete AssertHandler;

    Allocator = nullptr;
    ErrorCallback = nullptr;
    AssertHandler = nullptr;
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