#pragma once
#include "Object.h"
#include "ClothUtil.h"
#include "DxContextManagerCallbackImpl.h"
#include "NvClothEnvironment.h"


struct FClothMesh;

class UClothManager
{
public:
    UClothManager();
    ~UClothManager();
    static UClothManager* Instance;
	void InitClothManager(ID3D11Device* InDevice, ID3D11DeviceContext* InContext);
	void Tick(float deltaTime);
    void Release();
    
    Factory* GetFactory() { return Factory; }
    Solver* GetSolver() { return Solver; }

    //임시
    ID3D11Device* GetDevice() { return Device; }
    ID3D11DeviceContext* GetContext() { return Context; }

    FClothMesh* GetTestCloth() { return TestCloth; }

    void RegisterCloth(Cloth* InCloth) { Solver->addCloth(InCloth); }
    void UnRegisterCloth(Cloth* InCloth) { Solver->removeCloth(InCloth); }

private:

    FClothMesh* TestCloth = nullptr;

    DxContextManagerCallback* GraphicsContextManager = nullptr;
    Factory* Factory = nullptr;
    Solver* Solver = nullptr;

    ID3D11Device* Device = nullptr;
    ID3D11DeviceContext* Context = nullptr;

    // NvCloth 초기화에 필요한 콜백들
    NvClothAllocator* Allocator = nullptr;
    NvClothErrorCallback* ErrorCallback = nullptr;
    NvClothAssertHandler* AssertHandler = nullptr;

}; 
inline Range<physx::PxVec4> GetRange(TArray<PxVec4>& Arr)
{
    return Range<physx::PxVec4>(
        Arr.data(),
        Arr.data() + Arr.size());
}