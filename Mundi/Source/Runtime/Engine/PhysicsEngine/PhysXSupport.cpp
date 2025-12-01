#include "pch.h"
#include "PhysXSupport.h"
#include "PhysicalMaterial.h"

#include <thread>

// ==================================================================================
// GLOBAL POINTERS
// ==================================================================================

PxFoundation*            GPhysXFoundation = nullptr;
PxPhysics*               GPhysXSDK = nullptr;
PxCooking*               GPhysXCooking = nullptr;
PxDefaultCpuDispatcher*  GPhysXDispatcher = nullptr;
UPhysicalMaterial*       GPhysicalMaterial = nullptr;
PxPvd*                   GPhysXVisualDebugger = nullptr;
PxPvdTransport*          GPhysXPvdTransport = nullptr;
FPhysXAllocator*         GPhysXAllocator= nullptr;
FPhysXErrorCallback*     GPhysXErrorCallback = nullptr;

bool InitGamePhys()
{
    if (GPhysXFoundation)
    {
        return true;
    }

    GPhysXAllocator = new FPhysXAllocator();
    GPhysXErrorCallback = new FPhysXErrorCallback();

    GPhysXFoundation = PxCreateFoundation(PX_PHYSICS_VERSION, *GPhysXAllocator, *GPhysXErrorCallback);
    if (!GPhysXFoundation)
    {
        UE_LOG("[PhysX Error] PxCreateFoundation() 실행이 실패했습니다.");
        return false;
    }

    if (!GPhysicalMaterial)
    {
        GPhysicalMaterial = NewObject<UPhysicalMaterial>();
    }

    GPhysXVisualDebugger = PxCreatePvd(*GPhysXFoundation);

    // @note 언리얼 엔진은 cm 단위를 활용하지만, 퓨처 엔진은 m 단위를 활용한다.
    PxTolerancesScale PScale;
    PScale.length = 1.0f; // 기준 길이 (1m가 엔진에서 표현되는 값)
    PScale.speed = 10.0f; // 기준 속도 (물체의 평균적인 움직임 속도)

    GPhysXSDK = PxCreatePhysics(PX_PHYSICS_VERSION, *GPhysXFoundation, PScale, true, GPhysXVisualDebugger);
    if (!GPhysXSDK)
    {
        UE_LOG("[PhysX Error] PxCreatePhysics() 실행이 실패했습니다.");
        return false;
    }

    if (!PxInitExtensions(*GPhysXSDK, GPhysXVisualDebugger))
    {
        UE_LOG("[PhysX Error] PxInitExtensions() 실행이 실패했습니다.");
        return false;
    }

    if (GPhysXVisualDebugger)
    {
        GPhysXPvdTransport = PxDefaultPvdSocketTransportCreate("127.0.0.1", 5425, 10);
        if (GPhysXPvdTransport)
        {
            GPhysXVisualDebugger->connect(*GPhysXPvdTransport, PxPvdInstrumentationFlag::eALL);
        }
    }

    PxCookingParams CookingParams(PScale);
    // @note 메시를 쿠킹할 때 가까이 붙은 정점들을 하나로 합치는(Weld) 거리 임계값이다.
    //       1mm (=0.001m)를 임계값으로 사용한다.
    CookingParams.meshWeldTolerance = 0.001f;
    CookingParams.meshPreprocessParams = PxMeshPreprocessingFlag::eWELD_VERTICES;

    GPhysXCooking = PxCreateCooking(PX_PHYSICS_VERSION, *GPhysXFoundation, CookingParams);
    if (!GPhysXCooking)
    {
        UE_LOG("[PhysX Error] PxCreateCooking() 실행이 실패했습니다.");
        return false;
    }
    
    unsigned int LogicalCores = std::thread::hardware_concurrency();

    // @todo 현재는 논리 코어의 수만큼 워커 스레드수를 설정한다. 이후, 추가적인 실험을 통해서 적절한 값을 찾아야 한다.
    GPhysXDispatcher = PxDefaultCpuDispatcherCreate(std::max(1u, LogicalCores));
    if (!GPhysXDispatcher)
    {
        UE_LOG("[PhysX Error] PxDefaultCpuDispatcherCreate() 실행시 실패했습니다.");
        return false;
    }

    UE_LOG("[PhysX] PhysX 코어 초기화가 완료되었습니다.");
    return true;
}

void TermGamePhys()
{
    if (GPhysXDispatcher)     { GPhysXDispatcher->release(); GPhysXDispatcher = nullptr; }
    if (GPhysXCooking)        { GPhysXCooking->release(); GPhysXCooking = nullptr; }
    if (GPhysXSDK)            { PxCloseExtensions(); }
    if (GPhysXSDK)            { GPhysXSDK->release(); GPhysXSDK = nullptr; }
    if (GPhysicalMaterial)    { DeleteObject(GPhysicalMaterial); GPhysicalMaterial = nullptr; }

    // PVD 연결 해제 후 transport 해제 (순서 중요)
    if (GPhysXVisualDebugger)
    {
        if (GPhysXVisualDebugger->isConnected())
        {
            GPhysXVisualDebugger->disconnect();
        }
        GPhysXVisualDebugger->release();
        GPhysXVisualDebugger = nullptr;
    }
    if (GPhysXPvdTransport)   { GPhysXPvdTransport->release(); GPhysXPvdTransport = nullptr; }

    if (GPhysXFoundation)     { GPhysXFoundation->release(); GPhysXFoundation = nullptr; }

    delete GPhysXAllocator; GPhysXAllocator = nullptr;
    delete GPhysXErrorCallback; GPhysXErrorCallback = nullptr;

    UE_LOG("[PhysX] PhysX가 종료되었습니다.");
}
