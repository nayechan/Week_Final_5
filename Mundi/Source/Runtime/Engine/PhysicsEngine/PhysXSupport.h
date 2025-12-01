#pragma once

#include "PhysXPublic.h"
#include "PxPhysicsAPI.h"

class UPhysicalMaterial;
/**
 * @note 현재는 PhysX에서 제공하는 디폴트 할당자를 사용한다.
 * @note PhysX의 메모리 할당자 래퍼
 * @todo 별도의 메모리 할당 로직이 필요할 경우 'allocate()'를 오버라이딩해서 사용한다.
 */
class FPhysXAllocator : public PxDefaultAllocator
{
};

/**
 * PhysX 에러를 Future Engine의 로그 시스템으로 가져온다.
 */
class FPhysXErrorCallback : public PxErrorCallback
{
public:
    virtual void reportError(PxErrorCode::Enum code, const char* message, const char* file, int line) override
    {
        std::string ErrorCodeStr;
        switch (code)
        {
        case PxErrorCode::eNO_ERROR:          ErrorCodeStr = "Info"; break;
        case PxErrorCode::eDEBUG_INFO:        ErrorCodeStr = "Debug"; break;
        case PxErrorCode::eDEBUG_WARNING:     ErrorCodeStr = "Warning"; break;
        case PxErrorCode::eINVALID_PARAMETER: ErrorCodeStr = "Invalid Param"; break;
        case PxErrorCode::eINVALID_OPERATION: ErrorCodeStr = "Invalid Op"; break;
        case PxErrorCode::eOUT_OF_MEMORY:     ErrorCodeStr = "Out of Memory"; break;
        case PxErrorCode::eINTERNAL_ERROR:    ErrorCodeStr = "Internal Error"; break;
        default:                              ErrorCodeStr = "Unknown"; break;
        }

        UE_LOG("[PhysX Error] [%s] %s (%s:%d)\n", ErrorCodeStr.c_str(), message, file, line);
    }
};

// ==================================================================================
// GLOBAL POINTERS
// ==================================================================================

/** PhysX Foundation 싱글톤에 대한 포인터 */
extern PxFoundation*            GPhysXFoundation;//PhysX의 기반 시스템 (싱글톤)

extern PxPhysics*               GPhysXSDK;//PhysX 물리 엔진 인스턴스

extern PxCooking*               GPhysXCooking;//메시를 물리용으로 "쿠킹" (변환)하는 객체

extern PxDefaultCpuDispatcher*  GPhysXDispatcher;//CPU 멀티스레드 작업 분배기

extern UPhysicalMaterial*       GPhysicalMaterial; // 디폴트 물리 머티리얼

/** PhysX 디버거에 대한 포인터 */
extern PxPvd*                   GPhysXVisualDebugger;// PVD (PhysX Visual Debugger) 연결용

extern FPhysXAllocator*         GPhysXAllocator;// 메모리 할당자

extern FPhysXErrorCallback*     GPhysXErrorCallback;//에러 콜백

bool InitGamePhys();
void TermGamePhys();